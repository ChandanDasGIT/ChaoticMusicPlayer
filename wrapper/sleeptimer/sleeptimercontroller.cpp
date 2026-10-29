#include "wrapper/sleeptimer/sleeptimercontroller.h"

#include "wrapper/player/playercontroller.h"

#include <QJSEngine>

#include <algorithm>

namespace {

constexpr qint64 FadeMs = 10 * 1000; // the music fades out over the last 10 s
constexpr qint64 EndMarginMs = 400;  // "end of song": this close to the end counts as the end

} // namespace

SleepTimerController::SleepTimerController(PlayerController *player, QObject *parent)
    : QObject(parent)
    , m_player(player)
{
    Q_ASSERT(!s_instance);
    s_instance = this;
    m_ticker.setInterval(250);
    connect(&m_ticker, &QTimer::timeout, this, &SleepTimerController::onTick);
    connect(m_player, &PlayerController::positionChanged, this, &SleepTimerController::onPosition);
}

SleepTimerController::~SleepTimerController()
{
    s_instance = nullptr;
}

SleepTimerController *SleepTimerController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

qint64 SleepTimerController::remainingMs() const
{
    if (m_endOfSong)
        return std::max<qint64>(0, m_player->duration() - m_player->position());
    return m_minutesMs > 0 ? std::max<qint64>(0, m_minutesMs - m_clock.elapsed()) : 0;
}

void SleepTimerController::start(int minutes)
{
    m_endOfSong = false;
    m_minutesMs = qint64(std::max(1, minutes)) * 60 * 1000;
    m_clock.start();
    m_player->setFadeLevel(1);
    m_ticker.start();
    emit changed();
    emit tick();
}

void SleepTimerController::startEndOfSong()
{
    m_minutesMs = 0;
    m_endOfSong = true;
    m_player->setFadeLevel(1);
    m_ticker.start();
    emit changed();
    emit tick();
}

void SleepTimerController::cancel()
{
    if (!active())
        return;
    m_minutesMs = 0;
    m_endOfSong = false;
    m_ticker.stop();
    m_player->setFadeLevel(1);
    emit changed();
    emit tick();
}

void SleepTimerController::onTick()
{
    emit tick();
    if (m_endOfSong)
        return; // follows the song (onPosition)
    const qint64 left = remainingMs();
    m_player->setFadeLevel(left < FadeMs ? qreal(left) / FadeMs : 1.0);
    if (left <= 0)
        finish();
}

void SleepTimerController::onPosition()
{
    if (!m_endOfSong || !m_player->playing())
        return;
    const qint64 left = m_player->duration() - m_player->position();
    if (m_player->duration() > 0 && left <= EndMarginMs)
        finish();
}

// Time's up: pause, then the level comes back for the next time music plays.
void SleepTimerController::finish()
{
    m_player->pause();
    m_minutesMs = 0;
    m_endOfSong = false;
    m_ticker.stop();
    m_player->setFadeLevel(1);
    emit changed();
    emit tick();
}
