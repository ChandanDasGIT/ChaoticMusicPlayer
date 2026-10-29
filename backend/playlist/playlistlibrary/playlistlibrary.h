#pragma once

#include "backend/library/track/track.h"

#include <filesystem>
#include <string>
#include <vector>

// The user's named playlists (local files and YouTube songs mixed), kept in
// the order songs were added. Each playlist is one file in `dir` (playlist
// store format) plus an index file with ids and names.
class PlaylistLibrary
{
public:
    struct Entry
    {
        std::string id;
        std::string name;
        std::vector<Track> tracks;
    };

    explicit PlaylistLibrary(std::filesystem::path dir);

    void load();

    const std::vector<Entry> &playlists() const { return m_entries; }
    const Entry *find(const std::string &id) const;

    std::string create(const std::string &name); // returns the new id
    void rename(const std::string &id, const std::string &name);
    void remove(const std::string &id);

    // Appends tracks not already in the playlist; returns how many were added.
    int addTracks(const std::string &id, const std::vector<Track> &tracks);
    // Puts tracks (in this order) at `index`; one already in the playlist is
    // moved there instead. Returns how many were added.
    int insertTracks(const std::string &id, const std::vector<Track> &tracks, size_t index);
    void removeTrack(const std::string &id, size_t index);
    void moveTrack(const std::string &id, size_t from, size_t to);
    // New tags for that song in every playlist that has it; true if any had it.
    bool updateTrack(const Track &track);

private:
    Entry *findMutable(const std::string &id);
    bool saveIndex() const;
    bool savePlaylist(const Entry &entry) const;
    std::filesystem::path fileFor(const std::string &id) const;

    std::filesystem::path m_dir;
    std::vector<Entry> m_entries;
};
