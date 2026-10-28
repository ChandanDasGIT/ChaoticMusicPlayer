#include "backend/youtube/ytmetadatastore/ytmetadatastore.h"

#include "backend/storage/tsv/tsv.h"

#include <chrono>
#include <fstream>
#include <system_error>

namespace fs = std::filesystem;

namespace {

constexpr const char *Header = "ChaoticMusicPlayer ytmeta v2";
constexpr const char *HeaderV1 = "ChaoticMusicPlayer ytmeta v1"; // before close matches
constexpr size_t FieldCount = 14;
constexpr size_t FieldCountV1 = 13;
constexpr int64_t NoMatchRetrySeconds = 30LL * 24 * 3600;
constexpr char ArtistSeparator = '\x1f'; // unit separator: never part of a name

int64_t nowSeconds()
{
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

bool fileStamp(const std::string &path, int64_t &size, int64_t &time)
{
    std::error_code ec;
    const auto bytes = fs::file_size(path, ec);
    if (ec)
        return false;
    const auto written = fs::last_write_time(path, ec);
    if (ec)
        return false;
    size = int64_t(bytes);
    time = int64_t(written.time_since_epoch().count());
    return true;
}

std::string joinArtists(const std::vector<std::string> &artists)
{
    std::string out;
    for (const std::string &artist : artists) {
        if (!out.empty())
            out += ArtistSeparator;
        out += artist;
    }
    return out;
}

std::vector<std::string> splitArtists(const std::string &joined)
{
    std::vector<std::string> out;
    size_t start = 0;
    while (start <= joined.size() && !joined.empty()) {
        const size_t sep = joined.find(ArtistSeparator, start);
        out.push_back(joined.substr(start, sep - start));
        if (sep == std::string::npos)
            break;
        start = sep + 1;
    }
    return out;
}

} // namespace

YtMetadataStore::YtMetadataStore(fs::path file)
    : m_file(std::move(file))
{
}

void YtMetadataStore::load()
{
    m_entries.clear();
    std::ifstream in(m_file);
    std::string line;
    if (!in || !std::getline(in, line) || (line != Header && line != HeaderV1))
        return;
    const bool v1 = line == HeaderV1;

    while (std::getline(in, line)) {
        const std::vector<std::string> f = tsv::split(line);
        if (f.size() != (v1 ? FieldCountV1 : FieldCount))
            continue;
        YtMetadataEntry entry;
        entry.matched = f[1] == "1";
        if (v1 && !entry.matched)
            continue; // looked up before close matching existed: look up again
        entry.close = !v1 && f[13] == "1";
        entry.fileSize = tsv::toNumber<int64_t>(f[2]);
        entry.fileTime = tsv::toNumber<int64_t>(f[3]);
        entry.checkedAt = tsv::toNumber<int64_t>(f[4]);
        entry.song.videoId = tsv::unescape(f[5]);
        entry.song.title = tsv::unescape(f[6]);
        entry.song.artists = splitArtists(tsv::unescape(f[7]));
        entry.song.album = tsv::unescape(f[8]);
        entry.song.albumId = tsv::unescape(f[9]);
        entry.song.year = tsv::unescape(f[10]);
        entry.song.durationMs = tsv::toNumber<int64_t>(f[11]);
        entry.song.thumbnailUrl = tsv::unescape(f[12]);
        m_entries[tsv::unescape(f[0])] = std::move(entry);
    }
}

bool YtMetadataStore::save() const
{
    std::error_code ec;
    fs::create_directories(m_file.parent_path(), ec);

    const fs::path temp = m_file.string() + ".tmp";
    {
        std::ofstream out(temp, std::ios::trunc);
        if (!out)
            return false;
        out << Header << '\n';
        for (const auto &[path, e] : m_entries) {
            out << tsv::escape(path) << '\t' << (e.matched ? 1 : 0) << '\t' << e.fileSize << '\t'
                << e.fileTime << '\t' << e.checkedAt << '\t' << tsv::escape(e.song.videoId) << '\t'
                << tsv::escape(e.song.title) << '\t' << tsv::escape(joinArtists(e.song.artists)) << '\t'
                << tsv::escape(e.song.album) << '\t' << tsv::escape(e.song.albumId) << '\t'
                << tsv::escape(e.song.year) << '\t' << e.song.durationMs << '\t'
                << tsv::escape(e.song.thumbnailUrl) << '\t' << (e.close ? 1 : 0) << '\n';
        }
        if (!out)
            return false;
    }
    fs::rename(temp, m_file, ec);
    return !ec;
}

const YtMetadataEntry *YtMetadataStore::find(const std::string &path) const
{
    const auto it = m_entries.find(path);
    if (it == m_entries.end())
        return nullptr;

    int64_t size = 0, time = 0;
    if (!fileStamp(path, size, time) || size != it->second.fileSize || time != it->second.fileTime)
        return nullptr; // the file changed since the lookup
    if (!it->second.matched && nowSeconds() - it->second.checkedAt > NoMatchRetrySeconds)
        return nullptr; // try again: YouTube Music may have it by now
    return &it->second;
}

void YtMetadataStore::put(const std::string &path, YtMetadataEntry entry)
{
    fileStamp(path, entry.fileSize, entry.fileTime);
    entry.checkedAt = nowSeconds();
    m_entries[path] = std::move(entry);
}
