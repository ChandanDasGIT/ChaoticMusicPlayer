#include "backend/library/tageditor/tageditor.h"

#include <taglib/fileref.h>
#include <taglib/tpropertymap.h>

#include <system_error>

namespace fs = std::filesystem;

namespace {

std::string first(const TagLib::PropertyMap &props, const char *key)
{
    const TagLib::StringList values = props.value(key);
    return values.isEmpty() ? std::string() : values.front().to8Bit(true);
}

// All values of a multi-value tag, joined ("A; B").
std::string joined(const TagLib::PropertyMap &props, const char *key)
{
    std::string out;
    for (const TagLib::String &value : props.value(key)) {
        if (!out.empty())
            out += "; ";
        out += value.to8Bit(true);
    }
    return out;
}

std::string trim(std::string s)
{
    const auto start = s.find_first_not_of(" \t");
    if (start == std::string::npos)
        return {};
    return s.substr(start, s.find_last_not_of(" \t") - start + 1);
}

// "3" or "3/12" -> "3"
std::string numberPart(const std::string &value)
{
    return trim(value.substr(0, value.find('/')));
}

// A new number, keeping the "/12" total the file had.
std::string withTotal(const std::string &number, const std::string &old)
{
    const auto slash = old.find('/');
    if (number.empty() || slash == std::string::npos)
        return number;
    return number + old.substr(slash);
}

void set(TagLib::PropertyMap &props, const char *key, const std::string &value)
{
    const std::string v = trim(value);
    if (v.empty())
        props.erase(key);
    else
        props.replace(key, TagLib::StringList(TagLib::String(v, TagLib::String::UTF8)));
}

} // namespace

bool TagEditor::read(const fs::path &file, Tags &tags)
{
    TagLib::FileRef ref(file.c_str(), false);
    if (ref.isNull())
        return false;
    const TagLib::PropertyMap props = ref.properties();
    tags.title = first(props, "TITLE");
    tags.artist = joined(props, "ARTIST");
    tags.album = first(props, "ALBUM");
    tags.albumArtist = joined(props, "ALBUMARTIST");
    tags.track = numberPart(first(props, "TRACKNUMBER"));
    tags.disc = numberPart(first(props, "DISCNUMBER"));
    tags.year = first(props, "DATE");
    tags.genre = joined(props, "GENRE");
    return true;
}

bool TagEditor::write(const fs::path &file, const Tags &tags, const fs::path &backupDir, std::string &error)
{
    std::error_code ec;
    if (!fs::is_regular_file(file, ec)) {
        error = "The file isn't there any more.";
        return false;
    }
    // 1. A copy to fall back to
    fs::create_directories(backupDir, ec);
    const fs::path backup = backupDir / (file.filename().string() + ".backup");
    fs::copy_file(file, backup, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        error = "Couldn't make a safety copy first (" + ec.message() + "), so nothing was changed.";
        return false;
    }
    const auto restore = [&] {
        std::error_code restoreError;
        fs::copy_file(backup, file, fs::copy_options::overwrite_existing, restoreError);
        fs::remove(backup, restoreError);
    };

    // 2. The new tags, everything else as it was
    {
        TagLib::FileRef ref(file.c_str(), false);
        if (ref.isNull()) {
            fs::remove(backup, ec);
            error = "This kind of file can't be tagged.";
            return false;
        }
        TagLib::PropertyMap props = ref.properties();
        const std::string oldTrack = first(props, "TRACKNUMBER");
        const std::string oldDisc = first(props, "DISCNUMBER");
        set(props, "TITLE", tags.title);
        set(props, "ARTIST", tags.artist);
        set(props, "ALBUM", tags.album);
        set(props, "ALBUMARTIST", tags.albumArtist);
        set(props, "TRACKNUMBER", withTotal(trim(tags.track), oldTrack));
        set(props, "DISCNUMBER", withTotal(trim(tags.disc), oldDisc));
        set(props, "DATE", tags.year);
        set(props, "GENRE", tags.genre);
        ref.setProperties(props);
        if (!ref.save()) {
            restore();
            error = "The file couldn't be written (is it read-only?). It was left as it was.";
            return false;
        }
    }

    // 3. Read back: the file must still open, with the new title
    Tags check;
    if (!read(file, check) || check.title != trim(tags.title)) {
        restore();
        error = "The file didn't read back correctly after saving, so the old one was put back.";
        return false;
    }
    fs::remove(backup, ec);
    return true;
}
