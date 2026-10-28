#pragma once

#include "backend/lyrics/lrcparser/lrcparser.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

struct LyricsQuery
{
    std::string path; // local file (for .lrc / embedded lyrics)
    std::string title;
    std::string artist;
    std::string album;
    int64_t durationMs = 0;
    bool latinScript = false; // lyrics in English letters (romanized), not the original script
};

// Finds lyrics for a track by trying sources in the given order:
//   "local"  - <same name>.lrc next to the file, then lyrics embedded in the tags
//   "lrclib" - lrclib.net (online); results, including "not found", are cached
//   "ytmusic" - YouTube Music (finds the song, then its lyrics; usually unsynced); cached
// With latinScript, lyrics already written in English letters are preferred,
// and anything else is romanized (Transliterator) after fetching; caches keep
// what the sources returned.
// Blocking: call from a worker thread.
class LyricsProvider
{
public:
    explicit LyricsProvider(std::filesystem::path cacheDir);

    Lyrics fetch(const LyricsQuery &query, const std::vector<std::string> &sources) const;

    static Lyrics fromLocal(const LyricsQuery &query);
    // Empty lyrics = LRCLIB has none; nullopt = no answer (offline, server error),
    // which must not be cached as "not found".
    static std::optional<Lyrics> fromLrclib(const LyricsQuery &query);
    static std::optional<Lyrics> fromYtMusic(const LyricsQuery &query); // same convention

private:
    static Lyrics inScript(Lyrics lyrics, const LyricsQuery &query); // romanizes if asked

    // Cache file per source + track: the lyrics, or a marker for "not found".
    Lyrics cachedOnline(const std::string &source, const LyricsQuery &query,
                        const std::function<std::optional<Lyrics>()> &fetchOnline) const;

    std::filesystem::path m_cacheDir;
};
