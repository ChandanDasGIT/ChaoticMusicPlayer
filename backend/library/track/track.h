#pragma once

#include <cstdint>
#include <string>

// One audio file and its metadata.
struct Track
{
    std::string path;
    std::string title;
    std::string artist;
    std::string album;
    std::string albumArtist; // "" = not tagged
    int disc = 0;      // 0 = unknown
    int discTotal = 0; // 0 = unknown
    int track = 0;     // 0 = unknown
    int64_t durationMs = 0;
    bool hasCover = false; // embedded cover art present

    // YouTube Music tracks: path is "ytmusic:<videoId>" (see StreamResolver).
    std::string videoId;
    std::string thumbnailUrl;
};
