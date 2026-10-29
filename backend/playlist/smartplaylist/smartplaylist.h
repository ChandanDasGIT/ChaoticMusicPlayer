#pragma once

#include "backend/library/track/track.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

// Smart playlists: lists that fill themselves from rules ("played more than
// 10 times", "added in the last 30 days", "artist contains Rafi"...), kept as a
// JSON file. evaluate() picks the matching songs from the candidates given.
struct SmartRule
{
    // field: "title" "artist" "album" (text), "plays" "duration" (number; duration in
    // minutes), "lastPlayed" "added" (days ago), "source" ("local" / "youtube")
    std::string field;
    // op: text: "contains" "notContains" "is" "isNot"; number: "greater" "less" "equals";
    // days: "inLast" "notInLast"; source: "is"
    std::string op;
    std::string value;
};

struct SmartPlaylist
{
    std::string id;
    std::string name;
    bool matchAll = true; // else any rule
    std::vector<SmartRule> rules;
    // "plays" "lastPlayed" "added" "title" "artist" "random"
    std::string order = "plays";
    int limit = 0; // songs, 0 = all
};

// A song that may go into a smart playlist, with what the rules look at.
struct SmartCandidate
{
    Track track;
    int plays = 0;
    int64_t lastPlayed = 0; // unix seconds, 0 = never
    bool youTube = false;
};

class SmartPlaylistStore
{
public:
    explicit SmartPlaylistStore(std::filesystem::path file);

    // Loads the saved playlists; the first time (no file yet), a few starters.
    void load();
    bool save() const;

    const std::vector<SmartPlaylist> &playlists() const { return m_playlists; }
    const SmartPlaylist *find(const std::string &id) const;
    // Adds it (empty id: a new id is made) or replaces the one with its id; returns the id.
    std::string put(SmartPlaylist playlist);
    void remove(const std::string &id);

private:
    std::filesystem::path m_file;
    std::vector<SmartPlaylist> m_playlists;
};

// The songs of `playlist` among `candidates`, in its order, at most its limit.
// `now`: unix seconds (for "days ago").
std::vector<Track> evaluate(const SmartPlaylist &playlist, const std::vector<SmartCandidate> &candidates, int64_t now);
