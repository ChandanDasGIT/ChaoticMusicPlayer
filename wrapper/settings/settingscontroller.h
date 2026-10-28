#pragma once

#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <memory>

class QJSEngine;
class QQmlEngine;
class Settings;

// Persistent user settings (`SettingsController` singleton), stored in
// ~/.config/ChaoticMusicPlayer/settings.conf. Changes are saved shortly after
// they happen (debounced) and on exit.
class SettingsController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(qreal volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool playlistPinned READ playlistPinned WRITE setPlaylistPinned NOTIFY playlistPinnedChanged)
    // The YouTube and Library panels share the right-hand column: pinning one unpins the other.
    Q_PROPERTY(bool youtubePanelPinned READ youtubePanelPinned WRITE setYoutubePanelPinned NOTIFY youtubePanelPinnedChanged)
    Q_PROPERTY(bool libraryPanelPinned READ libraryPanelPinned WRITE setLibraryPanelPinned NOTIFY libraryPanelPinnedChanged)
    Q_PROPERTY(int artistAlbumWidth READ artistAlbumWidth WRITE setArtistAlbumWidth NOTIFY columnWidthsChanged)
    Q_PROPERTY(int trackWidth READ trackWidth WRITE setTrackWidth NOTIFY columnWidthsChanged)
    Q_PROPERTY(int durationWidth READ durationWidth WRITE setDurationWidth NOTIFY columnWidthsChanged)

    // Playback
    Q_PROPERTY(bool restorePlaylist READ restorePlaylist WRITE setRestorePlaylist NOTIFY restorePlaylistChanged)
    Q_PROPERTY(bool backgroundPlayback READ backgroundPlayback WRITE setBackgroundPlayback NOTIFY backgroundPlaybackChanged)
    Q_PROPERTY(QString themeMode READ themeMode WRITE setThemeMode NOTIFY themeModeChanged)
    Q_PROPERTY(bool resumeSession READ resumeSession WRITE setResumeSession NOTIFY resumeSessionChanged)
    Q_PROPERTY(int volumeStep READ volumeStep WRITE setVolumeStep NOTIFY volumeStepChanged)
    Q_PROPERTY(bool showRemainingTime READ showRemainingTime WRITE setShowRemainingTime NOTIFY showRemainingTimeChanged)
    // Seconds the end of a song overlaps the next one (0 = gapless)
    Q_PROPERTY(int crossfadeSeconds READ crossfadeSeconds WRITE setCrossfadeSeconds NOTIFY crossfadeSecondsChanged)
    // Lyrics: [{ id, name, enabled }] in the order they are tried
    Q_PROPERTY(QVariantList lyricsSources READ lyricsSources NOTIFY lyricsSourcesChanged)
    Q_PROPERTY(bool lyricsSyncedByDefault READ lyricsSyncedByDefault WRITE setLyricsSyncedByDefault NOTIFY lyricsSyncedByDefaultChanged)
    // Lyrics in English letters (romanized, same language) instead of their own script
    Q_PROPERTY(bool lyricsLatinScript READ lyricsLatinScript WRITE setLyricsLatinScript NOTIFY lyricsLatinScriptChanged)
    // YouTube
    Q_PROPERTY(bool ytBackgroundLookup READ ytBackgroundLookup WRITE setYtBackgroundLookup NOTIFY ytBackgroundLookupChanged)
    // Browser whose YouTube session yt-dlp reads ("" = not signed in)
    Q_PROPERTY(QString ytCookiesBrowser READ ytCookiesBrowser WRITE setYtCookiesBrowser NOTIFY ytCookiesBrowserChanged)
    // Album art for songs without one: look it up online for the playing song;
    // sources [{ id, name, enabled }] (all asked, the largest image wins)
    Q_PROPERTY(bool coversOnline READ coversOnline WRITE setCoversOnline NOTIFY coversOnlineChanged)
    Q_PROPERTY(QVariantList coverSources READ coverSources NOTIFY coverSourcesChanged)
    // Local library: folders scanned for music, and folders left out of them
    Q_PROPERTY(QStringList libraryFolders READ libraryFolders NOTIFY libraryFoldersChanged)
    Q_PROPERTY(QStringList libraryExcluded READ libraryExcluded NOTIFY libraryExcludedChanged)
    // Appearance: 0 = plain dark theme, 1 = full cover color
    Q_PROPERTY(qreal coverTintStrength READ coverTintStrength WRITE setCoverTintStrength NOTIFY coverTintStrengthChanged)
    // Visualizer under the volume bar; style: 0 = bars, 1 = mirrored bars, 2 = wave
    Q_PROPERTY(bool visualizerEnabled READ visualizerEnabled WRITE setVisualizerEnabled NOTIFY visualizerEnabledChanged)
    Q_PROPERTY(int visualizerStyle READ visualizerStyle WRITE setVisualizerStyle NOTIFY visualizerStyleChanged)

public:
    // No default argument: QML must use create(), not construct its own instance.
    explicit SettingsController(QObject *parent);
    ~SettingsController() override;

    static SettingsController *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    qreal volume() const;
    void setVolume(qreal volume);
    bool playlistPinned() const;
    void setPlaylistPinned(bool pinned);
    bool youtubePanelPinned() const;
    void setYoutubePanelPinned(bool pinned);
    bool libraryPanelPinned() const;
    void setLibraryPanelPinned(bool pinned);
    int artistAlbumWidth() const;
    void setArtistAlbumWidth(int width);
    int trackWidth() const;
    void setTrackWidth(int width);
    int durationWidth() const;
    void setDurationWidth(int width);

    bool restorePlaylist() const;    // load the last playlist on startup
    void setRestorePlaylist(bool restore);
    bool backgroundPlayback() const; // closing the window keeps the music playing
    void setBackgroundPlayback(bool enabled);
    QString themeMode() const; // "system" (follow the desktop), "dark" (default) or "light"
    void setThemeMode(const QString &mode);
    bool resumeSession() const; // reopen on the last song, paused where it was
    void setResumeSession(bool resume);

    // The last session (SessionController): song, position, length, and the
    // list it played from ("queue" or a playlist id).
    QString sessionPath() const;
    qint64 sessionPosition() const;
    qint64 sessionDuration() const;
    QString sessionList() const;
    void setSession(const QString &path, qint64 positionMs, qint64 durationMs, const QString &list);
    QString outputDevice() const;    // "" = system default; applied by main()
    void setOutputDevice(const QString &deviceId);
    int volumeStep() const;          // percent per mouse-wheel notch
    void setVolumeStep(int step);
    bool showRemainingTime() const;
    int crossfadeSeconds() const;
    void setCrossfadeSeconds(int seconds);
    void setShowRemainingTime(bool show);
    QVariantList lyricsSources() const;
    Q_INVOKABLE void moveLyricsSource(int index, int delta);
    Q_INVOKABLE void setLyricsSourceEnabled(int index, bool enabled);
    QStringList enabledLyricsSourceIds() const; // in the order they should be tried
    bool lyricsSyncedByDefault() const;
    void setLyricsSyncedByDefault(bool synced);
    bool lyricsLatinScript() const;
    void setLyricsLatinScript(bool latin);
    bool ytBackgroundLookup() const;
    QString ytCookiesBrowser() const;
    void setYtCookiesBrowser(const QString &browser);
    void setYtBackgroundLookup(bool enabled);
    bool coversOnline() const;
    void setCoversOnline(bool online);
    QVariantList coverSources() const;
    Q_INVOKABLE void setCoverSourceEnabled(const QString &id, bool enabled);
    QStringList enabledCoverSourceIds() const;
    QStringList libraryFolders() const;
    QStringList libraryExcluded() const;
    // Local paths or file:// URLs; return false if it was already there.
    Q_INVOKABLE bool addLibraryFolder(const QString &folder);
    Q_INVOKABLE void removeLibraryFolder(const QString &folder);
    Q_INVOKABLE bool addLibraryExcluded(const QString &folder);
    Q_INVOKABLE void removeLibraryExcluded(const QString &folder);
    qreal coverTintStrength() const;
    void setCoverTintStrength(qreal strength);
    bool visualizerEnabled() const;
    void setVisualizerEnabled(bool enabled);
    int visualizerStyle() const;
    void setVisualizerStyle(int style);

    // Used by PlaylistController (values of Playlist::SortColumn).
    int sortColumn() const;
    bool sortDescending() const;
    void setSort(int column, bool descending);
    bool shuffle() const;
    void setShuffle(bool shuffle);
    int repeatMode() const; // values of Playlist::RepeatMode
    void setRepeatMode(int mode);

signals:
    void volumeChanged();
    void playlistPinnedChanged();
    void youtubePanelPinnedChanged();
    void libraryPanelPinnedChanged();
    void columnWidthsChanged();
    void volumeStepChanged();
    void restorePlaylistChanged();
    void backgroundPlaybackChanged();
    void themeModeChanged();
    void resumeSessionChanged();
    void showRemainingTimeChanged();
    void crossfadeSecondsChanged();
    void lyricsSourcesChanged();
    void lyricsSyncedByDefaultChanged();
    void lyricsLatinScriptChanged();
    void ytBackgroundLookupChanged();
    void ytCookiesBrowserChanged();
    void coverTintStrengthChanged();
    void coversOnlineChanged();
    void coverSourcesChanged();
    void libraryFoldersChanged();
    void libraryExcludedChanged();
    void visualizerEnabledChanged();
    void visualizerStyleChanged();

private:
    void setWidth(const char *key, int width, int min, int max);
    QStringList pathList(const char *key) const;
    void setPathList(const char *key, const QStringList &paths);
    void scheduleSave();

    struct LyricsSource
    {
        QString id;
        bool enabled;
    };
    QList<LyricsSource> lyricsSourceList() const;
    void storeLyricsSources(const QList<LyricsSource> &sources);

    static inline SettingsController *s_instance = nullptr;

    std::unique_ptr<Settings> m_settings;
    QTimer m_saveTimer;
};
