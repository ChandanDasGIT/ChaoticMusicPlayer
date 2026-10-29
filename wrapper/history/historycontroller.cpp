#include "wrapper/history/historycontroller.h"

#include "backend/youtube/streamresolver/streamresolver.h"
#include "wrapper/player/playercontroller.h"
#include "wrapper/playlist/playlistcontroller.h"
#include "wrapper/playlists/playlistscontroller.h"
#include "wrapper/settings/settingscontroller.h"

#include <QDateTime>
#include <QJSEngine>
#include <QStandardPaths>

#include <algorithm>

namespace {

constexpr qint64 MinSongMs = 30 * 1000;      // shorter songs aren't counted
constexpr qint64 EnoughMs = 4 * 60 * 1000;   // listened this long: counted, even if not half
constexpr qint64 MaxStepMs = 1500;           // a bigger jump between positions is a seek, not listening

} // namespace

HistoryController::HistoryController(PlayerController *player, PlaylistController *playlist,
                                     PlaylistsController *playlists, SettingsController *settings, QObject *parent)
    : QObject(parent)
    , m_player(player)
    , m_playlist(playlist)
    , m_playlists(playlists)
    , m_settings(settings)
    , m_history(std::make_unique<ListeningHistory>(
          (QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
           + QStringLiteral("/ChaoticMusicPlayer/history.tsv"))
              .toStdString()))
{
    Q_ASSERT(!s_instance);
    s_instance = this;
    m_history->load();

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
    if (m_history->append(play))
        emit changed();
}

void HistoryController::clear()
{
    if (m_history->clear())
        emit changed();
}
