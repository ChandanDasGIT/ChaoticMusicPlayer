#include "backend/youtube/ytmusicclient/ytmusicclient.h"

#include "backend/network/httpclient/httpclient.h"

#include <nlohmann/json.hpp>

#include <cctype>
#include <algorithm>
#include <regex>
#include <variant>

using nlohmann::json;

namespace {

constexpr const char *ApiBase = "https://music.youtube.com/youtubei/v1/";
constexpr const char *ClientVersion = "1.20241023.01.00";
// Search filter "songs only" (as used by the YouTube Music website).
// Search filters (the "params" music.youtube.com sends for each search tab)
constexpr const char *SongsFilter = "EgWKAQIIAWoMEA4QChADEAQQCRAF";
constexpr const char *AlbumsFilter = "EgWKAQIYAWoMEA4QChADEAQQCRAF";
constexpr const char *ArtistsFilter = "EgWKAQIgAWoMEA4QChADEAQQCRAF";

// The Android app's client: its lyrics pages include timings, the website's don't.
constexpr const char *AndroidClientVersion = "7.21.50";

std::optional<json> call(const std::string &endpoint, json body, bool asAndroidApp = false)
{
    std::vector<std::string> headers = {"Content-Type: application/json"};
    if (asAndroidApp) {
        body["context"] = {
            {"client", {{"clientName", "ANDROID_MUSIC"}, {"clientVersion", AndroidClientVersion},
                        {"androidSdkVersion", 34}, {"hl", "en"}, {"gl", "US"}}},
        };
        headers.push_back(std::string("User-Agent: com.google.android.apps.youtube.music/") + AndroidClientVersion
                          + " (Linux; U; Android 14) gzip");
    } else {
        body["context"] = {
            {"client", {{"clientName", "WEB_REMIX"}, {"clientVersion", ClientVersion}, {"hl", "en"}, {"gl", "US"}}},
        };
        headers.insert(headers.end(), {
            "Origin: https://music.youtube.com",
            "Referer: https://music.youtube.com/",
            "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:130.0) Gecko/20100101 Firefox/130.0",
        });
    }
    const HttpResponse response =
        HttpClient::post(std::string(ApiBase) + endpoint + "?prettyPrint=false", body.dump(), headers, 15000);
    if (!response.ok())
        return std::nullopt;
    json parsed = json::parse(response.body, nullptr, false);
    if (parsed.is_discarded() || !parsed.is_object())
        return std::nullopt;
    return parsed;
}

// Follows a path of object keys / array indices; nullptr if any step is missing.
const json *at(const json &root, std::initializer_list<std::variant<const char *, size_t>> path)
{
    const json *node = &root;
    for (const auto &step : path) {
        if (std::holds_alternative<const char *>(step)) {
            const char *key = std::get<const char *>(step);
            if (!node->is_object() || !node->contains(key))
                return nullptr;
            node = &(*node)[key];
        } else {
            const size_t index = std::get<size_t>(step);
            if (!node->is_array() || index >= node->size())
                return nullptr;
            node = &(*node)[index];
        }
    }
    return node;
}

std::string text(const json *node)
{
    return node && node->is_string() ? node->get<std::string>() : std::string();
}

// Every object stored under `key`, anywhere in the tree (YouTube's nesting changes often).
void collect(const json &node, const char *key, std::vector<const json *> &out)
{
    if (node.is_object()) {
        for (auto it = node.begin(); it != node.end(); ++it) {
            if (it.key() == key)
                out.push_back(&it.value());
            collect(it.value(), key, out);
        }
    } else if (node.is_array()) {
        for (const json &child : node)
            collect(child, key, out);
    }
}

// "3:41" / "1:02:03" -> ms; 0 if it is not a duration
int64_t parseDuration(const std::string &s)
{
    int64_t total = 0;
    int parts = 0;
    size_t start = 0;
    for (;;) {
        const size_t colon = s.find(':', start);
        const std::string part = s.substr(start, colon - start);
        if (part.empty() || part.size() > 2)
            return 0;
        for (char c : part) {
            if (!std::isdigit(static_cast<unsigned char>(c)))
                return 0;
        }
        total = total * 60 + std::stoi(part);
        ++parts;
        if (colon == std::string::npos)
            break;
        start = colon + 1;
    }
    return parts >= 2 && parts <= 3 ? total * 1000 : 0;
}

std::string pageType(const json &run)
{
    return text(at(run, {"navigationEndpoint", "browseEndpoint", "browseEndpointContextSupportedConfigs",
                         "browseEndpointContextMusicConfig", "pageType"}));
}

// Largest thumbnail, asked for at a bigger size.
std::string largestThumbnail(const json *thumbs)
{
    if (!thumbs || !thumbs->is_array() || thumbs->empty())
        return {};
    std::string url = text(at(thumbs->back(), {"url"}));
    static const std::regex size(R"(=w\d+-h\d+)");
    return std::regex_replace(url, size, "=w544-h544");
}

std::optional<YtSong> parseSong(const json &item)
{
    const json *columns = at(item, {"flexColumns"});
    if (!columns || !columns->is_array() || columns->size() < 2)
        return std::nullopt;

    YtSong song;
    const json *titleRuns = at(*columns, {size_t(0), "musicResponsiveListItemFlexColumnRenderer", "text", "runs"});
    song.title = text(at(item, {"flexColumns", size_t(0), "musicResponsiveListItemFlexColumnRenderer", "text",
                                "runs", size_t(0), "text"}));
    song.videoId = text(at(item, {"playlistItemData", "videoId"}));
    if (song.videoId.empty() && titleRuns)
        song.videoId = text(at(*titleRuns, {size_t(0), "navigationEndpoint", "watchEndpoint", "videoId"}));
    if (song.videoId.empty() || song.title.empty())
        return std::nullopt;

    // Second column: "Artist & Artist • Album • 3:41"
    const json *runs = at(*columns, {size_t(1), "musicResponsiveListItemFlexColumnRenderer", "text", "runs"});
    std::string unlinkedArtists; // artists without a link come before the first " • "
    bool beforeFirstDot = true;
    if (runs && runs->is_array()) {
        for (const json &run : *runs) {
            const std::string value = text(at(run, {"text"}));
            const std::string type = pageType(run);
            if (value == " • ") {
                beforeFirstDot = false;
            } else if (type == "MUSIC_PAGE_TYPE_ARTIST") {
                song.artists.push_back(value);
            } else if (type == "MUSIC_PAGE_TYPE_ALBUM") {
                song.album = value;
                song.albumId = text(at(run, {"navigationEndpoint", "browseEndpoint", "browseId"}));
            } else if (const int64_t ms = parseDuration(value); ms > 0) {
                song.durationMs = ms;
            } else if (beforeFirstDot && value != " & " && value != ", ") {
                unlinkedArtists += value;
            }
        }
    }
    if (song.artists.empty() && !unlinkedArtists.empty())
        song.artists.push_back(unlinkedArtists);

    // Playlist / album rows keep the duration in a separate column.
    if (song.durationMs == 0)
        song.durationMs = parseDuration(text(at(item, {"fixedColumns", size_t(0),
            "musicResponsiveListItemFixedColumnRenderer", "text", "runs", size_t(0), "text"})));

    song.thumbnailUrl = largestThumbnail(at(item, {"thumbnail", "musicThumbnailRenderer", "thumbnail", "thumbnails"}));
    return song;
}

// An "up next" entry: title, "Artist • Album • 2024" byline, length.
std::optional<YtSong> parsePanelSong(const json &item)
{
    YtSong song;
    song.videoId = text(at(item, {"videoId"}));
    song.title = text(at(item, {"title", "runs", size_t(0), "text"}));
    if (song.videoId.empty() || song.title.empty())
        return std::nullopt;
    if (const json *runs = at(item, {"longBylineText", "runs"}); runs && runs->is_array()) {
        std::string unlinkedArtists; // artists without a link come before the first " • "
        bool beforeFirstDot = true;
        for (const json &run : *runs) {
            const std::string value = text(at(run, {"text"}));
            const std::string type = pageType(run);
            if (value == " • ") {
                beforeFirstDot = false;
            } else if (type == "MUSIC_PAGE_TYPE_ARTIST") {
                song.artists.push_back(value);
            } else if (type == "MUSIC_PAGE_TYPE_ALBUM") {
                song.album = value;
                song.albumId = text(at(run, {"navigationEndpoint", "browseEndpoint", "browseId"}));
            } else if (beforeFirstDot && value != " & " && value != ", ") {
                unlinkedArtists += value;
            }
        }
        if (song.artists.empty() && !unlinkedArtists.empty())
            song.artists.push_back(unlinkedArtists);
    }
    song.durationMs = parseDuration(text(at(item, {"lengthText", "runs", size_t(0), "text"})));
    song.thumbnailUrl = largestThumbnail(at(item, {"thumbnail", "thumbnails"}));
    return song;
}

std::string runsText(const json *runs)
{
    std::string out;
    if (runs && runs->is_array()) {
        for (const json &run : *runs)
            out += text(at(run, {"text"}));
    }
    return out;
}

// Carousels of cards (home page, artist pages): playlists, albums, artists.
std::vector<YtShelf> parseShelves(const json &response)
{
    std::vector<const json *> shelves;
    collect(response, "musicCarouselShelfRenderer", shelves);
    std::vector<YtShelf> result;
    for (const json *shelfJson : shelves) {
        YtShelf shelf;
        shelf.title = runsText(at(*shelfJson, {"header", "musicCarouselShelfBasicHeaderRenderer", "title", "runs"}));
        const json *contents = at(*shelfJson, {"contents"});
        if (!contents || !contents->is_array())
            continue;
        for (const json &entry : *contents) {
            const json *card = at(entry, {"musicTwoRowItemRenderer"});
            if (!card)
                continue;
            YtCollection item;
            item.browseId = text(at(*card, {"navigationEndpoint", "browseEndpoint", "browseId"}));
            if (item.browseId.empty())
                continue; // single songs / videos: not a collection
            item.title = runsText(at(*card, {"title", "runs"}));
            item.subtitle = runsText(at(*card, {"subtitle", "runs"}));
            item.thumbnailUrl = largestThumbnail(at(*card, {"thumbnailRenderer", "musicThumbnailRenderer",
                                                            "thumbnail", "thumbnails"}));
            shelf.items.push_back(std::move(item));
        }
        if (!shelf.items.empty())
            result.push_back(std::move(shelf));
    }
    return result;
}

} // namespace

std::optional<std::vector<YtSong>> YtMusicClient::searchSongs(const std::string &query)
{
    const auto response = call("search", {{"query", query}, {"params", SongsFilter}});
    if (!response)
        return std::nullopt;

    std::vector<const json *> items;
    collect(*response, "musicResponsiveListItemRenderer", items);
    std::vector<YtSong> songs;
    for (const json *item : items) {
        if (auto song = parseSong(*item))
            songs.push_back(std::move(*song));
    }
    return songs;
}

std::optional<std::string> YtMusicClient::albumYear(const std::string &albumId)
{
    if (albumId.empty())
        return std::string();
    const auto response = call("browse", {{"browseId", albumId}});
    if (!response)
        return std::nullopt;

    std::vector<const json *> headers;
    collect(*response, "musicResponsiveHeaderRenderer", headers);
    for (const json *header : headers) {
        const json *runs = at(*header, {"subtitle", "runs"});
        if (!runs || !runs->is_array())
            continue;
        for (const json &run : *runs) {
            const std::string value = text(at(run, {"text"}));
            if (value.size() == 4 && std::all_of(value.begin(), value.end(),
                                                 [](unsigned char c) { return std::isdigit(c) != 0; }))
                return value;
        }
    }
    return std::string();
}

std::optional<YtLyrics> YtMusicClient::lyrics(const std::string &videoId)
{
    const auto next = call("next", {{"videoId", videoId}});
    if (!next)
        return std::nullopt;

    // The "Lyrics" tab links to a browse id starting with "MPLY".
    std::vector<const json *> endpoints;
    collect(*next, "browseEndpoint", endpoints);
    std::string browseId;
    for (const json *endpoint : endpoints) {
        const std::string id = text(at(*endpoint, {"browseId"}));
        if (id.rfind("MPLY", 0) == 0) {
            browseId = id;
            break;
        }
    }
    if (browseId.empty())
        return YtLyrics{}; // this song has no lyrics on YouTube Music

    // Timed lyrics first (as the Android app asks for them).
    if (const auto timedPage = call("browse", {{"browseId", browseId}}, true)) {
        std::vector<const json *> models;
        collect(*timedPage, "timedLyricsModel", models);
        for (const json *model : models) {
            const json *lines = at(*model, {"lyricsData", "timedLyricsData"});
            if (!lines || !lines->is_array())
                continue;
            YtLyrics result;
            for (const json &line : *lines) {
                const std::string start = text(at(line, {"cueRange", "startTimeMilliseconds"}));
                if (start.empty() || !std::isdigit(static_cast<unsigned char>(start[0])))
                    continue;
                result.timed.push_back({std::stoll(start), text(at(line, {"lyricLine"}))});
            }
            result.credit = text(at(*model, {"lyricsData", "sourceMessage"}));
            if (!result.timed.empty())
                return result;
        }
    }

    // Otherwise the plain text the website shows.
    const auto page = call("browse", {{"browseId", browseId}});
    if (!page)
        return std::nullopt;

    std::vector<const json *> shelves;
    collect(*page, "musicDescriptionShelfRenderer", shelves);
    YtLyrics result;
    for (const json *shelf : shelves) {
        const json *runs = at(*shelf, {"description", "runs"});
        if (!runs || !runs->is_array())
            continue;
        for (const json &run : *runs)
            result.text += text(at(run, {"text"}));
        if (const json *footer = at(*shelf, {"footer", "runs"}); footer && footer->is_array()) {
            for (const json &run : *footer)
                result.credit += text(at(run, {"text"}));
        }
        break;
    }
    return result;
}

std::string YtMusicClient::watchUrl(const std::string &videoId)
{
    return "https://music.youtube.com/watch?v=" + videoId;
}

std::optional<std::vector<YtShelf>> YtMusicClient::home()
{
    const auto response = call("browse", {{"browseId", "FEmusic_home"}});
    if (!response)
        return std::nullopt;
    return parseShelves(*response);
}

std::optional<std::vector<YtCollection>> YtMusicClient::searchCollections(const std::string &query, bool artists)
{
    const auto response = call("search", {{"query", query}, {"params", artists ? ArtistsFilter : AlbumsFilter}});
    if (!response)
        return std::nullopt;
    std::vector<const json *> items;
    collect(*response, "musicResponsiveListItemRenderer", items);
    std::vector<YtCollection> out;
    for (const json *item : items) {
        YtCollection c;
        c.browseId = text(at(*item, {"navigationEndpoint", "browseEndpoint", "browseId"}));
        const bool isArtist = c.browseId.rfind("UC", 0) == 0;
        if (c.browseId.empty() || isArtist != artists)
            continue;
        c.title = runsText(at(*item, {"flexColumns", size_t(0), "musicResponsiveListItemFlexColumnRenderer", "text", "runs"}));
        c.subtitle = runsText(at(*item, {"flexColumns", size_t(1), "musicResponsiveListItemFlexColumnRenderer", "text", "runs"}));
        c.thumbnailUrl = largestThumbnail(at(*item, {"thumbnail", "musicThumbnailRenderer", "thumbnail", "thumbnails"}));
        if (!c.title.empty())
            out.push_back(std::move(c));
    }
    return out;
}

std::optional<YtArtist> YtMusicClient::artist(const std::string &browseId)
{
    const auto response = call("browse", {{"browseId", browseId}});
    if (!response)
        return std::nullopt;
    YtArtist artist;
    for (const char *header : {"musicImmersiveHeaderRenderer", "musicVisualHeaderRenderer"}) {
        std::vector<const json *> found;
        collect(*response, header, found);
        if (found.empty())
            continue;
        artist.name = runsText(at(*found.front(), {"title", "runs"}));
        artist.thumbnailUrl = largestThumbnail(at(*found.front(), {"thumbnail", "musicThumbnailRenderer", "thumbnail",
                                                                   "thumbnails"}));
        break;
    }
    // Top songs: the first list on the page
    std::vector<const json *> lists;
    collect(*response, "musicShelfRenderer", lists);
    if (!lists.empty()) {
        std::vector<const json *> items;
        collect(*lists.front(), "musicResponsiveListItemRenderer", items);
        for (const json *item : items)
            if (auto song = parseSong(*item))
                artist.songs.push_back(std::move(*song));
    }
    artist.shelves = parseShelves(*response);
    return artist;
}

std::optional<std::vector<YtSong>> YtMusicClient::upNext(const std::string &videoId)
{
    // The song's radio, as music.youtube.com asks for it when a song starts.
    const auto response = call("next", {{"videoId", videoId},
                                        {"playlistId", "RDAMVM" + videoId},
                                        {"enablePersistentPlaylistPanel", true},
                                        {"isAudioOnly", true},
                                        {"tunerSettingValue", "AUTOMIX_SETTING_NORMAL"}});
    if (!response)
        return std::nullopt;
    // The list's own entries only: each may also hold its music video version
    // ("counterpart"), which is not a separate song.
    std::vector<const json *> panels;
    collect(*response, "playlistPanelRenderer", panels);
    std::vector<YtSong> songs;
    for (const json *panel : panels) {
        const json *contents = at(*panel, {"contents"});
        if (!contents || !contents->is_array())
            continue;
        for (const json &entry : *contents) {
            const json *item = at(entry, {"playlistPanelVideoRenderer"});
            if (!item)
                item = at(entry, {"playlistPanelVideoWrapperRenderer", "primaryRenderer", "playlistPanelVideoRenderer"});
            if (item)
                if (auto song = parsePanelSong(*item))
                    songs.push_back(std::move(*song));
        }
        if (!songs.empty())
            break;
    }
    return songs;
}

std::optional<std::vector<YtSong>> YtMusicClient::collection(const std::string &browseId)
{
    const auto response = call("browse", {{"browseId", browseId}});
    if (!response)
        return std::nullopt;

    // Album pages name the album / artist / cover once, in the header.
    std::string headerTitle, headerArtist, headerThumb;
    std::vector<const json *> headers;
    collect(*response, "musicResponsiveHeaderRenderer", headers);
    if (!headers.empty()) {
        headerTitle = runsText(at(*headers.front(), {"title", "runs"}));
        headerArtist = runsText(at(*headers.front(), {"straplineTextOne", "runs"}));
        headerThumb = largestThumbnail(at(*headers.front(), {"thumbnail", "musicThumbnailRenderer",
                                                              "thumbnail", "thumbnails"}));
    }
    const bool isAlbum = browseId.rfind("MPREb", 0) == 0;

    std::vector<const json *> items;
    collect(*response, "musicResponsiveListItemRenderer", items);
    std::vector<YtSong> songs;
    for (const json *item : items) {
        auto song = parseSong(*item);
        if (!song)
            continue;
        if (isAlbum) {
            if (song->album.empty())
                song->album = headerTitle;
            if (song->artists.empty() && !headerArtist.empty())
                song->artists.push_back(headerArtist);
            if (song->thumbnailUrl.empty())
                song->thumbnailUrl = headerThumb;
        }
        songs.push_back(std::move(*song));
    }
    return songs;
}
