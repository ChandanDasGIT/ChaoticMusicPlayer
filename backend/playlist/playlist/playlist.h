#pragma once

#include "backend/library/track/track.h"

#include <cstddef>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>

// Ordered list of unique tracks (by path), kept sorted by the active sort
// column, plus the current (playing) track and the play order (shuffle/repeat).
class Playlist
{
public:
    enum class SortColumn {
        Title,       // title, artist
        ArtistAlbum, // artist, album, disc, track
        Track,       // album, disc, track (the default order)
        Duration,    // duration, title
        Custom,      // no sorting: the order tracks were added in (saved playlists)
    };

    enum class RepeatMode { Off, All, One };

    const std::vector<Track> &tracks() const { return m_tracks; }
    size_t size() const { return m_tracks.size(); }
    const Track &at(size_t index) const { return m_tracks[index]; }
    bool contains(const std::string &path) const { return m_paths.count(path) != 0; }
    int indexOf(const std::string &path) const;

    // Replaces everything (duplicates dropped) and sorts; clears the current track.
    void setTracks(std::vector<Track> tracks);

    // Where `track` belongs in the current sort order.
    size_t insertPosition(const Track &track) const;
    // Inserts at `position` (from insertPosition); the current track keeps pointing at the same song.
    void insert(size_t position, Track track);
    // Adds at the end, whatever the sort column (callers switch to Custom if needed).
    void append(Track track) { insert(m_tracks.size(), std::move(track)); }

    // Moves a track (manual reordering); the current track keeps pointing at the same song.
    void move(size_t from, size_t to);
    // Removing the current track leaves no current track.
    void remove(size_t index);
    void clear();

    // Re-sorts; the current track (and the shuffle order) are unaffected.
    void sort(SortColumn column, bool descending);
    SortColumn sortColumn() const { return m_sortColumn; }
    bool sortDescending() const { return m_sortDescending; }

    // Shuffle plays every track once in random order before any repeats.
    void setShuffle(bool shuffle);
    bool shuffle() const { return m_shuffle; }
    // RepeatMode::One only affects auto-advance, which the caller handles;
    // for next/previous it behaves like All.
    void setRepeatMode(RepeatMode mode) { m_repeat = mode; }
    RepeatMode repeatMode() const { return m_repeat; }

    int currentIndex() const { return m_current; }

    // Total length of the tracks still to play after the current one
    // (in play order; one shuffle cycle; repeat ignored).
    int64_t upcomingDurationMs() const;

    // -1 when there is no next / previous track.
    int nextIndex() const;
    int previousIndex() const;

    void select(int index);                     // user picked a track
    void deselect() { m_current = -1; }         // no current track
    void goNext();                              // move to nextIndex()
    void goPrevious();                          // move to previousIndex()
    void follow(const std::string &path);       // the player moved on to `path` by itself

private:
    int compare(const Track &a, const Track &b) const;
    bool less(const Track &a, const Track &b) const { return compare(a, b) < 0; }
    void rebuildShuffle();
    void pickNextCycleFirst();

    std::vector<Track> m_tracks;
    std::unordered_set<std::string> m_paths;
    SortColumn m_sortColumn = SortColumn::Track;
    bool m_sortDescending = false;
    int m_current = -1;

    RepeatMode m_repeat = RepeatMode::Off;
    bool m_shuffle = false;
    std::vector<std::string> m_order; // shuffle: play order of the current cycle (paths)
    int m_orderPos = -1;              // shuffle: current track's position in m_order
    std::string m_nextCycleFirst;     // shuffle + repeat: first track of the next cycle
    std::mt19937 m_rng{std::random_device{}()};
};
