#include "backend/playlist/playlistlibrary/playlistlibrary.h"

#include "backend/playlist/playliststore/playliststore.h"
#include "backend/storage/tsv/tsv.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <random>
#include <system_error>

namespace fs = std::filesystem;

namespace {

constexpr const char *IndexHeader = "ChaoticMusicPlayer playlists v1";

// Short unique id, safe as a file name.
std::string newId()
{
    static std::mt19937_64 rng{std::random_device{}()};
    const auto now = std::chrono::system_clock::now().time_since_epoch().count();
    char buffer[40];
    std::snprintf(buffer, sizeof(buffer), "%llx%04llx", static_cast<unsigned long long>(now),
                  static_cast<unsigned long long>(rng() & 0xffff));
    return buffer;
}

} // namespace

PlaylistLibrary::PlaylistLibrary(fs::path dir)
    : m_dir(std::move(dir))
{
}

void PlaylistLibrary::load()
{
    m_entries.clear();
    std::ifstream in(m_dir / "index.tsv");
    std::string line;
    if (!in || !std::getline(in, line) || line != IndexHeader)
        return;

    while (std::getline(in, line)) {
        const std::vector<std::string> f = tsv::split(line);
        if (f.size() != 2)
            continue;
        Entry entry;
        entry.id = tsv::unescape(f[0]);
        entry.name = tsv::unescape(f[1]);
        if (entry.id.empty() || entry.id.find_first_of("/\\.") != std::string::npos)
            continue; // ids are file names: never let one point elsewhere
        entry.tracks = PlaylistStore::load(fileFor(entry.id));
        m_entries.push_back(std::move(entry));
    }
}

const PlaylistLibrary::Entry *PlaylistLibrary::find(const std::string &id) const
{
    const auto it = std::find_if(m_entries.begin(), m_entries.end(), [&](const Entry &e) { return e.id == id; });
    return it != m_entries.end() ? &*it : nullptr;
}

PlaylistLibrary::Entry *PlaylistLibrary::findMutable(const std::string &id)
{
    return const_cast<Entry *>(find(id));
}

std::string PlaylistLibrary::create(const std::string &name)
{
    Entry entry;
    entry.id = newId();
    entry.name = name;
    m_entries.push_back(entry);
    savePlaylist(entry);
    saveIndex();
    return entry.id;
}

void PlaylistLibrary::rename(const std::string &id, const std::string &name)
{
    if (Entry *entry = findMutable(id)) {
        entry->name = name;
        saveIndex();
    }
}

void PlaylistLibrary::remove(const std::string &id)
{
    const auto it = std::find_if(m_entries.begin(), m_entries.end(), [&](const Entry &e) { return e.id == id; });
    if (it == m_entries.end())
        return;
    std::error_code ec;
    fs::remove(fileFor(id), ec);
    m_entries.erase(it);
    saveIndex();
}

int PlaylistLibrary::addTracks(const std::string &id, const std::vector<Track> &tracks)
{
    Entry *entry = findMutable(id);
    if (!entry)
        return 0;
    int added = 0;
    for (const Track &track : tracks) {
        const bool exists = std::any_of(entry->tracks.begin(), entry->tracks.end(),
                                        [&](const Track &t) { return t.path == track.path; });
        if (!exists) {
            entry->tracks.push_back(track);
            ++added;
        }
    }
    if (added > 0)
        savePlaylist(*entry);
    return added;
}

int PlaylistLibrary::insertTracks(const std::string &id, const std::vector<Track> &tracks, size_t index)
{
    Entry *entry = findMutable(id);
    if (!entry || tracks.empty())
        return 0;
    std::vector<Track> &list = entry->tracks;
    size_t at = std::min(index, list.size());
    int added = 0;
    for (const Track &track : tracks) {
        const auto existing = std::find_if(list.begin(), list.end(), [&](const Track &t) { return t.path == track.path; });
        if (existing != list.end()) {
            const size_t from = size_t(existing - list.begin());
            if (from < at)
                --at; // taken out ahead of the drop spot
            list.erase(existing);
        } else {
            ++added;
        }
        list.insert(list.begin() + std::ptrdiff_t(at), track);
        ++at;
    }
    savePlaylist(*entry);
    return added;
}

void PlaylistLibrary::removeTrack(const std::string &id, size_t index)
{
    Entry *entry = findMutable(id);
    if (!entry || index >= entry->tracks.size())
        return;
    entry->tracks.erase(entry->tracks.begin() + std::ptrdiff_t(index));
    savePlaylist(*entry);
}

void PlaylistLibrary::moveTrack(const std::string &id, size_t from, size_t to)
{
    Entry *entry = findMutable(id);
    if (!entry || from >= entry->tracks.size() || to >= entry->tracks.size() || from == to)
        return;
    Track track = std::move(entry->tracks[from]);
    entry->tracks.erase(entry->tracks.begin() + std::ptrdiff_t(from));
    entry->tracks.insert(entry->tracks.begin() + std::ptrdiff_t(to), std::move(track));
    savePlaylist(*entry);
}

bool PlaylistLibrary::saveIndex() const
{
    std::error_code ec;
    fs::create_directories(m_dir, ec);
    const fs::path file = m_dir / "index.tsv";
    const fs::path temp = file.string() + ".tmp";
    {
        std::ofstream out(temp, std::ios::trunc);
        if (!out)
            return false;
        out << IndexHeader << '\n';
        for (const Entry &entry : m_entries)
            out << tsv::escape(entry.id) << '\t' << tsv::escape(entry.name) << '\n';
        if (!out)
            return false;
    }
    fs::rename(temp, file, ec);
    return !ec;
}

bool PlaylistLibrary::savePlaylist(const Entry &entry) const
{
    return PlaylistStore::save(fileFor(entry.id), entry.tracks);
}

fs::path PlaylistLibrary::fileFor(const std::string &id) const
{
    return m_dir / (id + ".tsv");
}
