#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

// The private listening history: every song listened to (long enough to
// count), kept as a tab-separated text file on this computer. Recording a
// play appends one line, so a long history costs nothing to add to.
class ListeningHistory
{
public:
    struct Play
    {
        int64_t time = 0;     // when it was counted (unix seconds)
        std::string path;     // file, or "ytmusic:<videoId>"
        std::string title;
        std::string artist;
        std::string album;
        int64_t durationMs = 0;
        std::string thumbnailUrl; // YouTube songs
        std::string from;         // what it played from ("Queue", an album, a Mix...)
    };

    explicit ListeningHistory(std::filesystem::path file);

    void load();
    const std::vector<Play> &plays() const { return m_plays; } // oldest first

    bool append(const Play &play);
    bool remove(size_t index); // one play (index into plays())
    bool clear();

private:
    bool save(const std::vector<Play> &plays) const; // rewrites the file

    std::filesystem::path m_file;
    std::vector<Play> m_plays;
};
