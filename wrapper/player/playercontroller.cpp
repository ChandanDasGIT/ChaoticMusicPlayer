#include "wrapper/player/playercontroller.h"

#include "backend/audioengine/audiooutput/audiooutput.h"
#include "backend/audioengine/decoder/decoder.h"
#include "backend/audioengine/player/player.h"
#include "backend/youtube/streamresolver/streamresolver.h"

#include <QFileInfo>
#include <QJSEngine>

namespace {

PlayerController::State toState(Player::State state)
{
    switch (state) {
    case Player::State::Playing: return PlayerController::Playing;
    case Player::State::Paused:  return PlayerController::Paused;
    case Player::State::Stopped: break;
    }
    return PlayerController::Stopped;
}

} // namespace

PlayerController::PlayerController(QObject *parent)
    : QObject(parent)
{
    Q_ASSERT(!s_instance);
    s_instance = this;

    // Runs `fn` on this object's (GUI) thread.
    auto queued = [this](auto fn) {
        QMetaObject::invokeMethod(this, std::move(fn), Qt::QueuedConnection);
    };

    Player::Callbacks callbacks;
    callbacks.stateChanged = [this, queued](Player::State s) {
        queued([this, s] { setState(toState(s)); });
    };
    callbacks.positionChanged = [this, queued](int64_t ms) {
        queued([this, ms] { setPosition(ms); });
    };
    callbacks.durationChanged = [this, queued](int64_t ms) {
        queued([this, ms] { setDuration(ms); });
    };
    callbacks.trackChanged = [this, queued](const std::string &path) {
        queued([this, p = QString::fromStdString(path)] {
            setErrorString({});
            m_source = p;
            emit sourceChanged();
            emit trackChanged(p);
        });
    };
    callbacks.trackFinished = [this, queued] {
        queued([this] { emit trackFinished(); });
    };
    callbacks.error = [this, queued](const std::string &message) {
        queued([this, error = QString::fromStdString(message)] { setErrorString(error); });
    };

    m_resolver = std::make_unique<StreamResolver>();
    m_player = std::make_unique<Player>("ChaoticMusicPlayer", std::move(callbacks), m_resolver.get());
    applyVolume();

    m_probePool.setMaxThreadCount(1);
    connect(this, &PlayerController::sourceChanged, this, &PlayerController::updateQuality);
    refreshOutputDevices();
}

PlayerController::~PlayerController()
{
    m_player.reset(); // joins the player thread before members go away
    m_probePool.waitForDone();
    s_instance = nullptr;
}

PlayerController *PlayerController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

void PlayerController::setVolume(qreal volume)
{
    volume = qBound(0.0, volume, 1.0);
    if (qFuzzyCompare(volume, m_volume))
        return;
    m_volume = volume;
    applyVolume();
    emit volumeChanged();
}

void PlayerController::setMuted(bool muted)
{
    if (muted == m_muted)
        return;
    m_muted = muted;
    applyVolume();
    emit mutedChanged();
}

// Fallback title when the playlist doesn't describe the track: the file name.
QString PlayerController::title() const
{
    if (StreamResolver::isYouTube(m_source.toStdString()))
        return tr("YouTube track");
    return QFileInfo(m_source).completeBaseName();
}

void PlayerController::load(const QString &path)
{
    setErrorString({});
    m_source = path;
    emit sourceChanged();
    setPosition(0);
    m_player->load(path.toStdString());
}

void PlayerController::play()
{
    m_player->play();
}

void PlayerController::pause()
{
    m_player->pause();
}

void PlayerController::togglePlayPause()
{
    if (m_state == Playing)
        pause();
    else
        play();
}

void PlayerController::stop()
{
    m_player->stop();
}

void PlayerController::seek(qint64 positionMs)
{
    setPosition(positionMs); // update the UI right away; the engine confirms shortly
    m_player->seek(positionMs);
    emit seeked(positionMs);
}

void PlayerController::unload()
{
    if (m_source.isEmpty())
        return;
    setErrorString({});
    m_source.clear();
    emit sourceChanged();
    setPosition(0);
    m_player->unload();
}

void PlayerController::setNext(const QString &path, int crossfadeMs)
{
    m_player->setNext(path.toStdString(), crossfadeMs);
}

void PlayerController::setSkipFade(bool enabled)
{
    m_player->setSkipFade(enabled);
}

void PlayerController::setOutputDevice(const QString &deviceId)
{
    if (deviceId == m_outputDevice)
        return;
    m_outputDevice = deviceId;
    m_player->setOutputDevice(deviceId.toStdString());
    emit outputDeviceChanged();
}

void PlayerController::setYtCookiesBrowser(const QString &browser)
{
    m_resolver->setCookiesBrowser(browser.toStdString());
}

void PlayerController::refreshOutputDevices()
{
    m_probePool.start([this] {
        QVariantList devices{QVariantMap{
            {QStringLiteral("id"), QString()},
            {QStringLiteral("name"), tr("System default")},
        }};
        for (const AudioDevice &device : AudioOutput::listDevices()) {
            devices.append(QVariantMap{
                {QStringLiteral("id"), QString::fromStdString(device.id)},
                {QStringLiteral("name"), QString::fromStdString(device.name)},
            });
        }
        QMetaObject::invokeMethod(this, [this, devices] {
            m_outputDevices = devices;
            emit outputDevicesChanged();
        }, Qt::QueuedConnection);
    });
}

// Probes the file's format on a worker thread and builds the quality chip text.
void PlayerController::updateQuality()
{
    const int generation = ++m_qualityGeneration;
    const QString path = m_source;
    if (path.isEmpty()) {
        m_qualityText.clear();
        emit qualityTextChanged();
        return;
    }

    m_probePool.start([this, path, generation, resolver = m_resolver.get()] {
        AudioInfo info;
        QStringList parts;
        // YouTube tracks: probe the stream (its address is cached by then).
        const std::optional<std::string> source = resolver->resolve(path.toStdString());
        if (source && Decoder::probe(*source, info)) {
            if (info.sampleRate > 0)
                parts << QStringLiteral("%1 kHz").arg(info.sampleRate / 1000.0, 0, 'g', 3);
            if (info.bitrate > 0)
                parts << QStringLiteral("%1 kbps").arg((info.bitrate + 500) / 1000);
            if (!info.codecName.empty())
                parts << QString::fromStdString(info.codecName);
        }
        const QString text = parts.join(QStringLiteral(" · "));
        QMetaObject::invokeMethod(this, [this, generation, text] {
            if (generation != m_qualityGeneration || text == m_qualityText)
                return;
            m_qualityText = text;
            emit qualityTextChanged();
        }, Qt::QueuedConnection);
    });
}

void PlayerController::setState(State state)
{
    if (state == m_state)
        return;
    m_state = state;
    emit stateChanged();
}

void PlayerController::setPosition(qint64 position)
{
    if (position == m_position)
        return;
    m_position = position;
    emit positionChanged();
}

void PlayerController::setDuration(qint64 duration)
{
    if (duration == m_duration)
        return;
    m_duration = duration;
    emit durationChanged();
}

void PlayerController::setErrorString(const QString &error)
{
    if (error == m_errorString)
        return;
    m_errorString = error;
    emit errorStringChanged();
}

void PlayerController::applyVolume()
{
    m_player->setVolume(m_muted ? 0.0f : float(m_volume));
}

SpectrumAnalyzer &PlayerController::spectrum()
{
    return m_player->spectrum();
}
