#include "backend/playlist/playliststore/playliststore.h"

#include "backend/storage/tsv/tsv.h"

#include <fstream>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

namespace {

constexpr const char *Header = "ChaoticMusicPlayer playlist v2";
constexpr const char *HeaderV1 = "ChaoticMusicPlayer playlist v1"; // before YouTube tracks
constexpr size_t FieldCount = 11;
constexpr size_t FieldCountV1 = 9;

} // namespace

bool PlaylistStore::save(const fs::path &file, const std::vector<Track> &tracks)
{
    std::error_code ec;
    fs::create_directories(file.parent_path(), ec);

    // Write to a temp file, then rename: a crash never leaves a half-written playlist.
    const fs::path temp = file.string() + ".tmp";
    {
        std::ofstream out(temp, std::ios::trunc);
        if (!out)
            return false;
        out << Header << '\n';
        for (const Track &t : tracks) {
            out << tsv::escape(t.path) << '\t' << tsv::escape(t.title) << '\t' << tsv::escape(t.artist) << '\t'
                << tsv::escape(t.album) << '\t' << t.disc << '\t' << t.discTotal << '\t' << t.track << '\t'
                << t.durationMs << '\t' << (t.hasCover ? 1 : 0) << '\t' << tsv::escape(t.videoId) << '\t'
                << tsv::escape(t.thumbnailUrl) << '\n';
        }
        if (!out)
            return false;
    }
    fs::rename(temp, file, ec);
    return !ec;
}

std::vector<Track> PlaylistStore::load(const fs::path &file)
{
    std::vector<Track> tracks;
    std::ifstream in(file);
    std::string line;
    if (!in || !std::getline(in, line) || (line != Header && line != HeaderV1))
        return tracks;
    const size_t fieldCount = line == Header ? FieldCount : FieldCountV1;

    while (std::getline(in, line)) {
        const std::vector<std::string> f = tsv::split(line);
        if (f.size() != fieldCount)
            continue;

        Track t;
        t.path = tsv::unescape(f[0]);
        if (fieldCount == FieldCount) {
            t.videoId = tsv::unescape(f[9]);
            t.thumbnailUrl = tsv::unescape(f[10]);
        }
        std::error_code ec;
        if (t.videoId.empty() && !fs::is_regular_file(t.path, ec))
            continue; // local file that is gone
        t.title = tsv::unescape(f[1]);
        t.artist = tsv::unescape(f[2]);
        t.album = tsv::unescape(f[3]);
        t.disc = tsv::toNumber<int>(f[4]);
        t.discTotal = tsv::toNumber<int>(f[5]);
        t.track = tsv::toNumber<int>(f[6]);
        t.durationMs = tsv::toNumber<int64_t>(f[7]);
        t.hasCover = f[8] == "1";
        tracks.push_back(std::move(t));
    }
    return tracks;
}
