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
    std::string genre;       // "" = not tagged; several: "A, B"
    int disc = 0;      // 0 = unknown
    int discTotal = 0; // 0 = unknown
    int track = 0;     // 0 = unknown
    int64_t durationMs = 0;
    bool hasCover = false; // embedded cover art present
    int64_t addedAt = 0;   // library songs: when the library first found it (unix seconds)
    int64_t modifiedAt = 0; // library songs: the file's last change (file clock: only for comparing)

    // YouTube Music tracks: path is "ytmusic:<videoId>" (see StreamResolver).
    std::string videoId;
    std::string thumbnailUrl;
};
