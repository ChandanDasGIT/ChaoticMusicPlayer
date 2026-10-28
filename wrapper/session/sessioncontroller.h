#pragma once

#include <QObject>
#include <QTimer>

class PlayerController;
class PlaylistController;
class PlaylistsController;
class SettingsController;

// "Resume where you left off": remembers the loaded song, its position and the
// list it plays from (Settings keeps them), and reopens it paused there on the
// next start. Saved when the song changes, on pause, every 15 s while playing
// (so a crash loses little) and when the app quits.
class SessionController : public QObject
{
    Q_OBJECT

public:
    SessionController(PlayerController *player, PlaylistController *playlist, PlaylistsController *playlists,
                      SettingsController *settings, QObject *parent);

    // Once at startup, after the Queue and the playlists are loaded.
    void restore();
    void save();

private:
    PlayerController *const m_player;
    PlaylistController *const m_playlist;
    PlaylistsController *const m_playlists;
    SettingsController *const m_settings;
    QTimer m_timer;
};
