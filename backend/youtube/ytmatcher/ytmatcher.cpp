#include "backend/youtube/ytmatcher/ytmatcher.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <string_view>

namespace {

// Bracketed extras that don't change which song it is.
constexpr std::array<std::string_view, 14> ExtraWords = {
    "feat", "ft", "featuring", "from", "with", "remaster", "remastered", "version",
    "edit", "mono", "stereo", "explicit", "official", "audio",
};

std::string lowerAscii(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return c < 0x80 ? char(std::tolower(c)) : char(c); });
    return s;
}

bool startsWithExtraWord(const std::string &lowered)
{
    size_t i = 0;
    while (i < lowered.size() && (lowered[i] == ' ' || lowered[i] == '"' || lowered[i] == '\''))
        ++i;
    for (std::string_view word : ExtraWords) {
        if (lowered.compare(i, word.size(), word) == 0) {
            const size_t after = i + word.size();
            if (after == lowered.size() || !std::isalnum(static_cast<unsigned char>(lowered[after])))
                return true;
        }
    }
    return false;
}

void trim(std::string &s)
{
    const auto begin = s.find_first_not_of(' ');
    if (begin == std::string::npos) {
        s.clear();
        return;
    }
    s = s.substr(begin, s.find_last_not_of(' ') - begin + 1);
}

} // namespace

std::string YtMatcher::normalize(const std::string &name)
{
    std::string s = lowerAscii(name);

    // Drop "(...)" / "[...]" groups that only add extras.
    for (const auto &[open, close] : {std::pair{'(', ')'}, std::pair{'[', ']'}}) {
        size_t start = 0;
        while ((start = s.find(open, start)) != std::string::npos) {
            const size_t end = s.find(close, start + 1);
            if (end == std::string::npos)
                break;
            if (startsWithExtraWord(s.substr(start + 1, end - start - 1)))
                s.erase(start, end - start + 1);
            else
                start = end + 1;
        }
    }

    // " - Remastered 2011", " - Radio Edit", " feat. X" at the end
    for (const char *separator : {" - ", " feat. ", " ft. ", " featuring "}) {
        const size_t pos = s.find(separator);
        if (pos == std::string::npos)
            continue;
        const std::string tail = s.substr(pos + std::string_view(separator).size());
        if (std::string_view(separator) != " - " || startsWithExtraWord(tail)
            || tail.find("remaster") != std::string::npos || tail.find("version") != std::string::npos
            || tail.find("edit") != std::string::npos)
            s.erase(pos);
    }

    // ASCII punctuation becomes a space; other bytes (accents, non-Latin scripts) are kept.
    for (char &c : s) {
        const auto u = static_cast<unsigned char>(c);
        if (u < 0x80 && !std::isalnum(u))
            c = ' ';
    }
    std::string collapsed;
    for (char c : s) {
        if (c == ' ' && (collapsed.empty() || collapsed.back() == ' '))
            continue;
        collapsed += c;
    }
    trim(collapsed);
    return collapsed;
}

std::vector<std::string> YtMatcher::splitArtists(const std::string &artists)
{
    std::string s = artists;
    for (const char *separator : {" feat. ", " ft. ", " featuring ", " & ", " x ", " and ", ";", " / "}) {
        size_t pos = 0;
        const std::string_view sep(separator);
        while ((pos = s.find(sep, pos)) != std::string::npos)
            s.replace(pos, sep.size(), ", ");
    }

    std::vector<std::string> result;
    size_t start = 0;
    for (;;) {
        const size_t comma = s.find(',', start);
        std::string part = s.substr(start, comma - start);
        trim(part);
        if (!part.empty())
            result.push_back(part);
        if (comma == std::string::npos)
            break;
        start = comma + 1;
    }
    return result;
}

std::string YtMatcher::soundKey(const std::string &normalized)
{
    std::string s;
    // ph -> f, w -> v, y -> i
    for (size_t i = 0; i < normalized.size(); ++i) {
        const char c = normalized[i];
        if (c == 'p' && i + 1 < normalized.size() && normalized[i + 1] == 'h') {
            s += 'f';
            ++i;
        } else if ((c == 'e' || c == 'o') && i + 1 < normalized.size() && normalized[i + 1] == c) {
            s += c == 'e' ? 'i' : 'u'; // long vowels: ee -> i, oo -> u
            ++i;
        } else if (c == 'w') {
            s += 'v';
        } else if (c == 'y') {
            s += 'i';
        } else {
            s += c;
        }
    }

    // Drop "h" after a consonant (bh, dh, kh, gh, th...) but keep "ch" / "sh".
    const auto isVowel = [](char c) { return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u'; };
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];
        if (c == 'h' && i > 0) {
            const char prev = s[i - 1];
            const bool consonant = std::isalpha(static_cast<unsigned char>(prev)) && !isVowel(prev);
            if (consonant && prev != 'c' && prev != 's')
                continue;
        }
        // Doubled letters collapse: aa -> a, ee -> e, ll -> l ...
        if (!out.empty() && out.back() == c && std::isalpha(static_cast<unsigned char>(c)))
            continue;
        out += c;
    }
    return out;
}

namespace {

std::vector<std::string> words(const std::string &s)
{
    std::vector<std::string> result;
    size_t start = 0;
    while (start < s.size()) {
        const size_t space = s.find(' ', start);
        if (space != start)
            result.push_back(s.substr(start, space - start));
        if (space == std::string::npos)
            break;
        start = space + 1;
    }
    return result;
}

size_t levenshtein(const std::string &a, const std::string &b)
{
    std::vector<size_t> row(b.size() + 1);
    for (size_t j = 0; j <= b.size(); ++j)
        row[j] = j;
    for (size_t i = 1; i <= a.size(); ++i) {
        size_t diagonal = row[0];
        row[0] = i;
        for (size_t j = 1; j <= b.size(); ++j) {
            const size_t above = row[j];
            row[j] = std::min({row[j] + 1, row[j - 1] + 1, diagonal + (a[i - 1] == b[j - 1] ? 0 : 1)});
            diagonal = above;
        }
    }
    return row[b.size()];
}

constexpr double CloseRatio = 0.85;  // similarity needed for a close title match
constexpr size_t MinPrefixWords = 2; // "x y" may match "x y z ..." but "x" alone may not

} // namespace

YtMatcher::Match YtMatcher::titleMatch(const std::string &localTitle, const std::string &ytTitle)
{
    const std::string a = normalize(localTitle);
    const std::string b = normalize(ytTitle);
    if (a.empty() || b.empty())
        return Match::None;
    if (a == b)
        return Match::Exact;

    const std::vector<std::string> ka = words(soundKey(a));
    const std::vector<std::string> kb = words(soundKey(b));
    if (ka == kb)
        return Match::Close;

    // One title starts with the other, word by word.
    const std::vector<std::string> &shorter = ka.size() <= kb.size() ? ka : kb;
    const std::vector<std::string> &longer = ka.size() <= kb.size() ? kb : ka;
    if (shorter.size() >= MinPrefixWords && std::equal(shorter.begin(), shorter.end(), longer.begin()))
        return Match::Close;

    // Small spelling differences.
    const std::string ja = soundKey(a), jb = soundKey(b);
    const size_t length = std::max(ja.size(), jb.size());
    if (length >= 4 && 1.0 - double(levenshtein(ja, jb)) / double(length) >= CloseRatio)
        return Match::Close;
    return Match::None;
}

YtMatcher::Match YtMatcher::match(const Track &local, const YtSong &song)
{
    const Match title = titleMatch(local.title, song.title);
    if (title == Match::None)
        return Match::None;

    // The album or an artist must match exactly, however close the title is.
    const std::string album = normalize(local.album);
    if (!album.empty() && album == normalize(song.album))
        return title;

    for (const std::string &localArtist : splitArtists(local.artist)) {
        const std::string a = normalize(localArtist);
        for (const std::string &ytArtist : song.artists) {
            for (const std::string &part : splitArtists(ytArtist)) {
                if (!a.empty() && a == normalize(part))
                    return title;
            }
        }
    }
    return Match::None;
}

std::optional<YtSong> YtMatcher::bestMatch(const Track &local, const std::vector<YtSong> &candidates, bool *close)
{
    const YtSong *best = nullptr;
    bool bestClose = false;
    int64_t bestDiff = 0;
    for (const YtSong &song : candidates) {
        const Match m = match(local, song);
        if (m == Match::None)
            continue;
        const bool isClose = m == Match::Close;
        const int64_t diff = local.durationMs > 0 && song.durationMs > 0
                                 ? std::llabs(local.durationMs - song.durationMs) : 0;
        // Exact title matches first, then the closest duration.
        if (!best || (bestClose && !isClose) || (bestClose == isClose && diff < bestDiff)) {
            best = &song;
            bestClose = isClose;
            bestDiff = diff;
        }
    }
    if (close)
        *close = best && bestClose;
    return best ? std::optional<YtSong>(*best) : std::nullopt;
}

std::string YtMatcher::searchQuery(const Track &local)
{
    const std::vector<std::string> artists = splitArtists(local.artist);
    return artists.empty() ? local.title : local.title + " " + artists.front();
}
