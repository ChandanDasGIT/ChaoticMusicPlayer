#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct YtSong
{
    std::string videoId;
    std::string title;
    std::vector<std::string> artists;
    std::string album;
    std::string albumId;
    std::string year; // filled in separately (albumYear)
    int64_t durationMs = 0;
    std::string thumbnailUrl;
};

// A playlist or album card (YouTube Music home page).
struct YtCollection
{
    std::string browseId; // "VL..." playlist, "MPREb_..." album
    std::string title;
    std::string subtitle;
    std::string thumbnailUrl;
};

struct YtShelf
{
    std::string title;
    std::vector<YtCollection> items;
};

struct YtTimedLine
{
    int64_t startMs;
    std::string text;
};

struct YtLyrics
{
    std::string text;                // plain text, empty if the song has none
    std::vector<YtTimedLine> timed;  // timed lines when YouTube Music has them
    std::string credit;              // e.g. "Source: Musixmatch"
};

// Talks to YouTube Music's web API (the requests music.youtube.com itself
// makes). No account or API key. Blocking: call from worker threads.
// Every call returns std::nullopt when YouTube Music could not be reached or
// answered unexpectedly, so callers never mistake an outage for "nothing found".
class YtMusicClient
{
public:
    static std::optional<std::vector<YtSong>> searchSongs(const std::string &query);
    // Suggested searches for what has been typed so far ("arijit si" -> "arijit singh", ...).
    static std::optional<std::vector<std::string>> searchSuggestions(const std::string &input);
    static std::optional<std::string> albumYear(const std::string &albumId); // "" if unknown
    static std::optional<YtLyrics> lyrics(const std::string &videoId);

    // Home page shelves (without an account: trending / genre playlists).
    static std::optional<std::vector<YtShelf>> home();
    // Songs of a playlist or album (first page, up to ~100).
    static std::optional<std::vector<YtSong>> collection(const std::string &browseId);
    // What YouTube Music plays after this song (its radio / "up next" list,
    // ~25-50 songs). The song itself comes first.
    static std::optional<std::vector<YtSong>> upNext(const std::string &videoId);

    static std::string watchUrl(const std::string &videoId);
};
