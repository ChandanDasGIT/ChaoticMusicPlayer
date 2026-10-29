#include "backend/library/metadata/metadata.h"

#include <taglib/audioproperties.h>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/tpropertymap.h>
#include <taglib/tvariant.h>

#include "backend/audioengine/decoder/decoder.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string_view>
#include <system_error>

namespace {

std::string trimmed(const std::string &s)
{
    const auto begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos)
        return {};
    const auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(begin, end - begin + 1);
}

std::string toUtf8(const TagLib::String &s)
{
    return trimmed(s.to8Bit(true));
}

std::string firstValue(const TagLib::PropertyMap &props, const char *key)
{
    const TagLib::StringList values = props.value(key);
    return values.isEmpty() ? std::string() : toUtf8(values.front());
}

// Multi-value tags (artists, genres, ...) may come as several values or as
// one "A;B" value; both become "A, B".
std::string joinList(const TagLib::StringList &values)
{
    std::string result;
    for (const TagLib::String &value : values) {
        std::string part;
        for (char c : value.to8Bit(true)) {
            if (c == ';') {
                part = trimmed(part);
                if (!part.empty())
                    result += (result.empty() ? "" : ", ") + part;
                part.clear();
            } else {
                part += c;
            }
        }
        part = trimmed(part);
        if (!part.empty())
            result += (result.empty() ? "" : ", ") + part;
    }
    return result;
}

std::string joinArtists(const TagLib::PropertyMap &props)
{
    return joinList(props.value("ARTIST"));
}

int parseInt(const std::string &s)
{
    int value = 0;
    std::from_chars(s.data(), s.data() + s.size(), value);
    return value > 0 ? value : 0;
}

// "3/10" -> number 3, total 10
void parseNumberPair(const std::string &s, int &number, int &total)
{
    const auto slash = s.find('/');
    number = parseInt(trimmed(s.substr(0, slash)));
    if (slash != std::string::npos)
        total = parseInt(trimmed(s.substr(slash + 1)));
}

bool isSeparator(char c)
{
    return c == ' ' || c == '.' || c == '-' || c == '_';
}

// Fills in title / track number from names like "003   Song" or "03 - Song".
void applyFileNameFallbacks(const std::filesystem::path &path, Track &track)
{
    const std::string stem = path.stem().string();
    const bool titleFromFileName = track.title.empty();
    if (titleFromFileName)
        track.title = stem;

    if (track.track != 0)
        return;

    size_t digits = 0;
    while (digits < stem.size() && std::isdigit(static_cast<unsigned char>(stem[digits])))
        ++digits;
    if (digits == 0 || digits > 3 || digits >= stem.size() || !isSeparator(stem[digits]))
        return;

    size_t rest = digits;
    while (rest < stem.size() && isSeparator(stem[rest]))
        ++rest;
    if (rest == stem.size())
        return; // nothing after the number: the number *is* the title

    track.track = parseInt(stem.substr(0, digits));
    if (titleFromFileName)
        track.title = stem.substr(rest);
}

std::string joinValues(const TagLib::StringList &values, const char *separator)
{
    std::string result;
    for (const TagLib::String &value : values) {
        const std::string v = toUtf8(value);
        if (!v.empty())
            result += (result.empty() ? "" : separator) + v;
    }
    return result;
}

std::string formatDuration(int64_t ms)
{
    const int64_t s = ms / 1000;
    char buffer[32];
    if (s >= 3600)
        std::snprintf(buffer, sizeof(buffer), "%lld:%02lld:%02lld",
                      (long long)(s / 3600), (long long)(s / 60 % 60), (long long)(s % 60));
    else
        std::snprintf(buffer, sizeof(buffer), "%lld:%02lld", (long long)(s / 60), (long long)(s % 60));
    return buffer;
}

std::string formatSize(uintmax_t bytes)
{
    char buffer[32];
    if (bytes >= 1024 * 1024)
        std::snprintf(buffer, sizeof(buffer), "%.1f MB", double(bytes) / (1024 * 1024));
    else
        std::snprintf(buffer, sizeof(buffer), "%.1f KB", double(bytes) / 1024);
    return buffer;
}

void addField(std::vector<TrackDetails::Field> &fields, const char *label, const std::string &value)
{
    if (!value.empty())
        fields.push_back({label, value});
}

// "3" + total "10" -> "3 of 10"
std::string numberOf(const std::string &pair, const std::string &extraTotal)
{
    int number = 0, total = 0;
    parseNumberPair(pair, number, total);
    if (total == 0)
        total = parseInt(extraTotal);
    if (number == 0)
        return {};
    return total > 0 ? std::to_string(number) + " of " + std::to_string(total) : std::to_string(number);
}

std::string lowercase(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    return s;
}

std::vector<unsigned char> readEmbeddedCover(const std::filesystem::path &path)
{
    TagLib::FileRef file(path.c_str(), false);
    if (file.isNull())
        return {};

    const TagLib::List<TagLib::VariantMap> pictures = file.complexProperties("PICTURE");
    const TagLib::VariantMap *chosen = nullptr;
    for (const TagLib::VariantMap &picture : pictures) {
        if (!chosen)
            chosen = &picture;
        if (picture.value("pictureType").toString() == "Front Cover") {
            chosen = &picture;
            break;
        }
    }
    if (!chosen)
        return {};

    const TagLib::ByteVector data = chosen->value("data").toByteVector();
    return {data.begin(), data.end()};
}

std::vector<unsigned char> readFolderCover(const std::filesystem::path &path)
{
    constexpr std::array<std::string_view, 4> Names = {"cover", "folder", "front", "albumart"};
    constexpr std::array<std::string_view, 3> Extensions = {".jpg", ".jpeg", ".png"};

    std::error_code ec;
    for (const auto &entry : std::filesystem::directory_iterator(path.parent_path(), ec)) {
        const std::string stem = lowercase(entry.path().stem().string());
        const std::string ext = lowercase(entry.path().extension().string());
        if (std::find(Names.begin(), Names.end(), stem) == Names.end()
            || std::find(Extensions.begin(), Extensions.end(), ext) == Extensions.end()
            || !entry.is_regular_file(ec))
            continue;

        std::ifstream in(entry.path(), std::ios::binary);
        return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    }
    return {};
}

} // namespace

Track Metadata::read(const std::filesystem::path &path)
{
    Track track;
    track.path = path.string();

    TagLib::FileRef file(path.c_str(), true, TagLib::AudioProperties::Average);
    if (!file.isNull()) {
        if (const TagLib::Tag *tag = file.tag()) {
            track.title = toUtf8(tag->title());
            track.album = toUtf8(tag->album());
        }

        const TagLib::PropertyMap props = file.properties();
        track.artist = joinArtists(props);
        track.albumArtist = joinList(props.value("ALBUMARTIST"));

        int trackTotal = 0;
        parseNumberPair(firstValue(props, "TRACKNUMBER"), track.track, trackTotal);
        parseNumberPair(firstValue(props, "DISCNUMBER"), track.disc, track.discTotal);
        if (track.discTotal == 0)
            track.discTotal = parseInt(firstValue(props, "DISCTOTAL"));
        if (track.discTotal == 0)
            track.discTotal = parseInt(firstValue(props, "TOTALDISCS"));

        if (const TagLib::AudioProperties *audio = file.audioProperties())
            track.durationMs = audio->lengthInMilliseconds();

        track.hasCover = file.complexPropertyKeys().contains("PICTURE");
    }

    applyFileNameFallbacks(path, track);
    return track;
}

TrackDetails Metadata::readDetails(const std::filesystem::path &path)
{
    TrackDetails details;

    // Tags
    TagLib::FileRef file(path.c_str(), false);
    if (!file.isNull()) {
        const TagLib::PropertyMap props = file.properties();
        const auto first = [&](const char *key) { return firstValue(props, key); };
        auto firstOf = [&](const char *a, const char *b) {
            const std::string v = first(a);
            return v.empty() ? first(b) : v;
        };

        addField(details.tags, "Title", first("TITLE"));
        addField(details.tags, "Artist", joinArtists(props));
        addField(details.tags, "Album", first("ALBUM"));
        addField(details.tags, "Album artist", joinList(props.value("ALBUMARTIST")));
        addField(details.tags, "Track", numberOf(first("TRACKNUMBER"), firstOf("TRACKTOTAL", "TOTALTRACKS")));
        addField(details.tags, "Disc", numberOf(first("DISCNUMBER"), firstOf("DISCTOTAL", "TOTALDISCS")));
        addField(details.tags, "Year", first("DATE"));
        addField(details.tags, "Genre", joinList(props.value("GENRE")));
        addField(details.tags, "Composer", joinList(props.value("COMPOSER")));
        addField(details.tags, "Cover art",
                 file.complexPropertyKeys().contains("PICTURE") ? "Embedded" : "None");

        for (const auto &[key, values] : props)
            addField(details.rawTags, toUtf8(key).c_str(), joinValues(values, "; "));
    }

    // Audio
    AudioInfo info;
    if (Decoder::probe(path.string(), info)) {
        addField(details.audio, "Format", info.format);
        addField(details.audio, "Codec", info.codec);
        if (info.bitrate > 0)
            addField(details.audio, "Bitrate", std::to_string((info.bitrate + 500) / 1000) + " kbps");
        if (info.sampleRate > 0) {
            char rate[32];
            std::snprintf(rate, sizeof(rate), "%g kHz", info.sampleRate / 1000.0);
            addField(details.audio, "Sample rate", rate);
        }
        if (info.bitsPerSample > 0)
            addField(details.audio, "Bit depth", std::to_string(info.bitsPerSample) + "-bit");
        if (info.channels > 0)
            addField(details.audio, "Channels",
                     std::to_string(info.channels)
                         + (info.channelLayout.empty() ? "" : " (" + info.channelLayout + ")"));
        if (info.durationMs > 0)
            addField(details.audio, "Duration", formatDuration(info.durationMs));
    }

    // File
    std::error_code ec;
    addField(details.file, "File name", path.filename().string());
    addField(details.file, "Folder", path.parent_path().string());
    const uintmax_t size = std::filesystem::file_size(path, ec);
    if (!ec)
        addField(details.file, "Size", formatSize(size));

    return details;
}

std::vector<unsigned char> Metadata::readCover(const std::filesystem::path &path)
{
    std::vector<unsigned char> cover = readEmbeddedCover(path);
    return cover.empty() ? readFolderCover(path) : cover;
}

namespace {

// "-6.54 dB" / "0.98765" -> the number (false if there is none)
bool leadingNumber(const std::string &text, double &value)
{
    try {
        size_t used = 0;
        value = std::stod(text, &used);
        return used > 0;
    } catch (...) {
        return false;
    }
}

} // namespace

ReplayGain Metadata::readReplayGain(const std::filesystem::path &path)
{
    ReplayGain gain;
    TagLib::FileRef file(path.c_str(), false);
    if (file.isNull())
        return gain;
    const TagLib::PropertyMap props = file.properties();
    gain.hasTrack = leadingNumber(firstValue(props, "REPLAYGAIN_TRACK_GAIN"), gain.trackGainDb);
    gain.hasAlbum = leadingNumber(firstValue(props, "REPLAYGAIN_ALBUM_GAIN"), gain.albumGainDb);
    leadingNumber(firstValue(props, "REPLAYGAIN_TRACK_PEAK"), gain.trackPeak);
    leadingNumber(firstValue(props, "REPLAYGAIN_ALBUM_PEAK"), gain.albumPeak);
    // Opus: R128 gains in 1/256 dB, against -23 LUFS (ReplayGain's reference is 5 dB louder)
    double r128 = 0;
    if (!gain.hasTrack && leadingNumber(firstValue(props, "R128_TRACK_GAIN"), r128)) {
        gain.hasTrack = true;
        gain.trackGainDb = r128 / 256.0 + 5.0;
    }
    if (!gain.hasAlbum && leadingNumber(firstValue(props, "R128_ALBUM_GAIN"), r128)) {
        gain.hasAlbum = true;
        gain.albumGainDb = r128 / 256.0 + 5.0;
    }
    return gain;
}
