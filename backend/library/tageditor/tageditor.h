#pragma once

#include <filesystem>
#include <string>

// Reads and changes the common tags of one music file with TagLib. Writing is
// careful: the file is copied to `backupDir` first, the tags are written, the
// file is read back to check it, and the copy is put back if anything fails.
// Tags not edited here (lyrics, covers, ReplayGain...) are left as they are.
class TagEditor
{
public:
    struct Tags
    {
        std::string title;
        std::string artist;
        std::string album;
        std::string albumArtist;
        std::string track; // "3" (a "3/12" total in the file is kept)
        std::string disc;
        std::string year;
        std::string genre;
    };

    static bool read(const std::filesystem::path &file, Tags &tags);
    // false with `error` in plain words when the file couldn't be changed.
    static bool write(const std::filesystem::path &file, const Tags &tags, const std::filesystem::path &backupDir,
                      std::string &error);
};
