#pragma once

#include "backend/library/track/track.h"

#include <filesystem>
#include <string>
#include <vector>

// Everything shown in the track details window, grouped into sections.
struct TrackDetails
{
    struct Field
    {
        std::string label;
        std::string value;
    };

    std::vector<Field> tags;
    std::vector<Field> audio;
    std::vector<Field> file;
    std::vector<Field> rawTags; // every tag in the file, as stored
};

// Reads tags and audio properties with TagLib.
class Metadata
{
public:
    // Always returns a usable Track: missing tags fall back to the file name
    // (e.g. "03 - Song.mp3" gives track 3, title "Song").
    static Track read(const std::filesystem::path &path);

    // Full details for one file (tags + FFmpeg audio properties + file info).
    static TrackDetails readDetails(const std::filesystem::path &path);

    // Encoded cover image (JPEG/PNG bytes): the embedded front cover, else any
    // embedded picture, else cover/folder/front.(jpg|png) next to the file.
    // Empty when there is none.
    static std::vector<unsigned char> readCover(const std::filesystem::path &path);
};
