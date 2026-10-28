#include "backend/lyrics/lyricsprovider/lyricsprovider.h"

#include "backend/lyrics/transliterator/transliterator.h"
#include "backend/network/httpclient/httpclient.h"
#include "backend/youtube/ytmatcher/ytmatcher.h"
#include "backend/youtube/ytmusicclient/ytmusicclient.h"

#include <nlohmann/json.hpp>
#include <taglib/fileref.h>
#include <taglib/tpropertymap.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <functional>
#include <iterator>
#include <sstream>
#include <string_view>
#include <system_error>
#include <thread>

namespace fs = std::filesystem;

namespace {

constexpr const char *LrclibBase = "https://lrclib.net/api";
// "Not found" is remembered for a week; lyrics themselves are kept until the cache is cleared.
constexpr auto NotFoundTtl = std::chrono::hours(24 * 7);
constexpr const char *NotFoundMarker = "#chaotic:notfound";
constexpr const char *CreditPrefix = "#chaotic:credit:";
// Cache files of LRCLIB results ("lrclib3": synced-first / no-translation ranking,
// title-only fallback search).
constexpr const char *LrclibCacheName = "lrclib3";
// YouTube Music results ("ytmusic2": timed lyrics from the Android client).
constexpr const char *YtMusicCacheName = "ytmusic2";

std::string lowercase(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    return s;
}

std::string readFile(const fs::path &path)
{
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

// Our Track.artist joins several artists with ", "; online lookups match best on the first.
std::string primaryArtist(const std::string &artist)
{
    const auto comma = artist.find(", ");
    return comma == std::string::npos ? artist : artist.substr(0, comma);
}

// <song>.lrc in the same folder (extension matched case-insensitively).
fs::path findSidecarLrc(const fs::path &audio)
{
    std::error_code ec;
    const fs::path exact = fs::path(audio).replace_extension(".lrc");
    if (fs::is_regular_file(exact, ec))
        return exact;

    const std::string stem = audio.stem().string();
    for (const auto &entry : fs::directory_iterator(audio.parent_path(), ec)) {
        if (entry.path().stem().string() == stem && lowercase(entry.path().extension().string()) == ".lrc")
            return entry.path();
    }
    return {};
}

Lyrics parseFromJson(const nlohmann::json &item)
{
    const auto text = [&](const char *key) {
        const auto it = item.find(key);
        return it != item.end() && it->is_string() ? it->get<std::string>() : std::string();
    };
    Lyrics lyrics = LrcParser::parse(text("syncedLyrics"));
    if (lyrics.empty() || !lyrics.synced)
        lyrics = LrcParser::parse(text("plainLyrics"));
    return lyrics;
}

// LRCLIB is often briefly overloaded (503 "server is busy"): try up to three
// times, pausing a little longer each time.
HttpResponse lrclibGet(const std::string &url)
{
    const std::vector<std::string> headers = {"Accept: application/json"};
    HttpResponse response = HttpClient::get(url, headers);
    for (const int pauseMs : {700, 1500}) {
        if (!response.error.empty() || response.status < 500)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(pauseMs));
        response = HttpClient::get(url, headers);
    }
    return response;
}

// Reads like English prose: mostly ASCII letters and many common English words.
// Used to tell a translation (LRCLIB has some) from the song's own lyrics.
bool looksEnglish(const Lyrics &lyrics)
{
    // (Not "to", "me", "is", "in", "do", "a": common words of romanized Hindi too.)
    static const std::vector<std::string_view> common = {
        "the", "you", "your", "my", "are", "and", "of", "will", "how", "what", "it",
        "this", "that", "with", "for", "on", "don't", "can", "when", "where", "never", "love"};
    size_t words = 0, hits = 0, ascii = 0, bytes = 0;
    for (const LyricLine &line : lyrics.lines) {
        std::string word;
        for (const char c : line.text + ' ') {
            ++bytes;
            if (static_cast<unsigned char>(c) < 0x80)
                ++ascii;
            if (std::isalpha(static_cast<unsigned char>(c)) || c == '\'') {
                word += char(std::tolower(static_cast<unsigned char>(c)));
            } else if (!word.empty()) {
                ++words;
                if (std::find(common.begin(), common.end(), word) != common.end())
                    ++hits;
                word.clear();
            }
        }
    }
    return words >= 8 && ascii * 10 >= bytes * 9 && hits * 5 >= words; // >= 20% common words
}

// Lower is better: synced first, then closeness in length; a likely translation
// only when nothing else fits.
bool linesMostlyLatin(const Lyrics &lyrics)
{
    std::string all;
    for (const LyricLine &line : lyrics.lines)
        all += line.text + ' ';
    return Transliterator::isMostlyLatin(all);
}

double rank(const Lyrics &lyrics, double durationDiffSeconds, bool translationPenalty, bool wantLatin)
{
    double score = durationDiffSeconds;
    if (wantLatin && !linesMostlyLatin(lyrics))
        score += 50; // already-romanized lyrics beat converted ones (but synced still wins)
    if (!lyrics.synced)
        score += 100;
    if (translationPenalty)
        score += 1000;
    return score;
}

std::string cacheKey(const LyricsQuery &q)
{
    std::ostringstream key;
    key << lowercase(primaryArtist(q.artist)) << '|' << lowercase(q.title) << '|'
        << lowercase(q.album) << '|' << q.durationMs / 1000;
    return std::to_string(std::hash<std::string>{}(key.str()));
}

} // namespace

LyricsProvider::LyricsProvider(fs::path cacheDir)
    : m_cacheDir(std::move(cacheDir))
{
}

Lyrics LyricsProvider::fetch(const LyricsQuery &query, const std::vector<std::string> &sources) const
{
    // Sources in the user's order, but synced lyrics (which can follow the song)
    // from any of them beat plain text from an earlier one.
    Lyrics firstPlain;
    for (const std::string &source : sources) {
        Lyrics lyrics;
        if (source == "local")
            lyrics = fromLocal(query);
        else if (source == "lrclib")
            lyrics = cachedOnline(query.latinScript ? std::string(LrclibCacheName) + "-latin" : LrclibCacheName,
                                  query, [&] { return fromLrclib(query); });
        else if (source == "ytmusic")
            lyrics = cachedOnline(YtMusicCacheName, query, [&] { return fromYtMusic(query); });

        if (lyrics.empty())
            continue;
        lyrics.source = source;
        if (lyrics.synced)
            return inScript(std::move(lyrics), query);
        if (firstPlain.empty())
            firstPlain = std::move(lyrics);
    }
    return inScript(std::move(firstPlain), query);
}

Lyrics LyricsProvider::inScript(Lyrics lyrics, const LyricsQuery &query)
{
    if (query.latinScript) {
        for (LyricLine &line : lyrics.lines)
            line.text = Transliterator::toLatin(line.text);
    }
    return lyrics;
}

Lyrics LyricsProvider::fromLocal(const LyricsQuery &query)
{
    if (query.path.empty())
        return {};

    const fs::path audio(query.path);
    if (const fs::path lrc = findSidecarLrc(audio); !lrc.empty()) {
        Lyrics lyrics = LrcParser::parse(readFile(lrc));
        if (!lyrics.empty())
            return lyrics;
    }

    // Embedded: Vorbis/FLAC "LYRICS"/"UNSYNCEDLYRICS", ID3 USLT -> "LYRICS" in TagLib's property map.
    TagLib::FileRef file(audio.c_str(), false);
    if (file.isNull())
        return {};
    const TagLib::PropertyMap props = file.properties();
    for (const char *key : {"LYRICS", "UNSYNCEDLYRICS"}) {
        const TagLib::StringList values = props.value(key);
        if (!values.isEmpty()) {
            Lyrics lyrics = LrcParser::parse(values.front().to8Bit(true));
            if (!lyrics.empty())
                return lyrics;
        }
    }
    return {};
}

std::optional<Lyrics> LyricsProvider::fromLrclib(const LyricsQuery &query)
{
    if (query.title.empty())
        return Lyrics{};
    const std::string artist = primaryArtist(query.artist);

    struct Candidate
    {
        Lyrics lyrics;
        double durationDiff;
        double penalty = 0;
    };
    std::vector<Candidate> candidates;

    // 1) Exact lookup (needs all fields).
    if (!artist.empty() && !query.album.empty() && query.durationMs > 0) {
        const std::string url = std::string(LrclibBase) + "/get?track_name=" + HttpClient::urlEncode(query.title)
                                + "&artist_name=" + HttpClient::urlEncode(artist)
                                + "&album_name=" + HttpClient::urlEncode(query.album)
                                + "&duration=" + std::to_string((query.durationMs + 500) / 1000);
        const HttpResponse response = lrclibGet(url);
        if (!response.error.empty() || response.status >= 500)
            return std::nullopt;
        if (response.ok()) {
            const auto json = nlohmann::json::parse(response.body, nullptr, false);
            if (json.is_object()) {
                Lyrics lyrics = parseFromJson(json);
                // Synced and not a translation: as good as it gets.
                if (!lyrics.empty() && lyrics.synced && !looksEnglish(lyrics)
                    && (!query.latinScript || linesMostlyLatin(lyrics)))
                    return lyrics;
                if (!lyrics.empty())
                    candidates.push_back({std::move(lyrics), 0.0, 0.0});
            }
        }
        // 404, plain only or possibly a translation: also look at the search results
    }

    // Adds the results of a search within maxDiff seconds of the song's length;
    // false = LRCLIB gave no answer.
    const std::string album = YtMatcher::normalize(query.album);
    const auto search = [&](const std::string &url, double maxDiff, bool titleOnly) {
        const HttpResponse response = lrclibGet(url);
        if (!response.ok())
            return false;
        const auto results = nlohmann::json::parse(response.body, nullptr, false);
        if (!results.is_array())
            return false;
        for (const auto &item : results) {
            if (!item.is_object())
                continue;
            double diff = 0;
            if (query.durationMs > 0 && item.contains("duration") && item["duration"].is_number()) {
                diff = std::abs(item["duration"].get<double>() - query.durationMs / 1000.0);
                if (diff > maxDiff)
                    continue;
            }
            double penalty = 0;
            if (titleOnly) { // only the title and length vouch for it: prefer the same album
                const auto it = item.find("albumName");
                const std::string theirs = it != item.end() && it->is_string() ? it->get<std::string>() : "";
                if (album.empty() || YtMatcher::normalize(theirs) != album)
                    penalty = 20;
            }
            Lyrics lyrics = parseFromJson(item);
            if (!lyrics.empty())
                candidates.push_back({std::move(lyrics), diff, penalty});
        }
        return true;
    };

    // 2) Search by title and artist: results within 5 s of the song's length.
    std::string url = std::string(LrclibBase) + "/search?track_name=" + HttpClient::urlEncode(query.title);
    if (!artist.empty())
        url += "&artist_name=" + HttpClient::urlEncode(artist);
    const bool answered = search(url, 5, false);
    if (!answered && candidates.empty())
        return std::nullopt; // busy / offline: try again next time

    // 3) Nothing: LRCLIB may list the song under another artist (Bollywood tags often
    //    name the composer, LRCLIB the singer). Search by title alone, accepting only
    //    a length within 3 s.
    if (candidates.empty() && !artist.empty() && query.durationMs > 0) {
        if (!search(std::string(LrclibBase) + "/search?track_name=" + HttpClient::urlEncode(query.title), 3, true))
            return std::nullopt;
    }

    // An English-looking result is probably a translation when another one isn't.
    bool anyNotEnglish = false;
    std::vector<bool> english;
    for (const Candidate &c : candidates) {
        english.push_back(looksEnglish(c.lyrics));
        anyNotEnglish |= !english.back();
    }
    const Candidate *best = nullptr;
    double bestScore = 1e18;
    for (size_t i = 0; i < candidates.size(); ++i) {
        const double score = rank(candidates[i].lyrics, candidates[i].durationDiff, english[i] && anyNotEnglish,
                                  query.latinScript)
                             + candidates[i].penalty;
        if (score < bestScore) {
            bestScore = score;
            best = &candidates[i];
        }
    }
    return best ? best->lyrics : Lyrics{};
}

std::optional<Lyrics> LyricsProvider::fromYtMusic(const LyricsQuery &query)
{
    if (query.title.empty())
        return Lyrics{};

    Track local;
    local.title = query.title;
    local.artist = query.artist;
    local.album = query.album;
    local.durationMs = query.durationMs;

    const auto results = YtMusicClient::searchSongs(YtMatcher::searchQuery(local));
    if (!results)
        return std::nullopt;
    const auto song = YtMatcher::bestMatch(local, *results);
    if (!song)
        return Lyrics{};

    const auto found = YtMusicClient::lyrics(song->videoId);
    if (!found)
        return std::nullopt;
    Lyrics lyrics;
    if (!found->timed.empty()) {
        lyrics.synced = true;
        for (const YtTimedLine &line : found->timed)
            lyrics.lines.push_back({line.startMs, line.text == "\u266a" ? std::string() : line.text}); // ♪ = music
    } else {
        lyrics = LrcParser::parse(found->text);
    }
    lyrics.credit = found->credit;
    return lyrics;
}

Lyrics LyricsProvider::cachedOnline(const std::string &source, const LyricsQuery &query,
                                    const std::function<std::optional<Lyrics>()> &fetchOnline) const
{
    std::error_code ec;
    const fs::path file = m_cacheDir / (source + "-" + cacheKey(query) + ".lrc");

    if (fs::is_regular_file(file, ec)) {
        std::string cached = readFile(file);
        if (cached.rfind(NotFoundMarker, 0) == 0) {
            const auto age = fs::file_time_type::clock::now() - fs::last_write_time(file, ec);
            if (!ec && age < NotFoundTtl)
                return {};
        } else {
            std::string credit;
            if (cached.rfind(CreditPrefix, 0) == 0) {
                const size_t newline = cached.find('\n');
                credit = cached.substr(std::string_view(CreditPrefix).size(),
                                       newline - std::string_view(CreditPrefix).size());
                cached.erase(0, newline == std::string::npos ? cached.size() : newline + 1);
            }
            Lyrics lyrics = LrcParser::parse(cached);
            lyrics.credit = credit;
            return lyrics;
        }
    }

    const std::optional<Lyrics> answer = fetchOnline();
    if (!answer)
        return {}; // offline / server trouble: try again next time
    const Lyrics &lyrics = *answer;

    fs::create_directories(m_cacheDir, ec);
    std::ofstream out(file, std::ios::trunc);
    if (lyrics.empty()) {
        out << NotFoundMarker << '\n';
    } else {
        if (!lyrics.credit.empty())
            out << CreditPrefix << lyrics.credit << '\n';
        for (const LyricLine &line : lyrics.lines) {
            if (lyrics.synced) {
                char stamp[40];
                std::snprintf(stamp, sizeof(stamp), "[%02lld:%02lld.%02lld]",
                              (long long)(line.timeMs / 60000), (long long)(line.timeMs / 1000 % 60),
                              (long long)(line.timeMs % 1000 / 10));
                out << stamp;
            }
            out << line.text << '\n';
        }
    }
    return lyrics;
}
