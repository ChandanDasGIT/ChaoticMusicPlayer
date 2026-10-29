#pragma once

#include "backend/youtube/ytmusicclient/ytmusicclient.h"

#include <optional>
#include <string>
#include <vector>

// Uses the yt-dlp program for what the web API cannot do without an account:
// direct audio stream addresses, and reading the user's own library with
// their browser's YouTube session ("--cookies-from-browser"; the app never
// sees a password). Blocking: call from worker threads.
class YtDlp
{
public:
    // The program to run: a file's path, or "yt-dlp" (looked up in PATH; the
    // default). The app picks the newest copy it has (wrapper: YtDlpManager).
    static void setExecutable(const std::string &program);
    static std::string executable();

    // yt-dlp's version ("2025.10.22"), or nullopt if it can't run.
    static std::optional<std::string> version();
    static std::optional<std::string> versionOf(const std::string &program);
    // Versions are dates, sometimes with a fourth part: "2025.10.22.1".
    static bool olderThan(const std::string &a, const std::string &b);

    enum class AudioQuality { Best, Lowest }; // highest / lowest bitrate stream

    // Direct audio stream address for a video (expires after a few hours).
    static std::optional<std::string> streamUrl(const std::string &videoId, const std::string &cookiesBrowser,
                                                AudioQuality quality = AudioQuality::Best);

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
