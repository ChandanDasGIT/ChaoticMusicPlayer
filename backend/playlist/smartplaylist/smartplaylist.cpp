#include "backend/playlist/smartplaylist/smartplaylist.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <fstream>
#include <random>
#include <system_error>

#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using nlohmann::json;

namespace {

constexpr int64_t Day = 24 * 3600;

std::string lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return s;
}

double number(const std::string &s)
{
    try {
        return s.empty() ? 0 : std::stod(s);
    } catch (...) {
        return 0;
    }
}

// The starter playlists, made the first time.
std::vector<SmartPlaylist> starters()
{
    return {
        {"most-played", "Most played", true, {{"plays", "greater", "2"}}, "plays", 50},
        {"not-lately", "Not played in a while", true, {{"plays", "greater", "0"}, {"lastPlayed", "notInLast", "30"}},
         "lastPlayed", 50},
        {"recently-added", "Recently added", true, {{"added", "inLast", "30"}}, "added", 100},
        {"never-played", "Never played", true, {{"plays", "equals", "0"}, {"source", "is", "local"}}, "random", 50},
    };
}

bool matches(const SmartRule &rule, const SmartCandidate &song, int64_t now)
{
    const Track &t = song.track;
    if (rule.field == "title" || rule.field == "artist" || rule.field == "album") {
        const std::string text = lower(rule.field == "title" ? t.title : rule.field == "artist" ? t.artist : t.album);
        const std::string value = lower(rule.value);
        if (rule.op == "contains")
            return text.find(value) != std::string::npos;
        if (rule.op == "notContains")
            return text.find(value) == std::string::npos;
        if (rule.op == "is")
            return text == value;
        if (rule.op == "isNot")
            return text != value;
        return false;
    }
    if (rule.field == "plays" || rule.field == "duration") {
        const double have = rule.field == "plays" ? song.plays : double(t.durationMs) / 60000.0;
        const double want = number(rule.value);
        if (rule.op == "greater")
            return have > want;
        if (rule.op == "less")
            return have < want;
        if (rule.op == "equals")
            return rule.field == "plays" ? int(have) == int(want) : std::abs(have - want) < 0.5;
        return false;
    }
    if (rule.field == "lastPlayed" || rule.field == "added") {
        const int64_t when = rule.field == "lastPlayed" ? song.lastPlayed : t.addedAt;
        const bool recent = when > 0 && when >= now - int64_t(number(rule.value) * Day);
        if (rule.op == "inLast")
            return recent;
        if (rule.op == "notInLast")
            return !recent; // also never
        return false;
    }
    if (rule.field == "source")
        return (rule.value == "youtube") == song.youTube;
    return false;
}

json toJson(const SmartPlaylist &p)
{
    json rules = json::array();
    for (const SmartRule &r : p.rules)
        rules.push_back({{"field", r.field}, {"op", r.op}, {"value", r.value}});
    return {{"id", p.id}, {"name", p.name}, {"matchAll", p.matchAll}, {"rules", rules}, {"order", p.order},
            {"limit", p.limit}};
}

SmartPlaylist fromJson(const json &j)
{
    SmartPlaylist p;
    p.id = j.value("id", "");
    p.name = j.value("name", "");
    p.matchAll = j.value("matchAll", true);
    p.order = j.value("order", "plays");
    p.limit = j.value("limit", 0);
    if (const auto it = j.find("rules"); it != j.end() && it->is_array())
        for (const json &r : *it)
            p.rules.push_back({r.value("field", ""), r.value("op", ""), r.value("value", "")});
    return p;
}

} // namespace

SmartPlaylistStore::SmartPlaylistStore(fs::path file)
    : m_file(std::move(file))
{
}

void SmartPlaylistStore::load()
{
    m_playlists.clear();
    std::ifstream in(m_file);
    if (!in) {
        m_playlists = starters();
        save();
        return;
    }
    const json root = json::parse(in, nullptr, false);
    if (root.is_discarded() || !root.contains("playlists") || !root["playlists"].is_array())
        return; // damaged: start empty rather than overwrite it with starters
    for (const json &j : root["playlists"]) {
        SmartPlaylist p = fromJson(j);
        if (!p.id.empty())
            m_playlists.push_back(std::move(p));
    }
}

bool SmartPlaylistStore::save() const
{
    json list = json::array();
    for (const SmartPlaylist &p : m_playlists)
        list.push_back(toJson(p));
    std::error_code ec;
    fs::create_directories(m_file.parent_path(), ec);
    // Write to a temp file, then rename: a crash never leaves a half-written file.
    const fs::path temp = m_file.string() + ".tmp";
    {
        std::ofstream out(temp, std::ios::trunc);
        if (!out)
            return false;
        out << json{{"version", 1}, {"playlists", list}}.dump(2) << '\n';
        if (!out)
            return false;
    }
    fs::rename(temp, m_file, ec);
    return !ec;
}

const SmartPlaylist *SmartPlaylistStore::find(const std::string &id) const
{
    const auto it = std::find_if(m_playlists.begin(), m_playlists.end(), [&](const SmartPlaylist &p) { return p.id == id; });
    return it == m_playlists.end() ? nullptr : &*it;
}

std::string SmartPlaylistStore::put(SmartPlaylist playlist)
{
    if (playlist.id.empty()) {
        const auto now = std::chrono::system_clock::now().time_since_epoch();
        playlist.id = "smart-" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
    }
    const auto it = std::find_if(m_playlists.begin(), m_playlists.end(),
                                 [&](const SmartPlaylist &p) { return p.id == playlist.id; });
    const std::string id = playlist.id;
    if (it == m_playlists.end())
        m_playlists.push_back(std::move(playlist));
    else
        *it = std::move(playlist);
    save();
    return id;
}

void SmartPlaylistStore::remove(const std::string &id)
{
    const auto it = std::remove_if(m_playlists.begin(), m_playlists.end(), [&](const SmartPlaylist &p) { return p.id == id; });
    if (it == m_playlists.end())
        return;
    m_playlists.erase(it, m_playlists.end());
    save();
}

std::vector<Track> evaluate(const SmartPlaylist &playlist, const std::vector<SmartCandidate> &candidates, int64_t now)
{
    std::vector<const SmartCandidate *> picked;
    for (const SmartCandidate &song : candidates) {
        bool ok = playlist.matchAll;
        for (const SmartRule &rule : playlist.rules) {
            const bool hit = matches(rule, song, now);
            if (playlist.matchAll && !hit) {
                ok = false;
                break;
            }
            if (!playlist.matchAll && hit) {
                ok = true;
                break;
            }
        }
        if (playlist.rules.empty())
            ok = true; // no rules: everything
        if (ok)
            picked.push_back(&song);
    }

    const std::string &order = playlist.order;
    if (order == "random") {
        std::mt19937 random(std::random_device{}());
        std::shuffle(picked.begin(), picked.end(), random);
    } else {
        std::stable_sort(picked.begin(), picked.end(), [&](const SmartCandidate *a, const SmartCandidate *b) {
            if (order == "plays" && a->plays != b->plays)
                return a->plays > b->plays;
            if (order == "lastPlayed" && a->lastPlayed != b->lastPlayed)
                return a->lastPlayed > b->lastPlayed;
            if (order == "added" && a->track.addedAt != b->track.addedAt)
                return a->track.addedAt > b->track.addedAt;
            if (order == "artist" && lower(a->track.artist) != lower(b->track.artist))
                return lower(a->track.artist) < lower(b->track.artist);
            return lower(a->track.title) < lower(b->track.title);
        });
    }
    std::vector<Track> out;
    for (const SmartCandidate *song : picked) {
        if (playlist.limit > 0 && int(out.size()) >= playlist.limit)
            break;
        out.push_back(song->track);
    }
    return out;
}
