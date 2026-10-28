#pragma once

#include "backend/library/track/track.h"
#include "backend/youtube/ytmusicclient/ytmusicclient.h"

#include <optional>
#include <string>
#include <vector>

// Decides whether a YouTube Music song is the same as a local track.
// Rule: the titles match AND (the albums match OR an artist matches), after
// normalizing names. Titles may also match *closely* (different spellings /
// transliterations such as "Rag Noi" vs "Raag Noy"); exact title matches are
// preferred, then the closest duration.
class YtMatcher
{
public:
    // Lowercase, punctuation to spaces, and without "(feat. …)", "(From …)",
    // "- Remastered 2011" style extras.
    static std::string normalize(const std::string &name);
    // "A, B & C feat. D" -> {"A", "B", "C", "D"}
    static std::vector<std::string> splitArtists(const std::string &artists);

    enum class Match { None, Exact, Close };

    // Spelling-tolerant form of a normalized name: doubled letters collapsed,
    // y->i, ph->f, w->v, "h" after a consonant dropped ("raag noy" -> "rag noi").
    static std::string soundKey(const std::string &normalized);
    static Match titleMatch(const std::string &localTitle, const std::string &ytTitle);

    static Match match(const Track &local, const YtSong &song);
    static bool matches(const Track &local, const YtSong &song) { return match(local, song) != Match::None; }
    // `close` (if given) tells whether the winner only matched closely.
    static std::optional<YtSong> bestMatch(const Track &local, const std::vector<YtSong> &candidates,
                                           bool *close = nullptr);

    // What to search YouTube Music for.
    static std::string searchQuery(const Track &local);
};
