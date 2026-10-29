#pragma once

#include <QObject>
#include <QString>

class PlayerController;
class PlaylistController;
class SettingsController;

// Song-change notifications: when a new song starts playing while the app's
// window isn't in front, the desktop shows its title, artist and album (and
// the cover of local songs). Each one replaces the one before. Settings can
// turn them off.
class NotificationController : public QObject
{
    Q_OBJECT

public:
    NotificationController(PlayerController *player, PlaylistController *playlist, SettingsController *settings,
                           QObject *parent);

private:
    void onPosition();
    void notify();
    QString coverFile(const QString &path) const; // the song's cover saved for the desktop, or ""

    PlayerController *const m_player;
    PlaylistController *const m_playlist;
    SettingsController *const m_settings;
    bool m_pending = false; // a new song was loaded: tell once it plays
    uint m_lastId = 0;      // the notification to replace
};
