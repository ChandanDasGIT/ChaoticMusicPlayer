#include "wrapper/lyrics/lyricscontroller.h"

#include "backend/library/metadata/metadata.h"
#include "backend/lyrics/lyricsprovider/lyricsprovider.h"
#include "wrapper/player/playercontroller.h"
#include "wrapper/playlist/playlistcontroller.h"
#include "wrapper/settings/settingscontroller.h"

#include <QJSEngine>
#include <QStandardPaths>

#include <algorithm>

namespace {

// Show a line slightly before it is sung; reads more naturally.
constexpr qint64 LeadMs = 150;

QString displayName(const std::string &source)
{
    if (source == "local")
        return QStringLiteral("Local file");
    if (source == "lrclib")
        return QStringLiteral("LRCLIB");
    if (source == "ytmusic")
        return QStringLiteral("YouTube Music");
    return QString::fromStdString(source);
}

} // namespace

LyricsController::LyricsController(PlayerController *player, PlaylistController *playlist,
                                   SettingsController *settings, QObject *parent)
    : QObject(parent)
    , m_player(player)
    , m_playlist(playlist)
    , m_settings(settings)
{
    Q_ASSERT(!s_instance);
    s_instance = this;

    const QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
                             + QStringLiteral("/ChaoticMusicPlayer/lyrics");
    m_provider = std::make_shared<LyricsProvider>(cacheDir.toStdString());
    m_pool.setMaxThreadCount(1);

    connect(m_player, &PlayerController::sourceChanged, this, &LyricsController::onSourceChanged);
    connect(m_player, &PlayerController::positionChanged, this, &LyricsController::updateCurrentLine);
    connect(m_settings, &SettingsController::lyricsSourcesChanged, this, &LyricsController::reload);
    connect(m_settings, &SettingsController::lyricsLatinScriptChanged, this, &LyricsController::reload);
    onSourceChanged();
}

LyricsController::~LyricsController()
{
    m_pool.waitForDone(); // no task may post to a destroyed object
    s_instance = nullptr;
}

LyricsController *LyricsController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

void LyricsController::onSourceChanged()
{
    if (m_player->source() != m_path)
        reload();
}

void LyricsController::reload()
{
    m_path = m_player->source();
    const int generation = ++m_generation;

    m_lines.clear();
    m_times.clear();
    m_synced = false;
    m_sourceName.clear();
    m_currentLine = -1;
    m_status = m_path.isEmpty() ? NoTrack : Loading;
    emit lyricsChanged();
    emit currentLineChanged();
    if (m_path.isEmpty())
        return;

    // Use the playlist's metadata when it describes this file; otherwise read the tags.
    LyricsQuery query;
    query.path = m_path.toStdString();
    query.latinScript = m_settings->lyricsLatinScript();
    if (const Track *track = m_playlist->currentTrack(); track && track->path == query.path) {
        query.title = track->title;
        query.artist = track->artist;
        query.album = track->album;
        query.durationMs = track->durationMs;
    }

    std::vector<std::string> sources;
    for (const QString &id : m_settings->enabledLyricsSourceIds())
        sources.push_back(id.toStdString());

    m_pool.start([this, provider = m_provider, query, sources, generation]() mutable {
        if (query.title.empty()) {
            const Track track = Metadata::read(query.path);
            query.title = track.title;
            query.artist = track.artist;
            query.album = track.album;
            query.durationMs = track.durationMs;
        }
        const Lyrics lyrics = provider->fetch(query, sources);

        QMetaObject::invokeMethod(this, [this, generation, lyrics] {
            if (generation != m_generation)
                return; // the track changed meanwhile

            for (const LyricLine &line : lyrics.lines) {
                m_lines.append(QVariantMap{
                    {QStringLiteral("time"), qint64(line.timeMs)},
                    {QStringLiteral("text"), QString::fromStdString(line.text)},
                });
                m_times.push_back(line.timeMs);
            }
            m_synced = lyrics.synced;
            m_sourceName = displayName(lyrics.source);
            if (!lyrics.credit.empty())
                m_sourceName += QStringLiteral(" (%1)").arg(QString::fromStdString(lyrics.credit));
            m_status = lyrics.empty() ? NotFound : Found;
            emit lyricsChanged();
            updateCurrentLine();
        }, Qt::QueuedConnection);
    });
}

void LyricsController::updateCurrentLine()
{
    int line = -1;
    if (m_synced && !m_times.empty()) {
        const qint64 position = m_player->position() + LeadMs;
        line = int(std::upper_bound(m_times.begin(), m_times.end(), position) - m_times.begin()) - 1;
    }
    if (line == m_currentLine)
        return;
    m_currentLine = line;
    emit currentLineChanged();
}
