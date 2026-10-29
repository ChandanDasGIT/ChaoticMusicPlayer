#include "wrapper/history/historycontroller.h"

#include "backend/youtube/streamresolver/streamresolver.h"
#include "wrapper/locallibrary/locallibrarycontroller.h"
#include "wrapper/player/playercontroller.h"
#include "wrapper/playlist/playlistcontroller.h"
#include "wrapper/playlists/playlistscontroller.h"
#include "wrapper/settings/settingscontroller.h"

#include <QDateTime>
#include <QFileInfo>
#include <QLocale>
#include <QJSEngine>
#include <QStandardPaths>

#include <algorithm>
#include <map>
#include <random>
#include <unordered_map>
#include <unordered_set>

namespace {

constexpr qint64 MinSongMs = 30 * 1000;      // shorter songs aren't counted
constexpr qint64 EnoughMs = 4 * 60 * 1000;   // listened this long: counted, even if not half
constexpr qint64 MaxStepMs = 1500;           // a bigger jump between positions is a seek, not listening
constexpr int RecentCount = 20;
constexpr int MostPlayedCount = 10;
constexpr int MostPlayedDays = 30;
constexpr int AlbumCount = 12;
constexpr int EntryCount = 500;              // "See all" shows this many plays
constexpr int MixCount = 50;
constexpr int MixMinimum = 25;               // fewer most-played songs: topped up from the library

QString qs(const std::string &s)
{
    return QString::fromStdString(s);
}

// "Today", "Yesterday", a weekday within the week, else the date.
QString dayLabel(const QDate &day)
{
    const QDate today = QDate::currentDate();
    const qint64 ago = day.daysTo(today);
    if (ago == 0)
        return QObject::tr("Today");
    if (ago == 1)
        return QObject::tr("Yesterday");
    if (ago > 1 && ago < 7)
        return QLocale().dayName(day.dayOfWeek());
    return QLocale().toString(day, day.year() == today.year() ? QStringLiteral("d MMMM") : QStringLiteral("d MMMM yyyy"));
}

} // namespace

HistoryController::HistoryController(PlayerController *player, PlaylistController *playlist,
                                     PlaylistsController *playlists, LocalLibraryController *library,
                                     SettingsController *settings, QObject *parent)
    : QObject(parent)
    , m_player(player)
    , m_playlist(playlist)
    , m_playlists(playlists)
    , m_library(library)
    , m_settings(settings)
    , m_history(std::make_unique<ListeningHistory>(
          (QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
           + QStringLiteral("/ChaoticMusicPlayer/history.tsv"))
              .toStdString()))
{
    Q_ASSERT(!s_instance);
    s_instance = this;
    m_history->load();
    rebuild();

    connect(m_player, &PlayerController::sourceChanged, this, &HistoryController::startSong);
    connect(m_player, &PlayerController::positionChanged, this, &HistoryController::onPosition);
    startSong();
}

HistoryController::~HistoryController()
{
    s_instance = nullptr;
}

HistoryController *HistoryController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

// A song was loaded (also the same one again, e.g. on repeat): its listening starts from zero.
void HistoryController::startSong()
{
    m_path = m_player->source();
    m_listenedMs = 0;
    m_lastPosition = m_player->position();
    m_counted = false;
}

void HistoryController::onPosition()
{
    const qint64 position = m_player->position();
    const qint64 step = position - m_lastPosition;
    m_lastPosition = position;
    if (m_counted || m_path.isEmpty() || !m_player->playing() || step <= 0 || step > MaxStepMs)
        return; // already counted, nothing loaded, paused, or a seek

    m_listenedMs += step;
    const qint64 duration = m_player->duration();
    if (duration < MinSongMs)
        return;
    if (m_listenedMs >= std::min(duration / 2, EnoughMs))
        record();
}

void HistoryController::record()
{
    m_counted = true;
    const bool youTube = StreamResolver::isYouTube(m_path.toStdString());
    if (!m_settings->historyEnabled() || (youTube && !m_settings->historyYouTube()))
        return;

    ListeningHistory::Play play;
    play.time = QDateTime::currentSecsSinceEpoch();
    play.path = m_path.toStdString();
    play.durationMs = m_player->duration();
    if (const ::Track *track = m_playlist->trackForPath(m_path)) {
        play.title = track->title;
        play.artist = track->artist;
        play.album = track->album;
        play.thumbnailUrl = track->thumbnailUrl;
    }
    if (play.title.empty())
        play.title = m_player->title().toStdString();
    const QString from = !m_playlist->contextTitle().isEmpty() ? m_playlist->contextTitle()
                       : m_playlist->playingFromList()        ? m_playlists->playingFromName()
                                                              : tr("Queue");
    play.from = from.toStdString();
    if (m_history->append(play)) {
        rebuild();
        emit changed();
    }
}

void HistoryController::removeEntry(int index)
{
    // `entries` lists the plays latest first.
    const int count = int(m_history->plays().size());
    if (index < 0 || index >= count)
        return;
    if (m_history->remove(size_t(count - 1 - index))) {
        rebuild();
        emit changed();
    }
}

void HistoryController::clear()
{
    if (m_history->clear()) {
        rebuild();
        emit changed();
    }
}

namespace {

// A play as QML shows it; `item` is what the Queue and playlists take.
QVariantMap songOf(const ListeningHistory::Play &play)
{
    QVariantMap song{{QStringLiteral("path"), qs(play.path)},
                     {QStringLiteral("title"), qs(play.title)},
                     {QStringLiteral("artist"), qs(play.artist)},
                     {QStringLiteral("album"), qs(play.album)},
                     {QStringLiteral("thumbnail"), qs(play.thumbnailUrl)},
                     {QStringLiteral("durationMs"), qint64(play.durationMs)}};
    if (StreamResolver::isYouTube(play.path)) {
        song.insert(QStringLiteral("item"),
                    QVariantMap{{QStringLiteral("videoId"), qs(StreamResolver::videoId(play.path))},
                                {QStringLiteral("title"), qs(play.title)},
                                {QStringLiteral("artists"), qs(play.artist)},
                                {QStringLiteral("album"), qs(play.album)},
                                {QStringLiteral("duration"), qint64(play.durationMs)},
                                {QStringLiteral("thumbnail"), qs(play.thumbnailUrl)}});
    } else {
        song.insert(QStringLiteral("item"), QVariantMap{{QStringLiteral("path"), qs(play.path)}});
    }
    return song;
}

} // namespace

void HistoryController::rebuild()
{
    const std::vector<ListeningHistory::Play> &plays = m_history->plays();
    m_recent.clear();
    m_mostPlayed.clear();
    m_recentAlbums.clear();
    m_favoriteYouTube.clear();
    m_entries.clear();

    // Latest first: recent songs, recent albums, every play by day
    std::unordered_set<std::string> seenSongs, seenAlbums;
    for (auto it = plays.rbegin(); it != plays.rend(); ++it) {
        if (m_recent.size() < RecentCount && seenSongs.insert(it->path).second)
            m_recent.append(songOf(*it));
        const bool local = !StreamResolver::isYouTube(it->path);
        if (local && !it->album.empty() && m_recentAlbums.size() < AlbumCount
            && seenAlbums.insert(it->album + '\t' + it->artist).second)
            m_recentAlbums.append(QVariantMap{{QStringLiteral("album"), qs(it->album)},
                                              {QStringLiteral("artist"), qs(it->artist)},
                                              {QStringLiteral("path"), qs(it->path)}});
        if (m_entries.size() < EntryCount) {
            QVariantMap entry = songOf(*it);
            const QDateTime when = QDateTime::fromSecsSinceEpoch(it->time);
            entry.insert(QStringLiteral("day"), dayLabel(when.date()));
            entry.insert(QStringLiteral("clock"), QLocale().toString(when.time(), QLocale::ShortFormat));
            entry.insert(QStringLiteral("from"), qs(it->from));
            m_entries.append(entry);
        }
    }

    // Most played: the last 30 days (latest play of each song wins for its details)
    const qint64 since = QDateTime::currentSecsSinceEpoch() - qint64(MostPlayedDays) * 24 * 3600;
    std::unordered_map<std::string, int> counts, allTime;
    std::unordered_map<std::string, const ListeningHistory::Play *> latest;
    for (const ListeningHistory::Play &play : plays) {
        ++allTime[play.path];
        latest[play.path] = &play;
        if (play.time >= since)
            ++counts[play.path];
    }
    std::vector<std::pair<std::string, int>> ranked(counts.begin(), counts.end());
    std::sort(ranked.begin(), ranked.end(), [&](const auto &a, const auto &b) {
        return a.second != b.second ? a.second > b.second : latest[a.first]->time > latest[b.first]->time;
    });
    for (const auto &[path, n] : ranked) {
        if (m_mostPlayed.size() >= MostPlayedCount)
            break;
        QVariantMap song = songOf(*latest[path]);
        song.insert(QStringLiteral("plays"), n);
        m_mostPlayed.append(song);
    }

    // The YouTube song played most (its Mix is the recommendation)
    int best = 0;
    for (const auto &[path, n] : allTime) {
        if (StreamResolver::isYouTube(path) && n > best) {
            best = n;
            m_favoriteYouTube = songOf(*latest[path]).value(QStringLiteral("item")).toMap();
        }
    }
}

// A song from the lists to play: the library's details when it has them.
::Track HistoryController::trackOf(const QVariantMap &song) const
{
    const QString path = song.value(QStringLiteral("path")).toString();
    if (const ::Track *known = m_library->trackForPath(path))
        return *known;
    ::Track track;
    track.path = path.toStdString();
    track.title = song.value(QStringLiteral("title")).toString().toStdString();
    track.artist = song.value(QStringLiteral("artist")).toString().toStdString();
    track.album = song.value(QStringLiteral("album")).toString().toStdString();
    track.durationMs = song.value(QStringLiteral("durationMs")).toLongLong();
    track.thumbnailUrl = song.value(QStringLiteral("thumbnail")).toString().toStdString();
    if (StreamResolver::isYouTube(track.path))
        track.videoId = StreamResolver::videoId(track.path);
    return track;
}

void HistoryController::playList(const QString &which, int index)
{
    const QVariantList &list = which == QLatin1String("recent")     ? m_recent
                             : which == QLatin1String("mostPlayed") ? m_mostPlayed
                                                                    : m_entries;
    const QString title = which == QLatin1String("recent")     ? tr("Recently played")
                        : which == QLatin1String("mostPlayed") ? tr("Most played")
                                                               : tr("History");
    // The history repeats songs: each plays once; files that are gone are left out.
    std::vector<::Track> tracks;
    std::unordered_set<std::string> seen;
    for (const QVariant &entry : list) {
        const QVariantMap song = entry.toMap();
        const std::string path = song.value(QStringLiteral("path")).toString().toStdString();
        if ((StreamResolver::isYouTube(path) || QFileInfo::exists(qs(path))) && seen.insert(path).second)
            tracks.push_back(trackOf(song));
    }
    const std::string clicked = list.value(index).toMap().value(QStringLiteral("path")).toString().toStdString();
    const auto at = std::find_if(tracks.begin(), tracks.end(), [&](const ::Track &t) { return t.path == clicked; });
    const int start = at == tracks.end() ? 0 : int(at - tracks.begin());
    if (!tracks.empty())
        m_playlist->playTitled(std::move(tracks), start, title, false);
}

void HistoryController::playMix()
{
    // All-time play counts, most played first
    std::unordered_map<std::string, int> counts;
    std::unordered_map<std::string, const ListeningHistory::Play *> latest;
    for (const ListeningHistory::Play &play : m_history->plays()) {
        ++counts[play.path];
        latest[play.path] = &play;
    }
    std::vector<std::pair<std::string, int>> ranked(counts.begin(), counts.end());
    std::sort(ranked.begin(), ranked.end(), [](const auto &a, const auto &b) { return a.second > b.second; });

    std::vector<::Track> tracks;
    std::unordered_set<std::string> seen;
    for (const auto &[path, n] : ranked) {
        if (int(tracks.size()) >= MixCount)
            break;
        if (!StreamResolver::isYouTube(path) && !QFileInfo::exists(qs(path)))
            continue;
        tracks.push_back(trackOf(songOf(*latest[path])));
        seen.insert(path);
    }
    std::mt19937 random(std::random_device{}());
    if (int(tracks.size()) < MixMinimum) {
        std::vector<::Track> library = m_library->tracks();
        std::shuffle(library.begin(), library.end(), random);
        for (const ::Track &t : library) {
            if (int(tracks.size()) >= MixMinimum)
                break;
            if (seen.insert(t.path).second)
                tracks.push_back(t);
        }
    }
    if (tracks.empty())
        return;
    std::shuffle(tracks.begin(), tracks.end(), random);
    m_playlist->playTitled(std::move(tracks), 0, tr("Your Mix"), false);
}
