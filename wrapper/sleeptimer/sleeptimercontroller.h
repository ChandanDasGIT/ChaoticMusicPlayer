#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

class PlayerController;
class QJSEngine;
class QQmlEngine;

// The sleep timer (`SleepTimerController` singleton): pauses the music after a
// while, or at the end of the song playing. The music fades out over the last
// 10 seconds first; the volume itself is never changed.
class SleepTimerController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(bool endOfSong READ endOfSong NOTIFY changed)       // waits for the song to end
    Q_PROPERTY(qint64 remainingMs READ remainingMs NOTIFY tick)    // a timer for minutes: what is left

public:
    explicit SleepTimerController(PlayerController *player, QObject *parent);
    ~SleepTimerController() override;

    static SleepTimerController *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    bool active() const { return m_minutesMs > 0 || m_endOfSong; }
    bool endOfSong() const { return m_endOfSong; }
    qint64 remainingMs() const;

    Q_INVOKABLE void start(int minutes);
    Q_INVOKABLE void startEndOfSong();
    Q_INVOKABLE void cancel();

signals:
    void changed();
    void tick();

private:
    void onTick();
    void onPosition();
    void finish();

    static inline SleepTimerController *s_instance = nullptr;

    PlayerController *const m_player;
    QTimer m_ticker;          // updates the time left and the fade
    QElapsedTimer m_clock;
    qint64 m_minutesMs = 0;   // the length set (0 = no timer for minutes)
    bool m_endOfSong = false;
};
