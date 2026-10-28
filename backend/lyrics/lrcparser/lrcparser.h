#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct LyricLine
{
    int64_t timeMs = -1; // -1 for unsynced lyrics
    std::string text;    // may be empty (instrumental gap)
};

struct Lyrics
{
    std::vector<LyricLine> lines;
    bool synced = false;
    std::string source; // "local", "lrclib", "ytmusic"
    std::string credit; // e.g. "Source: Musixmatch" (YouTube Music)

    bool empty() const { return lines.empty(); }
};

// Parses LRC ("[01:23.45]text") or plain-text lyrics.
//  - several timestamps on one line ("[00:12.00][01:40.00]chorus") become several lines
//  - "[offset:+250]" shifts all times (positive = earlier, as in the LRC spec)
//  - other "[tag:value]" lines and word-timing tags ("<00:12.34>") are ignored
//  - text without any timestamp is returned as unsynced lines
class LrcParser
{
public:
    static Lyrics parse(const std::string &text);
};
