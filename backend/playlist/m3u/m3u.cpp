#include "backend/playlist/m3u/m3u.h"

#include "backend/library/metadata/metadata.h"
#include "backend/youtube/streamresolver/streamresolver.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <regex>
#include <sstream>
#include <system_error>
#include <utility>

namespace fs = std::filesystem;

namespace {

bool isValidUtf8(const std::string &s)
{
    for (size_t i = 0; i < s.size();) {
        const auto c = static_cast<unsigned char>(s[i]);
        const size_t length = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 0;
        if (length == 0 || i + length > s.size())
            return false;
        for (size_t k = 1; k < length; ++k) {
            if ((static_cast<unsigned char>(s[i + k]) >> 6) != 0x2)
                return false;
        }
        i += length;
    }
    return true;
}

// Old .m3u files are often Latin-1 (Windows players).
std::string latin1ToUtf8(const std::string &s)
{
    std::string out;
    for (const char ch : s) {
        const auto c = static_cast<unsigned char>(ch);
        if (c < 0x80) {
            out += char(c);
        } else {
            out += char(0xC0 | (c >> 6));
            out += char(0x80 | (c & 0x3F));
        }
    }
    return out;
}

std::string trim(const std::string &s)
{
    const size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return {};
    return s.substr(start, s.find_last_not_of(" \t\r\n") - start + 1);
}

std::string percentDecode(const std::string &s)
{
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size() && std::isxdigit(static_cast<unsigned char>(s[i + 1]))
            && std::isxdigit(static_cast<unsigned char>(s[i + 2]))) {
            out += char(std::stoi(s.substr(i + 1, 2), nullptr, 16));
            i += 2;
        } else {
            out += s[i];
        }
    }
    return out;
}

bool startsWith(const std::string &s, const char *prefix)
{
    return s.rfind(prefix, 0) == 0;
}

struct ExtInf
{
    int64_t durationMs = 0;
    std::string artist;
    std::string title;
};

// "#EXTINF:215,Artist - Title" (attributes such as tvg-id="..." may sit before the comma).
ExtInf parseExtInf(const std::string &line)
{
    ExtInf info;
    const std::string body = line.substr(8); // after "#EXTINF:"
    const size_t comma = body.find(',');
    try {
        const long long seconds = std::stoll(body.substr(0, comma));
        if (seconds > 0)
            info.durationMs = seconds * 1000;
    } catch (...) {
    }
    if (comma == std::string::npos)
        return info;
    const std::string name = trim(body.substr(comma + 1));
    const size_t dash = name.find(" - ");
    if (dash == std::string::npos) {
        info.title = name;
    } else {
        info.artist = trim(name.substr(0, dash));
        info.title = trim(name.substr(dash + 3));
    }
    return info;
}

} // namespace

std::string M3u::videoIdFromUrl(const std::string &url)
{
    // youtube.com/watch?v=ID, music.youtube.com/watch?v=ID, youtu.be/ID, youtube.com/shorts/ID
    static const std::regex pattern(
        R"(^https?://(?:(?:www\.|m\.|music\.)?youtube\.com/(?:watch\?(?:.*&)?v=|shorts/)|youtu\.be/)([A-Za-z0-9_-]{11}))");
    std::smatch match;
    return std::regex_search(url, match, pattern) ? match[1].str() : std::string();
}

M3u::Result M3u::read(const fs::path &file)
{
    Result result;
    std::ifstream in(file, std::ios::binary);
    if (!in)
        return result;
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (startsWith(content, "\xEF\xBB\xBF"))
        content.erase(0, 3); // UTF-8 byte order mark
    if (!isValidUtf8(content))
        content = latin1ToUtf8(content);
    result.ok = true;

    const fs::path folder = file.parent_path();
    ExtInf pending;
    std::istringstream lines(content);
    std::string line;
    while (std::getline(lines, line)) {
        line = trim(line);
        if (line.empty())
            continue;
        if (startsWith(line, "#EXTINF:")) {
            pending = parseExtInf(line);
            continue;
        }
        if (line[0] == '#')
            continue; // #EXTM3U and other directives
        const ExtInf info = std::exchange(pending, ExtInf());

        if (const std::string videoId = videoIdFromUrl(line); !videoId.empty()) {
            Track track;
            track.videoId = videoId;
            track.path = StreamResolver::pathFor(videoId);
            track.title = info.title.empty() ? videoId : info.title;
            track.artist = info.artist;
            track.durationMs = info.durationMs;
            track.thumbnailUrl = "https://i.ytimg.com/vi/" + videoId + "/hqdefault.jpg";
            result.tracks.push_back(std::move(track));
            continue;
        }

        std::string location = line;
        if (startsWith(location, "file://")) {
            location = percentDecode(location.substr(7));
            if (startsWith(location, "localhost/"))
                location.erase(0, 9);
            // "file:///C:/Music/..." -> "C:/Music/..."
            if (location.size() > 3 && location[0] == '/' && std::isalpha(static_cast<unsigned char>(location[1]))
                && location[2] == ':')
                location.erase(0, 1);
        } else if (location.find("://") != std::string::npos) {
            ++result.skipped; // a radio stream or another website
            continue;
        }
#ifndef _WIN32
        // Playlists made on Windows: "\" separates folders there
        std::replace(location.begin(), location.end(), '\\', '/');
#endif
        fs::path path(location);
        if (path.is_relative())
            path = folder / path;
        path = path.lexically_normal();
        std::error_code ec;
        if (!fs::is_regular_file(path, ec)) {
            ++result.missing;
            continue;
        }
        result.tracks.push_back(Metadata::read(path));
    }
    return result;
}

bool M3u::write(const fs::path &file, const std::vector<Track> &tracks)
{
    const fs::path folder = fs::absolute(file).parent_path().lexically_normal();
    const fs::path temp = file.string() + ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out)
            return false;
        out << "#EXTM3U\n";
        for (const Track &track : tracks) {
            const std::string name = track.artist.empty() ? track.title : track.artist + " - " + track.title;
            out << "#EXTINF:" << (track.durationMs > 0 ? track.durationMs / 1000 : -1) << ',' << name << '\n';
            if (!track.videoId.empty()) {
                out << "https://music.youtube.com/watch?v=" << track.videoId << '\n';
                continue;
            }
            const fs::path path = fs::path(track.path).lexically_normal();
            const fs::path relative = path.lexically_relative(folder);
            const bool inside = !relative.empty() && relative.generic_string().rfind("..", 0) != 0;
            out << (inside ? relative.string() : path.string()) << '\n';
        }
        if (!out)
            return false;
    }
    std::error_code ec;
    fs::rename(temp, file, ec);
    return !ec;
}
