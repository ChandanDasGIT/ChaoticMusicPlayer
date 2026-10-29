#include "backend/library/musiclibrary/musiclibrary.h"

#include "backend/library/metadata/metadata.h"
#include "backend/library/scanner/scanner.h"
#include "backend/storage/tsv/tsv.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <system_error>

namespace fs = std::filesystem;

namespace {

constexpr const char *Header = "ChaoticMusicPlayer library v3";
constexpr size_t FieldCount = 13;
constexpr const char *HeaderV2 = "ChaoticMusicPlayer library v2"; // before "added"
constexpr size_t FieldCountV2 = 12;
// v1 had no album artist: its songs are kept, and read again on the next refresh.
constexpr const char *HeaderV1 = "ChaoticMusicPlayer library v1";
constexpr size_t FieldCountV1 = 11;

struct Stamp
{
    int64_t size = 0;
    int64_t modified = 0;
};

int64_t nowSeconds()
{
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

// A file's modification stamp (file clock ticks) as unix seconds.
int64_t stampSeconds(int64_t modified)
{
    const auto time = fs::file_time_type(fs::file_time_type::duration(modified));
    const auto system = std::chrono::file_clock::to_sys(time);
    return std::chrono::duration_cast<std::chrono::seconds>(system.time_since_epoch()).count();
}

bool stampOf(const fs::path &path, Stamp &stamp)
{
    std::error_code ec;
    const auto size = fs::file_size(path, ec);
    if (ec)
        return false;
    const auto time = fs::last_write_time(path, ec);
    if (ec)
        return false;
    stamp.size = int64_t(size);
    stamp.modified = int64_t(time.time_since_epoch().count());
    return true;
}

// Audio files under the folders, skipping excluded and hidden folders.
std::vector<fs::path> collect(const std::vector<std::string> &folders, const std::vector<std::string> &excluded,
                              const std::atomic<bool> &cancel)
{
    std::vector<fs::path> files;
    for (const std::string &folder : folders) {
        std::error_code ec;
        fs::recursive_directory_iterator it(folder, fs::directory_options::skip_permission_denied, ec);
        for (const fs::recursive_directory_iterator end; !ec && it != end; it.increment(ec)) {
            if (cancel.load(std::memory_order_relaxed))
                return files;
            const fs::path &path = it->path();
            std::error_code typeError;
            if (it->is_directory(typeError)) {
                const std::string name = path.filename().string();
                const bool skip = (!name.empty() && name[0] == '.')
                                  || std::any_of(excluded.begin(), excluded.end(), [&](const std::string &ex) {
                                         return MusicLibrary::isInside(path, ex);
                                     });
                if (skip)
                    it.disable_recursion_pending();
                continue;
            }
            if (it->is_regular_file(typeError) && Scanner::isAudioFile(path))
                files.push_back(path);
        }
    }
    std::sort(files.begin(), files.end());
    files.erase(std::unique(files.begin(), files.end()), files.end()); // overlapping folders
    // A file directly inside an excluded folder given as a library folder itself
    files.erase(std::remove_if(files.begin(), files.end(),
                               [&](const fs::path &file) {
                                   return std::any_of(excluded.begin(), excluded.end(), [&](const std::string &ex) {
                                       return MusicLibrary::isInside(file, ex);
                                   });
                               }),
                files.end());
    return files;
}

} // namespace

MusicLibrary::MusicLibrary(fs::path indexFile)
    : m_indexFile(std::move(indexFile))
{
}

bool MusicLibrary::isInside(const fs::path &path, const fs::path &folder)
{
    const fs::path p = path.lexically_normal();
    const fs::path f = folder.lexically_normal();
    auto pi = p.begin();
    for (auto fi = f.begin(); fi != f.end(); ++fi, ++pi) {
        if (fi->empty()) // trailing slash
            continue;
        if (pi == p.end() || *pi != *fi)
            return false;
    }
    return true;
}

void MusicLibrary::load()
{
    m_entries.clear();
    std::ifstream in(m_indexFile);
    std::string line;
    const bool v1 = in && std::getline(in, line) && line == HeaderV1;
    const bool v2 = line == HeaderV2;
    if (in && (v1 || v2 || line == Header)) {
        while (std::getline(in, line)) {
            const std::vector<std::string> f = tsv::split(line);
            if (f.size() != (v1 ? FieldCountV1 : v2 ? FieldCountV2 : FieldCount))
                continue;
            Entry e;
            e.track.path = tsv::unescape(f[0]);
            e.size = tsv::toNumber<int64_t>(f[1]);
            e.modified = tsv::toNumber<int64_t>(f[2]);
            e.track.title = tsv::unescape(f[3]);
            e.track.artist = tsv::unescape(f[4]);
            e.track.album = tsv::unescape(f[5]);
            e.track.disc = tsv::toNumber<int>(f[6]);
            e.track.discTotal = tsv::toNumber<int>(f[7]);
            e.track.track = tsv::toNumber<int>(f[8]);
            e.track.durationMs = tsv::toNumber<int64_t>(f[9]);
            e.track.hasCover = f[10] == "1";
            if (v1)
                e.size = -1; // never matches the file: its tags are read again
            else
                e.track.albumArtist = tsv::unescape(f[11]);
            // Not known before v3: the file's own date is the best guess.
            e.track.addedAt = v1 || v2 ? stampSeconds(e.modified) : tsv::toNumber<int64_t>(f[12]);
            m_entries.push_back(std::move(e));
        }
    }
    std::sort(m_entries.begin(), m_entries.end(),
              [](const Entry &a, const Entry &b) { return a.track.path < b.track.path; });
    rebuildTracks();
}

bool MusicLibrary::save() const
{
    std::error_code ec;
    fs::create_directories(m_indexFile.parent_path(), ec);
    const fs::path temp = m_indexFile.string() + ".tmp";
    {
        std::ofstream out(temp, std::ios::trunc);
        if (!out)
            return false;
        out << Header << '\n';
        for (const Entry &e : m_entries) {
            const Track &t = e.track;
            out << tsv::escape(t.path) << '\t' << e.size << '\t' << e.modified << '\t' << tsv::escape(t.title)
                << '\t' << tsv::escape(t.artist) << '\t' << tsv::escape(t.album) << '\t' << t.disc << '\t'
                << t.discTotal << '\t' << t.track << '\t' << t.durationMs << '\t' << (t.hasCover ? 1 : 0) << '\t'
                << tsv::escape(t.albumArtist) << '\t' << t.addedAt << '\n';
        }
        if (!out)
            return false;
    }
    fs::rename(temp, m_indexFile, ec);
    return !ec;
}

MusicLibrary::RefreshResult MusicLibrary::refresh(const std::vector<std::string> &folders,
                                                  const std::vector<std::string> &excluded,
                                                  const std::function<void(size_t, size_t)> &progress,
                                                  const std::atomic<bool> &cancel)
{
    RefreshResult result;
    const std::vector<fs::path> files = collect(folders, excluded, cancel);
    if (cancel.load(std::memory_order_relaxed)) {
        result.cancelled = true;
        return result;
    }

    std::vector<Entry> fresh;
    fresh.reserve(files.size());
    size_t done = 0;
    auto old = m_entries.begin();
    for (const fs::path &file : files) {
        if (cancel.load(std::memory_order_relaxed)) {
            result.cancelled = true;
            return result; // keep the previous library as it was
        }
        const std::string path = file.string();
        // Both lists are sorted by path: walk them together.
        while (old != m_entries.end() && old->track.path < path)
            ++old;
        const bool known = old != m_entries.end() && old->track.path == path;

        Stamp stamp;
        if (!stampOf(file, stamp))
            continue;
        if (known && old->size == stamp.size && old->modified == stamp.modified) {
            fresh.push_back(*old); // unchanged: no need to read the tags again
        } else {
            Entry e;
            e.track = Metadata::read(file);
            e.track.path = path;
            e.size = stamp.size;
            e.modified = stamp.modified;
            e.track.addedAt = known ? old->track.addedAt : nowSeconds(); // a changed file keeps its date
            fresh.push_back(std::move(e));
            ++(known ? result.updated : result.added);
        }
        if (progress && (++done % 25 == 0 || done == files.size()))
            progress(done, files.size());
    }
    result.removed = m_entries.size() + result.added - fresh.size();

    m_entries = std::move(fresh);
    rebuildTracks();
    return result;
}

void MusicLibrary::rebuildTracks()
{
    m_tracks.clear();
    m_tracks.reserve(m_entries.size());
    for (const Entry &e : m_entries)
        m_tracks.push_back(e.track);
}
