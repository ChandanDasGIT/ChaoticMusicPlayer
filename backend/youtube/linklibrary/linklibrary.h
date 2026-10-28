#pragma once

#include "backend/youtube/ytmusicclient/ytmusicclient.h"

#include <filesystem>
#include <vector>

// The user's own list of saved YouTube songs ("Saved" tab): no account needed.
// Newest first; one entry per video. Stored as tab-separated text.
class LinkLibrary
{
public:
    explicit LinkLibrary(std::filesystem::path file);

    void load();
    bool save() const;

    const std::vector<YtSong> &songs() const { return m_songs; }
    // Adds at the top; returns false if the video is already saved.
    bool add(const YtSong &song);
    void remove(size_t index);

private:
    std::filesystem::path m_file;
    std::vector<YtSong> m_songs;
};
