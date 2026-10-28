#pragma once

#include "backend/library/track/track.h"

#include <filesystem>
#include <vector>

// Saves / loads the playlist as a tab-separated text file, so metadata does
// not need to be re-read on every start.
class PlaylistStore
{
public:
    static bool save(const std::filesystem::path &file, const std::vector<Track> &tracks);
    // Local tracks whose file no longer exists are dropped. Reads v1 files too.
    static std::vector<Track> load(const std::filesystem::path &file);
};
