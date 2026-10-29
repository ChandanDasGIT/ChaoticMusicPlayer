#include "backend/lyrics/lrcparser/lrcparser.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <optional>
#include <sstream>

namespace {

std::string trimmed(const std::string &s)
{
    const auto begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos)
        return {};
    const auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(begin, end - begin + 1);
}

// "mm:ss", "mm:ss.xx" or "mm:ss.xxx" -> milliseconds
std::optional<int64_t> parseTimestamp(const std::string &tag)
{
    const auto colon = tag.find(':');
    if (colon == std::string::npos || colon == 0)
        return std::nullopt;

    int minutes = 0;
    const char *begin = tag.data();
    auto [ptr, ec] = std::from_chars(begin, begin + colon, minutes);
    if (ec != std::errc() || ptr != begin + colon)
        return std::nullopt;

    const std::string rest = tag.substr(colon + 1);
    const auto dot = rest.find_first_of(".:");
    int seconds = 0;
    const char *secondsEnd = rest.data() + (dot == std::string::npos ? rest.size() : dot);
    const auto secondsResult = std::from_chars(rest.data(), secondsEnd, seconds);
    if (secondsResult.ec != std::errc() || secondsResult.ptr != secondsEnd || seconds > 59)
        return std::nullopt;

    int64_t fractionMs = 0;
    if (dot != std::string::npos) {
        const std::string fraction = rest.substr(dot + 1);
        if (fraction.empty()
            || !std::all_of(fraction.begin(), fraction.end(),
                            [](unsigned char c) { return std::isdigit(c) != 0; }))
            return std::nullopt;
        // ".5" = 500 ms, ".45" = 450 ms, ".456" = 456 ms
        std::string padded = (fraction + "000").substr(0, 3);
        fractionMs = std::stoi(padded);
    }
    return int64_t(minutes) * 60000 + int64_t(seconds) * 1000 + fractionMs;
}

// Removes "<mm:ss.xx>" word-timing tags (enhanced LRC).
std::string stripWordTimings(const std::string &text)
{
    std::string out;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '<') {
            const auto close = text.find('>', i);
            if (close != std::string::npos && parseTimestamp(text.substr(i + 1, close - i - 1))) {
                i = close;
                continue;
            }
        }
        out += text[i];
    }
    return out;
}

// "作词 : 米津玄師", "Composer: …": who wrote / arranged / produced it, not a lyric.
bool isCreditLine(const std::string &text)
{
    static const char *const labels[] = {
        "作词", "作詞", "作曲", "编曲", "編曲", "词", "詞", "曲", "制作人", "製作人", "监制", "監製", "混音", "母带",
        "和声", "和聲", "录音", "錄音", "吉他", "贝斯", "鼓", "弦乐", "出品", "发行", "發行", "制作", "製作",
        "作詞・作曲", "Lyrics", "Lyricist", "Lyricists", "Composer", "Composers", "Arranger", "Arrangement",
        "Producer", "Producers", "Written by", "Music"};
    // The label is everything before the first ":" / "：", if that comes early.
    size_t colon = text.find(':');
    const size_t wide = text.find("\xEF\xBC\x9A"); // "："
    if (wide != std::string::npos && (colon == std::string::npos || wide < colon))
        colon = wide;
    if (colon == std::string::npos || colon > 24)
        return false;
    const std::string label = trimmed(text.substr(0, colon));
    for (const char *known : labels)
        if (label == known)
            return true;
    return false;
}

} // namespace

Lyrics LrcParser::parse(const std::string &text)
{
    Lyrics lyrics;
    std::vector<LyricLine> timed;
    std::vector<std::string> plain;
    int64_t offsetMs = 0;

    std::istringstream stream(text);
    std::string rawLine;
    while (std::getline(stream, rawLine)) {
        std::string line = trimmed(rawLine);

        std::vector<int64_t> times;
        bool isTagLine = false;
        while (!line.empty() && line.front() == '[') {
            const auto close = line.find(']');
            if (close == std::string::npos)
                break;
            const std::string tag = line.substr(1, close - 1);
            if (const auto time = parseTimestamp(tag)) {
                times.push_back(*time);
            } else if (tag.size() > 1 && std::isalpha(static_cast<unsigned char>(tag.front()))
                       && tag.find(':') != std::string::npos) {
                // Metadata tag such as [ar:Artist] or [offset:+250]
                if (tag.rfind("offset:", 0) == 0) {
                    const std::string value = trimmed(tag.substr(7));
                    int parsed = 0;
                    const char *b = value.data() + (!value.empty() && value.front() == '+' ? 1 : 0);
                    if (std::from_chars(b, value.data() + value.size(), parsed).ec == std::errc())
                        offsetMs = parsed;
                }
                isTagLine = true;
            } else {
                break; // "[" that is part of the lyric text
            }
            line = trimmed(line.substr(close + 1));
        }

        if (!times.empty()) {
            const std::string lyric = trimmed(stripWordTimings(line));
            for (int64_t time : times)
                timed.push_back({time, lyric});
        } else if (!isTagLine) {
            plain.push_back(line);
        }
    }

    timed.erase(std::remove_if(timed.begin(), timed.end(), [](const LyricLine &l) { return isCreditLine(l.text); }),
                timed.end());
    plain.erase(std::remove_if(plain.begin(), plain.end(), isCreditLine), plain.end());

    if (!timed.empty()) {
        std::stable_sort(timed.begin(), timed.end(),
                         [](const LyricLine &a, const LyricLine &b) { return a.timeMs < b.timeMs; });

        // A translation under each line: the same time twice, again and again.
        // Keep the first line of each time (the original). Now and then two lines
        // at one time (e.g. a duet) stay as they are.
        size_t times = 0, doubled = 0;
        for (size_t i = 0; i < timed.size(); ++i) {
            if (timed[i].text.empty() || (i > 0 && timed[i - 1].timeMs == timed[i].timeMs))
                continue;
            ++times;
            if (i + 1 < timed.size() && timed[i + 1].timeMs == timed[i].timeMs && !timed[i + 1].text.empty())
                ++doubled;
        }
        if (doubled >= 3 && doubled * 10 >= times * 3) {
            std::vector<LyricLine> originals;
            for (size_t i = 0; i < timed.size(); ++i)
                if (i == 0 || timed[i - 1].timeMs != timed[i].timeMs)
                    originals.push_back(std::move(timed[i]));
            timed = std::move(originals);
        }
        for (LyricLine &line : timed)
            line.timeMs = std::max<int64_t>(0, line.timeMs - offsetMs);
        lyrics.lines = std::move(timed);
        lyrics.synced = true;
        return lyrics;
    }

    // Plain text: drop leading/trailing blank lines, keep the inner ones (verse breaks).
    while (!plain.empty() && plain.front().empty())
        plain.erase(plain.begin());
    while (!plain.empty() && plain.back().empty())
        plain.pop_back();
    for (std::string &line : plain)
        lyrics.lines.push_back({-1, std::move(line)});
    return lyrics;
}
