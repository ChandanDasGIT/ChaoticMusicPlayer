#include "backend/youtube/ytdlp/ytdlp.h"

#include "backend/system/process/process.h"

#include <nlohmann/json.hpp>

#include <cmath>

using nlohmann::json;

namespace {

thread_local std::string t_lastError;
thread_local bool t_rateLimited = false;

bool contains(const std::string &text, const char *part)
{
    return text.find(part) != std::string::npos;
}

// yt-dlp's "ERROR: [youtube] <id>: ..." line in words a listener understands.
std::string friendlyError(const std::string &line)
{
    if ((contains(line, "confirm you") && contains(line, "bot")) || contains(line, "429")
        || contains(line, "Too Many Requests")) {
        t_rateLimited = true;
        return "YouTube is limiting requests from your connection for now. "
               "This usually clears up within a few minutes; try again later.";
    }
    if (contains(line, "Requested format is not available") || contains(line, "Only images are available"))
        return "YouTube didn't offer an audio stream for this song. Updating yt-dlp usually fixes this.";
    if (contains(line, "Video unavailable") || contains(line, "Private video")
        || contains(line, "not available"))
        return "This song isn't available on YouTube (removed, private or blocked in your region).";

    // Otherwise yt-dlp's own text, without the "ERROR: [youtube] <id>:" prefix.
    std::string text = line;
    if (text.rfind("ERROR:", 0) == 0)
        text.erase(0, 6);
    if (const auto bracket = text.find(']'); text.find('[') == 1 && bracket != std::string::npos)
        text.erase(0, bracket + 1);
    if (const auto colon = text.find(": "); colon != std::string::npos && colon < 24)
        text.erase(0, colon + 2);
    while (!text.empty() && text.front() == ' ')
        text.erase(0, 1);
    return text.empty() ? "yt-dlp failed" : text;
}

std::vector<std::string> baseArgs(const std::string &cookiesBrowser)
{
    std::vector<std::string> args = {"yt-dlp", "--no-warnings", "--ignore-config"};
    if (!cookiesBrowser.empty()) {
        args.push_back("--cookies-from-browser");
        args.push_back(cookiesBrowser);
    }
    return args;
}

ProcessResult run(std::vector<std::string> args, int timeoutMs)
{
    ProcessResult result = runProcess(args, timeoutMs);
    t_rateLimited = false;
    if (!result.started)
        t_lastError = "yt-dlp is not installed";
    else if (result.timedOut)
        t_lastError = "yt-dlp took too long";
    else if (result.exitCode != 0) {
        // Last "ERROR: ..." line is the useful part.
        const auto pos = result.err.rfind("ERROR:");
        t_lastError = pos != std::string::npos
                          ? friendlyError(result.err.substr(pos, result.err.find('\n', pos) - pos))
                          : "yt-dlp failed";
    }
    return result;
}

std::string str(const json &item, const char *key)
{
    const auto it = item.find(key);
    return it != item.end() && it->is_string() ? it->get<std::string>() : std::string();
}

YtSong songFromInfo(const json &item)
{
    YtSong song;
    song.videoId = str(item, "id");
    song.title = str(item, "track");
    if (song.title.empty())
        song.title = str(item, "title");
    song.album = str(item, "album");
    if (const auto it = item.find("duration"); it != item.end() && it->is_number())
        song.durationMs = int64_t(std::llround(it->get<double>() * 1000));

    if (const auto it = item.find("artists"); it != item.end() && it->is_array()) {
        for (const json &artist : *it) {
            if (artist.is_string())
                song.artists.push_back(artist.get<std::string>());
        }
    }
    for (const char *key : {"artist", "creator", "channel", "uploader"}) {
        if (!song.artists.empty())
            break;
        const std::string value = str(item, key);
        if (!value.empty())
            song.artists.push_back(value.size() > 8 && value.compare(value.size() - 8, 8, " - Topic") == 0
                                       ? value.substr(0, value.size() - 8)
                                       : value);
    }

    if (const auto it = item.find("thumbnails"); it != item.end() && it->is_array() && !it->empty())
        song.thumbnailUrl = str(it->back(), "url");
    if (song.thumbnailUrl.empty())
        song.thumbnailUrl = str(item, "thumbnail");
    if (song.thumbnailUrl.empty() && !song.videoId.empty())
        song.thumbnailUrl = "https://i.ytimg.com/vi/" + song.videoId + "/hqdefault.jpg";
    return song;
}

} // namespace

std::optional<std::string> YtDlp::version()
{
    ProcessResult result = run({"yt-dlp", "--version"}, 10000);
    if (!result.ok())
        return std::nullopt;
    std::string v = result.out;
    while (!v.empty() && (v.back() == '\n' || v.back() == '\r'))
        v.pop_back();
    return v;
}

std::optional<std::string> YtDlp::streamUrl(const std::string &videoId, const std::string &cookiesBrowser)
{
    std::vector<std::string> args = baseArgs(cookiesBrowser);
    // Audio only, preferring Opus/WebM (what YouTube Music serves best).
    // "--": what follows is always a link, never an option (links can come from the user).
    args.insert(args.end(), {"-f", "bestaudio[ext=webm]/bestaudio", "--get-url", "--no-playlist",
                             "--", YtMusicClient::watchUrl(videoId)});
    ProcessResult result = run(args, 30000);
    if (!result.ok())
        return std::nullopt;
    const std::string url = result.out.substr(0, result.out.find('\n'));
    if (url.rfind("http", 0) != 0) {
        t_lastError = "yt-dlp returned no stream";
        return std::nullopt;
    }
    return url;
}

std::optional<std::vector<YtSong>> YtDlp::entries(const std::string &url, const std::string &cookiesBrowser)
{
    std::vector<std::string> args = baseArgs(cookiesBrowser);
    // "--": a pasted "link" like "--exec ..." must never become an option.
    args.insert(args.end(), {"-J", "--flat-playlist", "--", url});
    ProcessResult result = run(args, 90000);
    if (!result.ok())
        return std::nullopt;

    const json info = json::parse(result.out, nullptr, false);
    if (!info.is_object()) {
        t_lastError = "yt-dlp returned something unexpected";
        return std::nullopt;
    }

    std::vector<YtSong> songs;
    if (const auto it = info.find("entries"); it != info.end() && it->is_array()) {
        for (const json &entry : *it) {
            if (entry.is_object()) {
                YtSong song = songFromInfo(entry);
                if (!song.videoId.empty())
                    songs.push_back(std::move(song));
            }
        }
    } else {
        YtSong song = songFromInfo(info);
        if (!song.videoId.empty())
            songs.push_back(std::move(song));
    }
    return songs;
}

std::optional<std::vector<YtSong>> YtDlp::likedSongs(const std::string &cookiesBrowser)
{
    return entries("https://music.youtube.com/playlist?list=LM", cookiesBrowser);
}

std::string YtDlp::lastError()
{
    return t_lastError;
}

bool YtDlp::lastErrorWasRateLimit()
{
    return t_rateLimited;
}
