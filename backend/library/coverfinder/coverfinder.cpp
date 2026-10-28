#include "backend/library/coverfinder/coverfinder.h"

#include "backend/network/httpclient/httpclient.h"
#include "backend/youtube/ytmatcher/ytmatcher.h"
#include "backend/youtube/ytmusicclient/ytmusicclient.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <functional>
#include <future>
#include <iterator>
#include <regex>
#include <system_error>
#include <thread>

namespace fs = std::filesystem;
using nlohmann::json;

namespace {

constexpr auto MissingTtl = std::chrono::hours(24 * 7);
// When two images are equally large: the earlier source wins.
constexpr const char *Preference[] = {"folder", "itunes", "musicbrainz", "ytmusic"};

std::string lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return s;
}

std::string primaryArtist(const std::string &artist)
{
    const std::vector<std::string> artists = YtMatcher::splitArtists(artist);
    return artists.empty() ? artist : artists.front();
}

// Who the album is by: the album artist, else the song's main artist.
std::string albumBy(const CoverQuery &q)
{
    return q.albumArtist.empty() ? primaryArtist(q.artist) : q.albumArtist;
}

std::vector<unsigned char> readBytes(const fs::path &file)
{
    std::ifstream in(file, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::vector<unsigned char> download(const std::string &url, const std::vector<std::string> &headers = {})
{
    const HttpResponse response = HttpClient::get(url, headers, 20000);
    if (!response.ok())
        return {};
    return {response.body.begin(), response.body.end()};
}

std::string str(const json &item, const char *key)
{
    const auto it = item.find(key);
    return it != item.end() && it->is_string() ? it->get<std::string>() : std::string();
}

bool endsWith(const std::string &s, const std::string &suffix)
{
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// Normalized album name without trailing edition words:
// "Guru (Original Motion Picture Soundtrack)" -> "guru",
// "Mere Bhagwan Mere Guru - EP" -> "mere bhagwan mere guru". A name made only
// of such words keeps them ("1989 (Deluxe)" -> "1989", not "").
std::string albumKey(const std::string &album)
{
    static const char *const Editions[] = {
        "original motion picture soundtrack", "motion picture soundtrack", "original soundtrack",
        "original score", "soundtrack", "ost", "deluxe edition", "deluxe version", "deluxe",
        "expanded edition", "expanded", "anniversary edition", "special edition", "remastered",
        "remaster", "bonus track version", "ep", "single",
    };
    std::string key = YtMatcher::normalize(album);
    for (bool changed = true; changed;) {
        changed = false;
        for (const char *edition : Editions) {
            const std::string suffix = std::string(" ") + edition;
            if (endsWith(key, suffix)) {
                key.erase(key.size() - suffix.size());
                changed = true;
            }
        }
        // A trailing year ("Album 2011"), but not a name that is only a year.
        if (key.size() > 5 && key[key.size() - 5] == ' '
            && std::all_of(key.end() - 4, key.end(), [](unsigned char c) { return std::isdigit(c); })
            && (key.compare(key.size() - 4, 2, "19") == 0 || key.compare(key.size() - 4, 2, "20") == 0)) {
            key.erase(key.size() - 5);
            changed = true;
        }
    }
    return key;
}

bool artistMatches(const std::string &ours, const std::string &theirs)
{
    for (const std::string &x : YtMatcher::splitArtists(ours)) {
        for (const std::string &y : YtMatcher::splitArtists(theirs)) {
            const std::string a = YtMatcher::normalize(x), b = YtMatcher::normalize(y);
            if (!a.empty() && a == b)
                return true;
        }
    }
    return false;
}

// Their artist is the album artist or one of the song's artists.
bool byOurArtist(const CoverQuery &q, const std::string &theirs)
{
    return (!q.albumArtist.empty() && artistMatches(q.albumArtist, theirs)) || artistMatches(q.artist, theirs);
}

// ---- Sources: each returns the address of its best guess, or "" ----------

std::string fromYtMusic(const CoverQuery &q)
{
    // A song's thumbnail is its album's cover: only usable when the album is ours.
    const auto ourAlbum = [&](const std::string &album) { return q.album.empty() || CoverFinder::sameAlbum(q.album, album); };
    std::string url;
    if (!q.knownYtThumbnail.empty() && ourAlbum(q.knownYtAlbum)) {
        url = q.knownYtThumbnail;
    } else {
        Track local;
        local.title = q.title;
        local.artist = q.artist;
        local.album = q.album;
        local.durationMs = q.durationMs;
        const auto results = YtMusicClient::searchSongs(YtMatcher::searchQuery(local));
        if (!results)
            return {};
        const auto song = YtMatcher::bestMatch(local, *results);
        if (!song || !ourAlbum(song->album))
            return {};
        url = song->thumbnailUrl;
    }
    // googleusercontent covers: "=s0" asks for the original size.
    static const std::regex size(R"(=(w\d+-h\d+|s\d+)[^/]*$)");
    if (url.find("googleusercontent.com") != std::string::npos)
        url = std::regex_replace(url, size, "=s0");
    return url;
}

std::string itunesSearch(const CoverQuery &q, const std::string &term, bool byAlbum)
{
    const std::string url = "https://itunes.apple.com/search?media=music&limit=15&entity="
                            + std::string(byAlbum ? "album" : "song") + "&term=" + HttpClient::urlEncode(term);
    const HttpResponse response = HttpClient::get(url, {"Accept: application/json"}, 15000);
    if (!response.ok())
        return {};
    const json answer = json::parse(response.body, nullptr, false);
    const auto results = answer.is_object() ? answer.find("results") : answer.end();
    if (!answer.is_object() || results == answer.end() || !results->is_array())
        return {};
    for (const json &item : *results) {
        const bool same = byAlbum ? CoverFinder::sameAlbum(q.album, str(item, "collectionName"))
                                  : YtMatcher::titleMatch(q.title, str(item, "trackName")) != YtMatcher::Match::None;
        if (!same || !byOurArtist(q, str(item, "artistName")))
            continue;
        std::string art = str(item, "artworkUrl100");
        // Asked for big: Apple sends at most the original's size.
        static const std::regex size(R"(\d+x\d+bb)");
        return std::regex_replace(art, size, "3000x3000bb");
    }
    return {};
}

std::string fromItunes(const CoverQuery &q)
{
    if (q.album.empty())
        return itunesSearch(q, primaryArtist(q.artist) + ' ' + q.title, false);
    // By artist and album; some albums are only found by their name.
    std::string url = itunesSearch(q, albumBy(q) + ' ' + q.album, true);
    if (url.empty())
        url = itunesSearch(q, q.album, true);
    return url;
}

std::string luceneQuoted(const std::string &value)
{
    std::string out = "\"";
    for (char c : value) {
        if (c == '"' || c == '\\')
            out += '\\';
        out += c;
    }
    return out + '"';
}

std::string fromMusicBrainz(const CoverQuery &q)
{
    if (q.album.empty())
        return {};
    const std::string query = "releasegroup:" + luceneQuoted(q.album) + " AND artist:" + luceneQuoted(albumBy(q));
    const std::string url = "https://musicbrainz.org/ws/2/release-group/?fmt=json&limit=5&query=" + HttpClient::urlEncode(query);
    HttpResponse response = HttpClient::get(url, {"Accept: application/json"}, 15000);
    if (response.status == 503) { // MusicBrainz allows one request per second: once more, a bit later
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        response = HttpClient::get(url, {"Accept: application/json"}, 15000);
    }
    if (!response.ok())
        return {};
    const json answer = json::parse(response.body, nullptr, false);
    const auto groups = answer.is_object() ? answer.find("release-groups") : answer.end();
    if (!answer.is_object() || groups == answer.end() || !groups->is_array())
        return {};
    for (const json &group : *groups) {
        const int score = group.contains("score") && group["score"].is_number() ? group["score"].get<int>() : 0;
        if (score < 90 || !CoverFinder::sameAlbum(q.album, str(group, "title")))
            continue;
        // Credited artists, when listed: one must be ours.
        const auto credits = group.find("artist-credit");
        if (credits != group.end() && credits->is_array() && !credits->empty()) {
            bool ours = false;
            for (const json &credit : *credits)
                ours = ours || (credit.is_object() && byOurArtist(q, str(credit, "name")));
            if (!ours)
                continue;
        }
        // The largest prepared size (originals can be huge scans).
        return "https://coverartarchive.org/release-group/" + str(group, "id") + "/front-1200";
    }
    return {};
}

} // namespace

CoverFinder::CoverFinder(fs::path cacheDir)
    : m_cacheDir(std::move(cacheDir))
{
}

bool CoverFinder::sameAlbum(const std::string &ours, const std::string &theirs)
{
    const std::string a = albumKey(ours);
    return !a.empty() && a == albumKey(theirs);
}

void CoverFinder::removeOldCache() const
{
    std::error_code ec;
    for (const auto &entry : fs::directory_iterator(m_cacheDir, ec)) {
        if (entry.path().filename().string().rfind("cover-", 0) == 0)
            fs::remove(entry.path(), ec);
    }
}

bool CoverFinder::imageSize(const std::vector<unsigned char> &d, int &width, int &height)
{
    width = height = 0;
    if (d.size() >= 24 && d[0] == 0x89 && d[1] == 'P' && d[2] == 'N' && d[3] == 'G') {
        width = (d[16] << 24) | (d[17] << 16) | (d[18] << 8) | d[19];
        height = (d[20] << 24) | (d[21] << 16) | (d[22] << 8) | d[23];
        return width > 0 && height > 0;
    }
    if (d.size() < 4 || d[0] != 0xFF || d[1] != 0xD8)
        return false;
    for (size_t i = 2; i + 9 < d.size();) {
        if (d[i] != 0xFF) {
            ++i;
            continue;
        }
        const unsigned char marker = d[i + 1];
        if (marker == 0xFF) {
            ++i;
            continue;
        }
        const size_t length = size_t(d[i + 2] << 8 | d[i + 3]);
        const bool startOfFrame = marker >= 0xC0 && marker <= 0xCF && marker != 0xC4 && marker != 0xC8 && marker != 0xCC;
        if (startOfFrame) {
            height = d[i + 5] << 8 | d[i + 6];
            width = d[i + 7] << 8 | d[i + 8];
            return width > 0 && height > 0;
        }
        i += 2 + length;
    }
    return false;
}

CoverFinder::Image CoverFinder::folderImage(const std::string &audioPath)
{
    Image best;
    std::error_code ec;
    for (const auto &entry : fs::directory_iterator(fs::path(audioPath).parent_path(), ec)) {
        const std::string ext = lower(entry.path().extension().string());
        const std::string stem = lower(entry.path().stem().string());
        if (ext != ".jpg" && ext != ".jpeg" && ext != ".png")
            continue;
        const bool coverName = stem == "cover" || stem == "folder" || stem == "front" || stem == "album"
                               || stem.rfind("albumart", 0) == 0 || stem == "artwork";
        if (!coverName)
            continue;
        Image image;
        image.data = readBytes(entry.path());
        image.source = "folder";
        if (imageSize(image.data, image.width, image.height)
            && int64_t(image.width) * image.height > int64_t(best.width) * best.height)
            best = std::move(image);
    }
    return best;
}

fs::path CoverFinder::cacheFile(const CoverQuery &q, const char *suffix) const
{
    // One cover per album (by its album artist, else its main artist); per song when there is no album.
    const std::string key = q.album.empty() ? "song|" + q.path : lower(albumBy(q)) + '|' + lower(q.album);
    char hex[17];
    std::snprintf(hex, sizeof(hex), "%016zx", std::hash<std::string>{}(key));
    return m_cacheDir / (std::string("cover2-") + hex + suffix);
}

CoverFinder::Image CoverFinder::cached(const CoverQuery &query) const
{
    std::error_code ec;
    const fs::path file = cacheFile(query, ".img");
    if (fs::is_regular_file(file, ec)) {
        Image image;
        image.data = readBytes(file);
        std::ifstream source(cacheFile(query, ".src"));
        std::getline(source, image.source);
        if (imageSize(image.data, image.width, image.height))
            return image;
    }
    return folderImage(query.path); // not looked up online (yet)
}

bool CoverFinder::lookedUp(const CoverQuery &query) const
{
    std::error_code ec;
    return fs::is_regular_file(cacheFile(query, ".src"), ec) || knownMissing(query);
}

bool CoverFinder::knownMissing(const CoverQuery &query) const
{
    std::error_code ec;
    const fs::path marker = cacheFile(query, ".none");
    if (!fs::is_regular_file(marker, ec))
        return false;
    const auto age = fs::file_time_type::clock::now() - fs::last_write_time(marker, ec);
    return !ec && age < MissingTtl;
}

CoverFinder::Image CoverFinder::find(const CoverQuery &query, const std::vector<std::string> &sources,
                                     const std::function<void(const Image &)> &improved) const
{
    const auto wanted = [&](const char *id) { return std::find(sources.begin(), sources.end(), id) != sources.end(); };
    const auto rank = [](const std::string &source) {
        return std::find(std::begin(Preference), std::end(Preference), source) - std::begin(Preference);
    };

    std::error_code ec;
    fs::create_directories(m_cacheDir, ec);
    // The best image so far is saved at once, so it can be shown while slower sources answer.
    Image best;
    const auto consider = [&](Image image) {
        if (image.data.empty() || !imageSize(image.data, image.width, image.height))
            return;
        const int64_t area = int64_t(image.width) * image.height;
        const int64_t bestArea = int64_t(best.width) * best.height;
        if (area < bestArea || (area == bestArea && rank(image.source) >= rank(best.source)))
            return;
        best = std::move(image);
        if (best.source != "folder") { // folder images are read from the folder each time
            // Written aside, then renamed: a reader never sees half an image.
            const fs::path temp = cacheFile(query, ".img.tmp");
            {
                std::ofstream out(temp, std::ios::binary | std::ios::trunc);
                out.write(reinterpret_cast<const char *>(best.data.data()), std::streamsize(best.data.size()));
            }
            fs::rename(temp, cacheFile(query, ".img"), ec);
        } else {
            fs::remove(cacheFile(query, ".img"), ec);
        }
        std::ofstream(cacheFile(query, ".src"), std::ios::trunc) << best.source << '\n'; // = looked up
        if (improved)
            improved(best);
    };

    // The online sources are asked at the same time: the wait is the slowest one, not their sum.
    std::vector<std::future<Image>> pending;
    const auto online = [&](const char *id, std::function<std::string()> address) {
        if (!wanted(id))
            return;
        pending.push_back(std::async(std::launch::async, [id, address = std::move(address)] {
            Image image;
            const std::string url = address();
            if (!url.empty()) {
                image.data = download(url);
                image.source = id;
            }
            return image;
        }));
    };
    online("itunes", [query] { return fromItunes(query); });
    online("musicbrainz", [query] { return fromMusicBrainz(query); });
    online("ytmusic", [query] { return fromYtMusic(query); });

    if (wanted("folder"))
        consider(folderImage(query.path));
    // In the order they arrive.
    while (!pending.empty()) {
        for (auto it = pending.begin(); it != pending.end();) {
            if (it->wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
                consider(it->get());
                it = pending.erase(it);
            } else {
                ++it;
            }
        }
        if (!pending.empty())
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    if (best.data.empty())
        std::ofstream(cacheFile(query, ".none")) << "nothing found\n";
    else
        fs::remove(cacheFile(query, ".none"), ec);
    return best;
}
