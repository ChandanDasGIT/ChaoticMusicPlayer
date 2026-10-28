#include "backend/youtube/linklibrary/linklibrary.h"

#include "backend/storage/tsv/tsv.h"

#include <algorithm>
#include <fstream>
#include <system_error>

namespace fs = std::filesystem;

namespace {

constexpr const char *Header = "ChaoticMusicPlayer saved links v1";
constexpr size_t FieldCount = 6;
constexpr char ArtistSeparator = '\x1f';

std::string joinArtists(const std::vector<std::string> &artists)
{
    std::string out;
    for (const std::string &artist : artists)
        out += (out.empty() ? "" : std::string(1, ArtistSeparator)) + artist;
    return out;
}

std::vector<std::string> splitArtists(const std::string &joined)
{
    std::vector<std::string> out;
    size_t start = 0;
    while (!joined.empty()) {
        const size_t sep = joined.find(ArtistSeparator, start);
        out.push_back(joined.substr(start, sep - start));
        if (sep == std::string::npos)
            break;
        start = sep + 1;
    }
    return out;
}

} // namespace

LinkLibrary::LinkLibrary(fs::path file)
    : m_file(std::move(file))
{
}

void LinkLibrary::load()
{
    m_songs.clear();
    std::ifstream in(m_file);
    std::string line;
    if (!in || !std::getline(in, line) || line != Header)
        return;

    while (std::getline(in, line)) {
        const std::vector<std::string> f = tsv::split(line);
        if (f.size() != FieldCount)
            continue;
        YtSong song;
        song.videoId = tsv::unescape(f[0]);
        song.title = tsv::unescape(f[1]);
        song.artists = splitArtists(tsv::unescape(f[2]));
        song.album = tsv::unescape(f[3]);
        song.durationMs = tsv::toNumber<int64_t>(f[4]);
        song.thumbnailUrl = tsv::unescape(f[5]);
        if (!song.videoId.empty())
            m_songs.push_back(std::move(song));
    }
}

bool LinkLibrary::save() const
{
    std::error_code ec;
    fs::create_directories(m_file.parent_path(), ec);

    const fs::path temp = m_file.string() + ".tmp";
    {
        std::ofstream out(temp, std::ios::trunc);
        if (!out)
            return false;
        out << Header << '\n';
        for (const YtSong &s : m_songs) {
            out << tsv::escape(s.videoId) << '\t' << tsv::escape(s.title) << '\t'
                << tsv::escape(joinArtists(s.artists)) << '\t' << tsv::escape(s.album) << '\t'
                << s.durationMs << '\t' << tsv::escape(s.thumbnailUrl) << '\n';
        }
        if (!out)
            return false;
    }
    fs::rename(temp, m_file, ec);
    return !ec;
}

bool LinkLibrary::add(const YtSong &song)
{
    const bool exists = std::any_of(m_songs.begin(), m_songs.end(),
                                    [&](const YtSong &s) { return s.videoId == song.videoId; });
    if (exists || song.videoId.empty())
        return false;
    m_songs.insert(m_songs.begin(), song);
    return true;
}

void LinkLibrary::remove(size_t index)
{
    if (index < m_songs.size())
        m_songs.erase(m_songs.begin() + std::ptrdiff_t(index));
}
