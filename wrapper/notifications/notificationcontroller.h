#pragma once

#include <QObject>
#include <QString>

class PlayerController;
class PlaylistController;
class SettingsController;

// Song-change notifications: when a new song starts playing while the app's
// window isn't in front, the desktop shows its title, artist and album, with
// the cover the app shows (embedded, found online, or YouTube's thumbnail,
// fetched in the background first). Each one replaces the one before.
// Settings can turn them off.
class NotificationController : public QObject
{
    Q_OBJECT

public:
    NotificationController(PlayerController *player, PlaylistController *playlist, SettingsController *settings,
                           QObject *parent);

private:
    void onPosition();
    void notify();
    void send(const QString &title, const QString &subtitle, const QString &coverFile);
    // The cover saved for the desktop to show, or "" (any thread)
    static QString saveCover(const QString &path, const QString &thumbnail);

    PlayerController *const m_player;
    PlaylistController *const m_playlist;
    SettingsController *const m_settings;
    bool m_pending = false; // a new song was loaded: tell once it plays
    uint m_lastId = 0;      // the notification to replace
};
