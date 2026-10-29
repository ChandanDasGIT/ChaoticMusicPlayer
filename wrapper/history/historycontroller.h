#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include <memory>

#include "backend/history/listeninghistory/listeninghistory.h"

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
class HistoryController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(int count READ count NOTIFY changed)

public:
    HistoryController(PlayerController *player, PlaylistController *playlist, PlaylistsController *playlists,
                      SettingsController *settings, QObject *parent);
    ~HistoryController() override;

    static HistoryController *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    int count() const { return int(m_history->plays().size()); }
    const std::vector<ListeningHistory::Play> &plays() const { return m_history->plays(); }

    Q_INVOKABLE void clear();

signals:
    void changed();

private:
    void startSong();
    void onPosition();
    void record();

    static inline HistoryController *s_instance = nullptr;

    PlayerController *const m_player;
    PlaylistController *const m_playlist;
    PlaylistsController *const m_playlists;
    SettingsController *const m_settings;
    std::unique_ptr<ListeningHistory> m_history;

    // The song being listened to
    QString m_path;
    qint64 m_listenedMs = 0;
    qint64 m_lastPosition = 0;
    bool m_counted = false;
};
