#pragma once

#include "backend/library/track/track.h"

#include <filesystem>
#include <string>
#include <vector>

// M3U / M3U8 playlists, as other players (VLC, foobar2000, ...) read and write them.
//   Reading: local paths (absolute, or relative to the playlist file), file://
//            addresses, and YouTube / YouTube Music links (titled from
//            #EXTINF). Other web addresses (radio streams) are skipped; so are
//            files that no longer exist. UTF-8, with a Latin-1 fallback.
//   Writing: UTF-8 with #EXTINF lines. Songs in the playlist file's folder (or
//            below it) are written relative to it, so the folder can be moved
//            as a whole; others with their full path. YouTube songs as links.
// Blocking (reads every file's tags): call from a worker thread.
class M3u
{
public:
    struct Result
    {
        bool ok = false;               // the file could be read
        std::vector<Track> tracks;
        int missing = 0;               // local files that don't exist (any more)
        int skipped = 0;               // web addresses that aren't YouTube
    };

    static Result read(const std::filesystem::path &file);
    static bool write(const std::filesystem::path &file, const std::vector<Track> &tracks);

    // The 11-character video id of a YouTube / YouTube Music link, else "".
    static std::string videoIdFromUrl(const std::string &url);
};
