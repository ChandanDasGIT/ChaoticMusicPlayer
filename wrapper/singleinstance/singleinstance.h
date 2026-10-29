#pragma once

#include <QObject>
#include <QString>

class QLocalServer;

// Keeps one running copy of the app per user. A second launch connects to the
// first copy's local socket ($XDG_RUNTIME_DIR/ChaoticMusicPlayer.sock), which
// then emits activationRequested() to bring its window forward.
class SingleInstance : public QObject
{
    Q_OBJECT

public:
    explicit SingleInstance(QObject *parent);
    ~SingleInstance() override;

    // Returns false when another copy is already running (it has been asked
    // to come to the front) and this process should exit.
    bool start();
    // Stops answering (a restart: the new copy must not find this one).
    void stop();

signals:
    void activationRequested();

private:
    QString m_socketPath;
    QLocalServer *m_server = nullptr;
};
