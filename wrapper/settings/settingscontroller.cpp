#include "wrapper/settings/settingscontroller.h"

#include "backend/storage/tsv/tsv.h"

#include <QDir>
#include <QUrl>

#include "backend/settings/settings/settings.h"

#include <QJSEngine>

#include <algorithm>
#include <QStandardPaths>

namespace {

constexpr const char *VolumeKey = "volume";
constexpr const char *PinnedKey = "playlistPinned";
constexpr const char *YoutubePinnedKey = "youtube.panelPinned";
constexpr const char *LibraryPinnedKey = "library.panelPinned";
constexpr const char *ArtistAlbumWidthKey = "column.artistAlbum.width";
constexpr const char *TrackWidthKey = "column.track.width";
constexpr const char *DurationWidthKey = "column.duration.width";
constexpr const char *SortColumnKey = "sort.column";
constexpr const char *SortDescendingKey = "sort.descending";
constexpr const char *ShuffleKey = "shuffle";
constexpr const char *RepeatModeKey = "repeat";
constexpr const char *VolumeStepKey = "volume.step";
constexpr const char *RestorePlaylistKey = "playlist.restoreOnStartup";
constexpr const char *BackgroundPlaybackKey = "playback.background";
constexpr const char *ThemeModeKey = "appearance.theme";
constexpr const char *ResumeKey = "playback.resume";
constexpr const char *SessionPathKey = "session.path";
constexpr const char *SessionPositionKey = "session.positionMs";
constexpr const char *SessionDurationKey = "session.durationMs";
constexpr const char *SessionListKey = "session.list";
constexpr const char *OldDarkModeKey = "appearance.dark"; // before "Follow system": a dark / light switch
constexpr const char *OutputDeviceKey = "audio.outputDevice";
constexpr const char *ShowRemainingKey = "seekbar.showRemaining";
constexpr const char *CrossfadeKey = "playback.crossfadeSeconds";
constexpr int MaxCrossfadeSeconds = 12;
constexpr const char *LyricsSourcesKey = "lyrics.sources";
constexpr const char *LyricsSyncedKey = "lyrics.syncedByDefault";
constexpr const char *LyricsLatinKey = "lyrics.latinScript";
constexpr const char *YtLookupKey = "youtube.backgroundLookup";
constexpr const char *YtAutoplayKey = "youtube.autoplay";
constexpr const char *YtQueueMixKey = "youtube.queueMix";
constexpr const char *YtCookiesKey = "youtube.cookiesBrowser";
constexpr const char *TintStrengthKey = "appearance.tintStrength";
constexpr const char *CoversOnlineKey = "covers.online";
constexpr const char *CoversDisabledKey = "covers.disabledSources"; // comma-separated ids
const QList<QPair<QString, QString>> CoverSourceNames = {
    {QStringLiteral("folder"), QStringLiteral("Images in the song's folder (cover.jpg …)")},
    {QStringLiteral("itunes"), QStringLiteral("iTunes (Apple)")},
    {QStringLiteral("musicbrainz"), QStringLiteral("MusicBrainz (Cover Art Archive)")},
    {QStringLiteral("ytmusic"), QStringLiteral("YouTube Music")},
};
constexpr const char *LibraryFoldersKey = "library.folders";   // tab-separated, tsv-escaped
constexpr const char *LibraryExcludedKey = "library.excluded";

// "file:///home/me/Music/" or "/home/me/Music/" -> "/home/me/Music"
QString cleanFolder(const QString &folder)
{
    QString path = folder.startsWith(QStringLiteral("file:")) ? QUrl(folder).toLocalFile() : folder;
    path = QDir::cleanPath(path);
    return path;
}
constexpr const char *VisualizerKey = "appearance.visualizer";
constexpr const char *VisualizerStyleKey = "appearance.visualizerStyle";
constexpr int VisualizerStyleCount = 3;

// id -> display name, in the default order
const QList<QPair<QString, QString>> LyricsSourceNames = {
    {QStringLiteral("local"), QStringLiteral("Local files (.lrc / tags)")},
    {QStringLiteral("lrclib"), QStringLiteral("LRCLIB (online)")},
    {QStringLiteral("ytmusic"), QStringLiteral("YouTube Music")},
};

} // namespace

SettingsController::SettingsController(QObject *parent)
    : QObject(parent)
{
    Q_ASSERT(!s_instance);
    s_instance = this;

    const QString file = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
                         + QStringLiteral("/ChaoticMusicPlayer/settings.conf");
    m_settings = std::make_unique<Settings>(file.toStdString());
    m_settings->load();

    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(500);
    connect(&m_saveTimer, &QTimer::timeout, this, [this] { m_settings->save(); });
}

SettingsController::~SettingsController()
{
    m_settings->save();
    s_instance = nullptr;
}

SettingsController *SettingsController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

qreal SettingsController::volume() const
{
    return qBound(0.0, m_settings->doubleValue(VolumeKey, 0.8), 1.0);
}

void SettingsController::setVolume(qreal volume)
{
    if (qFuzzyCompare(volume, this->volume()))
        return;
    m_settings->setDouble(VolumeKey, volume);
    scheduleSave();
    emit volumeChanged();
}

bool SettingsController::playlistPinned() const
{
    return m_settings->boolValue(PinnedKey, true); // docked by default on big windows
}

void SettingsController::setPlaylistPinned(bool pinned)
{
    if (pinned == playlistPinned())
        return;
    m_settings->setBool(PinnedKey, pinned);
    scheduleSave();
    emit playlistPinnedChanged();
}

bool SettingsController::youtubePanelPinned() const
{
    return m_settings->boolValue(YoutubePinnedKey, false);
}

void SettingsController::setYoutubePanelPinned(bool pinned)
{
    if (pinned == youtubePanelPinned())
        return;
    m_settings->setBool(YoutubePinnedKey, pinned);
    scheduleSave();
    emit youtubePanelPinnedChanged();
    if (pinned)
        setLibraryPanelPinned(false);
}

bool SettingsController::libraryPanelPinned() const
{
    return m_settings->boolValue(LibraryPinnedKey, false);
}

void SettingsController::setLibraryPanelPinned(bool pinned)
{
    if (pinned == libraryPanelPinned())
        return;
    m_settings->setBool(LibraryPinnedKey, pinned);
    scheduleSave();
    emit libraryPanelPinnedChanged();
    if (pinned)
        setYoutubePanelPinned(false);
}

int SettingsController::artistAlbumWidth() const
{
    return m_settings->intValue(ArtistAlbumWidthKey, 220);
}

void SettingsController::setArtistAlbumWidth(int width)
{
    setWidth(ArtistAlbumWidthKey, width, 60, 800);
}

int SettingsController::trackWidth() const
{
    return m_settings->intValue(TrackWidthKey, 52);
}

void SettingsController::setTrackWidth(int width)
{
    setWidth(TrackWidthKey, width, 36, 160);
}

int SettingsController::durationWidth() const
{
    return m_settings->intValue(DurationWidthKey, 60);
}

void SettingsController::setDurationWidth(int width)
{
    setWidth(DurationWidthKey, width, 44, 160);
}

int SettingsController::sortColumn() const
{
    return m_settings->intValue(SortColumnKey, 2); // Playlist::SortColumn::Track
}

bool SettingsController::sortDescending() const
{
    return m_settings->boolValue(SortDescendingKey, false);
}

void SettingsController::setSort(int column, bool descending)
{
    m_settings->setInt(SortColumnKey, column);
    m_settings->setBool(SortDescendingKey, descending);
    scheduleSave();
}

bool SettingsController::restorePlaylist() const
{
    return m_settings->boolValue(RestorePlaylistKey, true);
}

void SettingsController::setRestorePlaylist(bool restore)
{
    if (restore == restorePlaylist())
        return;
    m_settings->setBool(RestorePlaylistKey, restore);
    scheduleSave();
    emit restorePlaylistChanged();
}

bool SettingsController::backgroundPlayback() const
{
    return m_settings->boolValue(BackgroundPlaybackKey, false);
}

void SettingsController::setBackgroundPlayback(bool enabled)
{
    if (enabled == backgroundPlayback())
        return;
    m_settings->setBool(BackgroundPlaybackKey, enabled);
    scheduleSave();
    emit backgroundPlaybackChanged();
}

bool SettingsController::resumeSession() const
{
    return m_settings->boolValue(ResumeKey, true);
}

void SettingsController::setResumeSession(bool resume)
{
    if (resume == resumeSession())
        return;
    m_settings->setBool(ResumeKey, resume);
    scheduleSave();
    emit resumeSessionChanged();
}

QString SettingsController::sessionPath() const
{
    return QString::fromStdString(m_settings->value(SessionPathKey));
}

qint64 SettingsController::sessionPosition() const
{
    return QString::fromStdString(m_settings->value(SessionPositionKey)).toLongLong();
}

qint64 SettingsController::sessionDuration() const
{
    return QString::fromStdString(m_settings->value(SessionDurationKey)).toLongLong();
}

QString SettingsController::sessionList() const
{
    return QString::fromStdString(m_settings->value(SessionListKey));
}

void SettingsController::setSession(const QString &path, qint64 positionMs, qint64 durationMs, const QString &list)
{
    const std::string position = std::to_string(positionMs);
    if (path == sessionPath() && m_settings->value(SessionPositionKey) == position && list == sessionList())
        return;
    m_settings->setValue(SessionPathKey, path.toStdString());
    m_settings->setValue(SessionPositionKey, position);
    m_settings->setValue(SessionDurationKey, std::to_string(durationMs));
    m_settings->setValue(SessionListKey, list.toStdString());
    scheduleSave();
}

QString SettingsController::themeMode() const
{
    const QString mode = QString::fromStdString(m_settings->value(ThemeModeKey));
    if (mode == QLatin1String("system") || mode == QLatin1String("dark") || mode == QLatin1String("light"))
        return mode;
    return m_settings->boolValue(OldDarkModeKey, true) ? QStringLiteral("dark") : QStringLiteral("light");
}

void SettingsController::setThemeMode(const QString &mode)
{
    if (mode == themeMode() || (mode != QLatin1String("system") && mode != QLatin1String("dark")
                                && mode != QLatin1String("light")))
        return;
    m_settings->setValue(ThemeModeKey, mode.toStdString());
    scheduleSave();
    emit themeModeChanged();
}

QString SettingsController::outputDevice() const
{
    return QString::fromStdString(m_settings->value(OutputDeviceKey));
}

void SettingsController::setOutputDevice(const QString &deviceId)
{
    if (deviceId == outputDevice())
        return;
    m_settings->setValue(OutputDeviceKey, deviceId.toStdString());
    scheduleSave();
}

int SettingsController::volumeStep() const
{
    return qBound(1, m_settings->intValue(VolumeStepKey, 5), 25);
}

void SettingsController::setVolumeStep(int step)
{
    step = qBound(1, step, 25);
    if (step == volumeStep())
        return;
    m_settings->setInt(VolumeStepKey, step);
    scheduleSave();
    emit volumeStepChanged();
}

bool SettingsController::showRemainingTime() const
{
    return m_settings->boolValue(ShowRemainingKey, true);
}

void SettingsController::setShowRemainingTime(bool show)
{
    if (show == showRemainingTime())
        return;
    m_settings->setBool(ShowRemainingKey, show);
    scheduleSave();
    emit showRemainingTimeChanged();
}

// Stored as "local:1,lrclib:1,ytmusic:0"; unknown ids are dropped and
// missing ones appended (enabled), so new sources show up after updates.
QList<SettingsController::LyricsSource> SettingsController::lyricsSourceList() const
{
    QList<LyricsSource> list;
    const QString stored = QString::fromStdString(m_settings->value(LyricsSourcesKey));
    for (const QString &entry : stored.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const QString id = entry.section(QLatin1Char(':'), 0, 0);
        const bool known = std::any_of(LyricsSourceNames.begin(), LyricsSourceNames.end(),
                                       [&](const auto &pair) { return pair.first == id; });
        const bool duplicate = std::any_of(list.begin(), list.end(),
                                           [&](const LyricsSource &s) { return s.id == id; });
        if (known && !duplicate)
            list.append({id, entry.section(QLatin1Char(':'), 1, 1) != QLatin1String("0")});
    }
    for (const auto &[id, name] : LyricsSourceNames) {
        if (std::none_of(list.begin(), list.end(), [&](const LyricsSource &s) { return s.id == id; }))
            list.append({id, true});
    }
    return list;
}

void SettingsController::storeLyricsSources(const QList<LyricsSource> &sources)
{
    QStringList parts;
    for (const LyricsSource &source : sources)
        parts << source.id + (source.enabled ? QStringLiteral(":1") : QStringLiteral(":0"));
    m_settings->setValue(LyricsSourcesKey, parts.join(QLatin1Char(',')).toStdString());
    scheduleSave();
    emit lyricsSourcesChanged();
}

QVariantList SettingsController::lyricsSources() const
{
    QVariantList result;
    for (const LyricsSource &source : lyricsSourceList()) {
        QString name;
        for (const auto &[id, displayName] : LyricsSourceNames) {
            if (id == source.id)
                name = displayName;
        }
        result.append(QVariantMap{
            {QStringLiteral("id"), source.id},
            {QStringLiteral("name"), name},
            {QStringLiteral("enabled"), source.enabled},
        });
    }
    return result;
}

QStringList SettingsController::enabledLyricsSourceIds() const
{
    QStringList ids;
    for (const LyricsSource &source : lyricsSourceList()) {
        if (source.enabled)
            ids << source.id;
    }
    return ids;
}

void SettingsController::moveLyricsSource(int index, int delta)
{
    QList<LyricsSource> sources = lyricsSourceList();
    const int target = index + delta;
    if (index < 0 || index >= sources.size() || target < 0 || target >= sources.size())
        return;
    sources.move(index, target);
    storeLyricsSources(sources);
}

void SettingsController::setLyricsSourceEnabled(int index, bool enabled)
{
    QList<LyricsSource> sources = lyricsSourceList();
    if (index < 0 || index >= sources.size() || sources[index].enabled == enabled)
        return;
    sources[index].enabled = enabled;
    storeLyricsSources(sources);
}

bool SettingsController::lyricsSyncedByDefault() const
{
    return m_settings->boolValue(LyricsSyncedKey, true);
}

void SettingsController::setLyricsSyncedByDefault(bool synced)
{
    if (synced == lyricsSyncedByDefault())
        return;
    m_settings->setBool(LyricsSyncedKey, synced);
    scheduleSave();
    emit lyricsSyncedByDefaultChanged();
}

int SettingsController::crossfadeSeconds() const
{
    return qBound(0, m_settings->intValue(CrossfadeKey, 4), MaxCrossfadeSeconds);
}

void SettingsController::setCrossfadeSeconds(int seconds)
{
    seconds = qBound(0, seconds, MaxCrossfadeSeconds);
    if (seconds == crossfadeSeconds())
        return;
    m_settings->setInt(CrossfadeKey, seconds);
    scheduleSave();
    emit crossfadeSecondsChanged();
}

bool SettingsController::lyricsLatinScript() const
{
    return m_settings->boolValue(LyricsLatinKey, false);
}

void SettingsController::setLyricsLatinScript(bool latin)
{
    if (latin == lyricsLatinScript())
        return;
    m_settings->setBool(LyricsLatinKey, latin);
    scheduleSave();
    emit lyricsLatinScriptChanged();
}

bool SettingsController::ytBackgroundLookup() const
{
    return m_settings->boolValue(YtLookupKey, true);
}

void SettingsController::setYtBackgroundLookup(bool enabled)
{
    if (enabled == ytBackgroundLookup())
        return;
    m_settings->setBool(YtLookupKey, enabled);
    scheduleSave();
    emit ytBackgroundLookupChanged();
}

bool SettingsController::ytAutoplay() const
{
    return m_settings->boolValue(YtAutoplayKey, true);
}

void SettingsController::setYtAutoplay(bool enabled)
{
    if (enabled == ytAutoplay())
        return;
    m_settings->setBool(YtAutoplayKey, enabled);
    scheduleSave();
    emit ytAutoplayChanged();
}

bool SettingsController::ytQueueMix() const
{
    return m_settings->boolValue(YtQueueMixKey, true);
}

void SettingsController::setYtQueueMix(bool enabled)
{
    if (enabled == ytQueueMix())
        return;
    m_settings->setBool(YtQueueMixKey, enabled);
    scheduleSave();
    emit ytQueueMixChanged();
}

QString SettingsController::ytCookiesBrowser() const
{
    return QString::fromStdString(m_settings->value(YtCookiesKey));
}

void SettingsController::setYtCookiesBrowser(const QString &browser)
{
    if (browser == ytCookiesBrowser())
        return;
    m_settings->setValue(YtCookiesKey, browser.toStdString());
    scheduleSave();
    emit ytCookiesBrowserChanged();
}

qreal SettingsController::coverTintStrength() const
{
    return qBound(0.0, m_settings->doubleValue(TintStrengthKey, 1.0), 1.0);
}

void SettingsController::setCoverTintStrength(qreal strength)
{
    strength = qBound(0.0, strength, 1.0);
    if (qFuzzyCompare(strength + 1, coverTintStrength() + 1))
        return;
    m_settings->setDouble(TintStrengthKey, strength);
    scheduleSave();
    emit coverTintStrengthChanged();
}

bool SettingsController::coversOnline() const
{
    return m_settings->boolValue(CoversOnlineKey, true);
}

void SettingsController::setCoversOnline(bool online)
{
    if (online == coversOnline())
        return;
    m_settings->setBool(CoversOnlineKey, online);
    scheduleSave();
    emit coversOnlineChanged();
}

QVariantList SettingsController::coverSources() const
{
    const QStringList disabled = QString::fromStdString(m_settings->value(CoversDisabledKey)).split(QLatin1Char(','));
    QVariantList result;
    for (const auto &[id, name] : CoverSourceNames)
        result.append(QVariantMap{{QStringLiteral("id"), id}, {QStringLiteral("name"), name},
                                  {QStringLiteral("enabled"), !disabled.contains(id)}});
    return result;
}

void SettingsController::setCoverSourceEnabled(const QString &id, bool enabled)
{
    QStringList disabled = QString::fromStdString(m_settings->value(CoversDisabledKey)).split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (enabled == !disabled.contains(id))
        return;
    if (enabled)
        disabled.removeAll(id);
    else
        disabled << id;
    m_settings->setValue(CoversDisabledKey, disabled.join(QLatin1Char(',')).toStdString());
    scheduleSave();
    emit coverSourcesChanged();
}

QStringList SettingsController::enabledCoverSourceIds() const
{
    QStringList ids;
    for (const QVariant &source : coverSources()) {
        const QVariantMap map = source.toMap();
        if (map.value(QStringLiteral("enabled")).toBool())
            ids << map.value(QStringLiteral("id")).toString();
    }
    return ids;
}

QStringList SettingsController::pathList(const char *key) const
{
    QStringList paths;
    for (const std::string &field : tsv::split(m_settings->value(key))) {
        if (!field.empty())
            paths << QString::fromStdString(tsv::unescape(field));
    }
    return paths;
}

void SettingsController::setPathList(const char *key, const QStringList &paths)
{
    std::string joined;
    for (const QString &path : paths) {
        if (!joined.empty())
            joined += '\t';
        joined += tsv::escape(path.toStdString());
    }
    m_settings->setValue(key, joined);
    scheduleSave();
}

QStringList SettingsController::libraryFolders() const
{
    return pathList(LibraryFoldersKey);
}

QStringList SettingsController::libraryExcluded() const
{
    return pathList(LibraryExcludedKey);
}

bool SettingsController::addLibraryFolder(const QString &folder)
{
    const QString path = cleanFolder(folder);
    QStringList folders = libraryFolders();
    if (path.isEmpty() || folders.contains(path))
        return false;
    folders << path;
    setPathList(LibraryFoldersKey, folders);
    emit libraryFoldersChanged();
    return true;
}

void SettingsController::removeLibraryFolder(const QString &folder)
{
    QStringList folders = libraryFolders();
    if (folders.removeAll(cleanFolder(folder)) == 0)
        return;
    setPathList(LibraryFoldersKey, folders);
    emit libraryFoldersChanged();
}

bool SettingsController::addLibraryExcluded(const QString &folder)
{
    const QString path = cleanFolder(folder);
    QStringList excluded = libraryExcluded();
    if (path.isEmpty() || excluded.contains(path))
        return false;
    excluded << path;
    setPathList(LibraryExcludedKey, excluded);
    emit libraryExcludedChanged();
    return true;
}

void SettingsController::removeLibraryExcluded(const QString &folder)
{
    QStringList excluded = libraryExcluded();
    if (excluded.removeAll(cleanFolder(folder)) == 0)
        return;
    setPathList(LibraryExcludedKey, excluded);
    emit libraryExcludedChanged();
}

bool SettingsController::visualizerEnabled() const
{
    return m_settings->boolValue(VisualizerKey, true);
}

void SettingsController::setVisualizerEnabled(bool enabled)
{
    if (enabled == visualizerEnabled())
        return;
    m_settings->setBool(VisualizerKey, enabled);
    scheduleSave();
    emit visualizerEnabledChanged();
}

int SettingsController::visualizerStyle() const
{
    return qBound(0, m_settings->intValue(VisualizerStyleKey, 0), VisualizerStyleCount - 1);
}

void SettingsController::setVisualizerStyle(int style)
{
    style = qBound(0, style, VisualizerStyleCount - 1);
    if (style == visualizerStyle())
        return;
    m_settings->setInt(VisualizerStyleKey, style);
    scheduleSave();
    emit visualizerStyleChanged();
}

bool SettingsController::shuffle() const
{
    return m_settings->boolValue(ShuffleKey, false);
}

void SettingsController::setShuffle(bool shuffle)
{
    m_settings->setBool(ShuffleKey, shuffle);
    scheduleSave();
}

int SettingsController::repeatMode() const
{
    return qBound(0, m_settings->intValue(RepeatModeKey, 0), 2);
}

void SettingsController::setRepeatMode(int mode)
{
    m_settings->setInt(RepeatModeKey, mode);
    scheduleSave();
}

void SettingsController::setWidth(const char *key, int width, int min, int max)
{
    width = qBound(min, width, max);
    if (width == m_settings->intValue(key, -1))
        return;
    m_settings->setInt(key, width);
    scheduleSave();
    emit columnWidthsChanged();
}

void SettingsController::scheduleSave()
{
    m_saveTimer.start();
}
