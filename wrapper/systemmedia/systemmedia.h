#pragma once

#include <QObject>

#include <memory>

class PlayerController;
class PlaylistController;

// The system's media controls: media keys, the lock screen, the desktop's
// "now playing" panel. Each platform has its own systemmedia_<platform>.cpp
// (Linux: MPRIS over D-Bus).
class SystemMedia : public QObject
{
    Q_OBJECT

public:
    SystemMedia(PlayerController *player, PlaylistController *playlist, QObject *parent);
    ~SystemMedia() override;

signals:
    void raiseRequested(); // the system asked to bring the window to the front
    void quitRequested();  // the system asked the app to quit

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
