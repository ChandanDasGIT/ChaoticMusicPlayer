#pragma once

#include "backend/library/track/track.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

// The local music library: every audio file in the user's library folders
// (minus excluded folders and hidden ones), with its tags. Kept in an index
// file so the app starts with it at once; a refresh reads tags only for new
// or changed files (size / modification time) and forgets files that are gone.
// Not thread-safe: one thread at a time (refresh() is meant for a worker).
class MusicLibrary
{
public:
    struct RefreshResult
    {
        size_t added = 0;
        size_t updated = 0;
        size_t removed = 0;
        bool cancelled = false;
    };

    explicit MusicLibrary(std::filesystem::path indexFile);

    void load();
    bool save() const;

    const std::vector<Track> &tracks() const { return m_tracks; }

    // Blocking. progress(done, total) is called while tags are read.
    RefreshResult refresh(const std::vector<std::string> &folders, const std::vector<std::string> &excluded,
                          const std::function<void(size_t done, size_t total)> &progress,
                          const std::atomic<bool> &cancel);

    // `path` is `folder` or inside it (whole path components: /music is not inside /mus).
    static bool isInside(const std::filesystem::path &path, const std::filesystem::path &folder);

private:
    struct Entry
    {
        Track track;
        int64_t size = 0;
        int64_t modified = 0;
    };

    std::filesystem::path m_indexFile;
    std::vector<Entry> m_entries; // sorted by path
    std::vector<Track> m_tracks;  // same order, for callers

    void rebuildTracks();
};
