#pragma once

#include "backend/youtube/ytmusicclient/ytmusicclient.h"

#include <optional>
#include <string>
#include <vector>

// Uses the installed yt-dlp program for what the web API cannot do without an
// account: direct audio stream addresses, and reading the user's own library
// with their browser's YouTube session ("--cookies-from-browser"; the app
// never sees a password). Blocking: call from worker threads.
class YtDlp
{
public:
    // yt-dlp's version, or nullopt if it is not installed.
    static std::optional<std::string> version();

    // Direct audio stream address for a video (expires after a few hours).
    static std::optional<std::string> streamUrl(const std::string &videoId, const std::string &cookiesBrowser);

    // The videos behind a link: one for a video link, all of them for a playlist.
    // nullopt = yt-dlp failed (bad link, offline, not signed in, ...).
    static std::optional<std::vector<YtSong>> entries(const std::string &url, const std::string &cookiesBrowser);

    // "Liked music" of the signed-in user.
    static std::optional<std::vector<YtSong>> likedSongs(const std::string &cookiesBrowser);

    // Last error from yt-dlp (per thread), in plain words.
    static std::string lastError();
    // The last failure was YouTube limiting this connection ("confirm you're
    // not a bot", HTTP 429): more requests right away only prolong it.
    static bool lastErrorWasRateLimit();
};
