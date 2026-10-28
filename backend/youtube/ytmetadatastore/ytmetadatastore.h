#pragma once

#include "backend/youtube/ytmusicclient/ytmusicclient.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>

struct YtMetadataEntry
{
    bool matched = false; // false = looked up, no match on YouTube Music
    bool close = false;   // matched by a close (not exact) title
    YtSong song;          // valid when matched
    int64_t fileSize = 0; // the local file as it was when looked up
    int64_t fileTime = 0;
    int64_t checkedAt = 0; // unix seconds
};

// Disk cache of YouTube Music lookups for local files (tab-separated text).
// An entry is ignored once the local file changes (size / modification time),
// and a "no match" is retried after 30 days.
class YtMetadataStore
{
public:
    explicit YtMetadataStore(std::filesystem::path file);

    void load();
    bool save() const;

    // Cheap: is there any record for this path (no file checks)?
    bool contains(const std::string &path) const { return m_entries.count(path) != 0; }
    // The record if it still applies to the file on disk; nullptr otherwise.
    const YtMetadataEntry *find(const std::string &path) const;
    // Stamps the entry with the file's current size / time and the current time.
    void put(const std::string &path, YtMetadataEntry entry);

private:
    std::filesystem::path m_file;
    std::unordered_map<std::string, YtMetadataEntry> m_entries;
};
