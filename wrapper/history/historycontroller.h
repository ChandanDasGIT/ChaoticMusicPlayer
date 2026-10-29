#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <memory>

#include "backend/history/listeninghistory/listeninghistory.h"
#include "backend/library/track/track.h"

class LocalLibraryController;
class PlayerController;
class PlaylistController;
class PlaylistsController;
class QJSEngine;
class QQmlEngine;
class SettingsController;

// The private listening history (`HistoryController` singleton). Watches what
// plays and records a song once it has been listened to for half its length
// or 4 minutes (whichever comes first; songs under 30 s don't count). Only
// time actually listened counts: seeking ahead or pausing adds nothing.
// Settings turn it off, or leave YouTube Music songs out.
//
// For Home it also offers what was played lately, the most played songs of
// the last 30 days, recent albums, the most played YouTube song (for its Mix)
// and the whole history by day, and plays them as lists.
class HistoryController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(int count READ count NOTIFY changed)
    // Songs: [{ path, title, artist, album, thumbnail, durationMs, item, ... }]; `item`
    // is what the Queue / playlists take for it (drag and drop, menus).
    Q_PROPERTY(QVariantList recent READ recent NOTIFY changed)            // different songs, latest first
    Q_PROPERTY(QVariantList mostPlayed READ mostPlayed NOTIFY changed)    // last 30 days, + plays
    Q_PROPERTY(QVariantList recentAlbums READ recentAlbums NOTIFY changed) // [{ album, artist, path }]
    Q_PROPERTY(QVariantMap favoriteYouTube READ favoriteYouTube NOTIFY changed) // a YouTube song map, or empty
    Q_PROPERTY(QVariantList entries READ entries NOTIFY changed)          // every play, latest first, + day, clock
    // Numbers about the library and the listening: { songs, albums, artists, libraryMs, plays,
    // playsThisMonth, listenedMs, youTubePlays, topArtists, topAlbums, topSongs ([{ name, plays }]) }
    Q_PROPERTY(QVariantMap stats READ stats NOTIFY statsChanged)

public:
    HistoryController(PlayerController *player, PlaylistController *playlist, PlaylistsController *playlists,
                      LocalLibraryController *library, SettingsController *settings, QObject *parent);
    ~HistoryController() override;

    static HistoryController *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    int count() const { return int(m_history->plays().size()); }
    const std::vector<ListeningHistory::Play> &plays() const { return m_history->plays(); }

    QVariantList recent() const { return m_recent; }
    QVariantList mostPlayed() const { return m_mostPlayed; }
    QVariantList recentAlbums() const { return m_recentAlbums; }
    QVariantMap favoriteYouTube() const { return m_favoriteYouTube; }
    QVariantList entries() const { return m_entries; }
    QVariantMap stats() const { return m_stats; }

    Q_INVOKABLE void clear();
    Q_INVOKABLE void removeEntry(int index); // one play from `entries`
    // Plays "recent", "mostPlayed" or "entries" from that song (without touching the Queue).
    Q_INVOKABLE void playList(const QString &which, int index);
    // "Your Mix": the most played songs, shuffled (topped up from the library while the history is short).
    Q_INVOKABLE void playMix();

signals:
    void changed();
    void statsChanged();

private:
    void startSong();
    void onPosition();
    void record();
    void rebuild(); // the lists above
    void rebuildStats();
    ::Track trackOf(const QVariantMap &song) const;

    static inline HistoryController *s_instance = nullptr;

    PlayerController *const m_player;
    PlaylistController *const m_playlist;
    PlaylistsController *const m_playlists;
    LocalLibraryController *const m_library;
    SettingsController *const m_settings;
    std::unique_ptr<ListeningHistory> m_history;
    QVariantList m_recent;
    QVariantList m_mostPlayed;
    QVariantList m_recentAlbums;
    QVariantMap m_favoriteYouTube;
    QVariantList m_entries;
    QVariantMap m_stats;

    // The song being listened to
    QString m_path;
    qint64 m_listenedMs = 0;
    qint64 m_lastPosition = 0;
    bool m_counted = false;
};
