#include "backend/playlist/playlist/playlist.h"

#include <algorithm>
#include <cctype>

namespace {

// Case-insensitive (ASCII); empty strings sort last.
int compareText(const std::string &a, const std::string &b)
{
    if (a.empty() != b.empty())
        return a.empty() ? 1 : -1;

    const size_t n = std::min(a.size(), b.size());
    for (size_t i = 0; i < n; ++i) {
        const int ca = std::tolower(static_cast<unsigned char>(a[i]));
        const int cb = std::tolower(static_cast<unsigned char>(b[i]));
        if (ca != cb)
            return ca < cb ? -1 : 1;
    }
    return a.size() == b.size() ? 0 : (a.size() < b.size() ? -1 : 1);
}

template<typename T>
int compareNumber(T a, T b)
{
    return a < b ? -1 : (a > b ? 1 : 0);
}

// Unknown (0) track numbers sort last.
int compareTrackNumber(int a, int b)
{
    if ((a == 0) != (b == 0))
        return a == 0 ? 1 : -1;
    return compareNumber(a, b);
}

int disc(const Track &t)
{
    return std::max(t.disc, 1);
}

} // namespace

int Playlist::indexOf(const std::string &path) const
{
    const auto it = std::find_if(m_tracks.begin(), m_tracks.end(),
                                 [&](const Track &t) { return t.path == path; });
    return it != m_tracks.end() ? int(it - m_tracks.begin()) : -1;
}

void Playlist::setTracks(std::vector<Track> tracks)
{
    m_tracks.clear();
    m_paths.clear();
    m_current = -1;

    m_tracks.reserve(tracks.size());
    for (Track &track : tracks) {
        if (m_paths.insert(track.path).second)
            m_tracks.push_back(std::move(track));
    }
    std::stable_sort(m_tracks.begin(), m_tracks.end(),
                     [this](const Track &a, const Track &b) { return less(a, b); });
    if (m_shuffle)
        rebuildShuffle();
}

size_t Playlist::insertPosition(const Track &track) const
{
    const auto it = std::upper_bound(m_tracks.begin(), m_tracks.end(), track,
                                     [this](const Track &a, const Track &b) { return less(a, b); });
    return size_t(it - m_tracks.begin());
}

void Playlist::insert(size_t position, Track track)
{
    position = std::min(position, m_tracks.size());
    m_paths.insert(track.path);

    if (m_shuffle) {
        // Somewhere after the upcoming track, so what plays next doesn't change.
        // (If this cycle is used up, the new tracks are what play next.)
        const size_t lo = std::min(m_order.size(), size_t(m_orderPos + 2));
        std::uniform_int_distribution<size_t> dist(lo, m_order.size());
        m_order.insert(m_order.begin() + std::ptrdiff_t(dist(m_rng)), track.path);
        if (m_nextCycleFirst.empty() || m_nextCycleFirst == m_order.back())
            pickNextCycleFirst();
    }

    m_tracks.insert(m_tracks.begin() + std::ptrdiff_t(position), std::move(track));
    if (m_current >= 0 && int(position) <= m_current)
        ++m_current;
}

void Playlist::move(size_t from, size_t to)
{
    if (from >= m_tracks.size() || to >= m_tracks.size() || from == to)
        return;
    const std::string currentPath = m_current >= 0 ? m_tracks[size_t(m_current)].path : std::string();
    Track track = std::move(m_tracks[from]);
    m_tracks.erase(m_tracks.begin() + std::ptrdiff_t(from));
    m_tracks.insert(m_tracks.begin() + std::ptrdiff_t(to), std::move(track));
    if (!currentPath.empty())
        m_current = indexOf(currentPath);
}

void Playlist::remove(size_t index)
{
    if (index >= m_tracks.size())
        return;

    const std::string path = m_tracks[index].path;
    m_paths.erase(path);
    m_tracks.erase(m_tracks.begin() + std::ptrdiff_t(index));

    if (m_current == int(index))
        m_current = -1;
    else if (m_current > int(index))
        --m_current;

    if (m_shuffle) {
        // The track after the removed one stays "next" in the shuffle order.
        const auto it = std::find(m_order.begin(), m_order.end(), path);
        if (it != m_order.end()) {
            const int position = int(it - m_order.begin());
            m_order.erase(it);
            if (position <= m_orderPos)
                --m_orderPos;
        }
        if (m_nextCycleFirst == path || (!m_order.empty() && m_nextCycleFirst == m_order.back()))
            pickNextCycleFirst();
    }
}

void Playlist::clear()
{
    m_tracks.clear();
    m_paths.clear();
    m_current = -1;
    m_order.clear();
    m_orderPos = -1;
    m_nextCycleFirst.clear();
}

void Playlist::sort(SortColumn column, bool descending)
{
    const std::string currentPath = m_current >= 0 ? m_tracks[size_t(m_current)].path : std::string();

    m_sortColumn = column;
    m_sortDescending = descending;
    std::stable_sort(m_tracks.begin(), m_tracks.end(),
                     [this](const Track &a, const Track &b) { return less(a, b); });

    if (!currentPath.empty()) {
        const auto it = std::find_if(m_tracks.begin(), m_tracks.end(),
                                     [&](const Track &t) { return t.path == currentPath; });
        m_current = int(it - m_tracks.begin());
    }
}

void Playlist::setShuffle(bool shuffle)
{
    if (shuffle == m_shuffle)
        return;
    m_shuffle = shuffle;
    m_order.clear();
    m_orderPos = -1;
    m_nextCycleFirst.clear();
    if (shuffle)
        rebuildShuffle();
}

int Playlist::nextIndex() const
{
    if (m_tracks.empty())
        return -1;

    if (m_shuffle) {
        if (size_t(m_orderPos + 1) < m_order.size())
            return indexOf(m_order[size_t(m_orderPos + 1)]);
        return m_repeat != RepeatMode::Off ? indexOf(m_nextCycleFirst) : -1;
    }

    if (m_current < 0)
        return 0;
    if (size_t(m_current) + 1 < m_tracks.size())
        return m_current + 1;
    return m_repeat != RepeatMode::Off ? 0 : -1;
}

int Playlist::previousIndex() const
{
    if (m_shuffle)
        return m_orderPos > 0 ? indexOf(m_order[size_t(m_orderPos - 1)]) : -1;

    if (m_current > 0)
        return m_current - 1;
    if (m_current == 0 && m_repeat != RepeatMode::Off && m_tracks.size() > 1)
        return int(m_tracks.size()) - 1;
    return -1;
}

int64_t Playlist::upcomingDurationMs() const
{
    int64_t total = 0;
    if (m_shuffle) {
        for (size_t i = size_t(m_orderPos + 1); i < m_order.size(); ++i) {
            const int index = indexOf(m_order[i]);
            if (index >= 0)
                total += m_tracks[size_t(index)].durationMs;
        }
        return total;
    }
    for (size_t i = size_t(m_current + 1); i < m_tracks.size(); ++i)
        total += m_tracks[i].durationMs;
    return total;
}

void Playlist::select(int index)
{
    if (index < 0 || size_t(index) >= m_tracks.size())
        return;
    m_current = index;
    if (!m_shuffle)
        return;

    // Make the picked track the current entry of the shuffle order, keeping
    // the history before it and the rest of the cycle after it.
    const std::string &path = m_tracks[size_t(index)].path;
    const auto it = std::find(m_order.begin(), m_order.end(), path);
    const int found = int(it - m_order.begin());
    if (found == m_orderPos)
        return;
    if (it != m_order.end()) {
        m_order.erase(it);
        if (found < m_orderPos)
            --m_orderPos;
    }
    m_order.insert(m_order.begin() + std::ptrdiff_t(m_orderPos + 1), path);
    ++m_orderPos;
    if (m_nextCycleFirst.empty() || m_nextCycleFirst == m_order.back())
        pickNextCycleFirst();
}

void Playlist::goNext()
{
    const int next = nextIndex();
    if (next < 0)
        return;
    m_current = next;
    if (!m_shuffle)
        return;

    if (size_t(m_orderPos + 1) < m_order.size())
        ++m_orderPos;
    else
        rebuildShuffle(); // new cycle, starting with the current (= m_nextCycleFirst)
}

void Playlist::goPrevious()
{
    const int previous = previousIndex();
    if (previous < 0)
        return;
    m_current = previous;
    if (m_shuffle)
        --m_orderPos;
}

void Playlist::follow(const std::string &path)
{
    const int index = indexOf(path);
    if (index < 0) {
        m_current = -1; // playing something that was removed from the playlist
        return;
    }
    if (index == m_current)
        return; // repeat-one replaying the same track
    if (index == nextIndex())
        goNext();
    else
        select(index);
}

// Current track first (if any), everything else in random order.
void Playlist::rebuildShuffle()
{
    m_order.clear();
    m_order.reserve(m_tracks.size());
    for (const Track &track : m_tracks)
        m_order.push_back(track.path);
    std::shuffle(m_order.begin(), m_order.end(), m_rng);

    m_orderPos = -1;
    if (m_current >= 0) {
        const auto it = std::find(m_order.begin(), m_order.end(), m_tracks[size_t(m_current)].path);
        std::iter_swap(m_order.begin(), it);
        m_orderPos = 0;
    }
    pickNextCycleFirst();
}

// Any track except the one that ends this cycle, so nothing plays twice in a row.
void Playlist::pickNextCycleFirst()
{
    m_nextCycleFirst.clear();
    if (m_order.empty())
        return;
    if (m_order.size() == 1) {
        m_nextCycleFirst = m_order.front();
        return;
    }
    std::uniform_int_distribution<size_t> dist(0, m_order.size() - 2);
    m_nextCycleFirst = m_order[dist(m_rng)];
}

int Playlist::compare(const Track &a, const Track &b) const
{
    if (m_sortColumn == SortColumn::Custom)
        return 0; // stable sorts keep the order; inserts go to the end
    int c = 0;
    switch (m_sortColumn) {
    case SortColumn::Title:
        (c = compareText(a.title, b.title))
            || (c = compareText(a.artist, b.artist));
        break;
    case SortColumn::ArtistAlbum:
        (c = compareText(a.artist, b.artist))
            || (c = compareText(a.album, b.album))
            || (c = compareNumber(disc(a), disc(b)))
            || (c = compareTrackNumber(a.track, b.track));
        break;
    case SortColumn::Track:
        (c = compareText(a.album, b.album))
            || (c = compareNumber(disc(a), disc(b)))
            || (c = compareTrackNumber(a.track, b.track))
            || (c = compareText(a.title, b.title));
        break;
    case SortColumn::Duration:
        (c = compareNumber(a.durationMs, b.durationMs))
            || (c = compareText(a.title, b.title));
        break;
    case SortColumn::Custom:
        break; // handled above
    }
    if (c == 0) // deterministic order for ties (path is unique)
        c = compareNumber(a.path, b.path);
    return m_sortDescending ? -c : c;
}
