#include "wrapper/visualizer/visualizercontroller.h"

#include "backend/audioengine/spectrum/spectrum.h"
#include "wrapper/player/playercontroller.h"
#include "wrapper/settings/settingscontroller.h"

#include <QJSEngine>

namespace {

constexpr int FrameMs = 16;

} // namespace

VisualizerController::VisualizerController(PlayerController *player, SettingsController *settings,
                                           QObject *parent)
    : QObject(parent)
    , m_player(player)
    , m_settings(settings)
    , m_bands(int(SpectrumAnalyzer::BandCount), 0.0)
{
    Q_ASSERT(!s_instance);
    s_instance = this;

    m_timer.setInterval(FrameMs);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &VisualizerController::computeBands);
    connect(m_player, &PlayerController::stateChanged, this, &VisualizerController::update);
    connect(m_settings, &SettingsController::visualizerEnabledChanged, this, &VisualizerController::update);
    update();
}

VisualizerController::~VisualizerController()
{
    m_player->spectrum().setEnabled(false);
    s_instance = nullptr;
}

VisualizerController *VisualizerController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

int VisualizerController::bandCount() const
{
    return int(SpectrumAnalyzer::BandCount);
}

void VisualizerController::setShown(bool shown)
{
    if (shown == m_shown)
        return;
    m_shown = shown;
    emit shownChanged();
    update();
}

void VisualizerController::update()
{
    const bool wanted = m_settings->visualizerEnabled() && m_shown;
    m_player->spectrum().setEnabled(wanted);
    // Not playing: keep running until the bars have fallen (computeBands stops it).
    if (wanted && (m_player->playing() || m_timer.isActive()))
        m_timer.start();
    else if (!wanted)
        m_timer.stop();
}

void VisualizerController::computeBands()
{
    std::array<float, SpectrumAnalyzer::BandCount> values{};
    const bool moving = m_player->spectrum().compute(values);
    for (size_t i = 0; i < values.size(); ++i)
        m_bands[qsizetype(i)] = values[i];
    emit bandsChanged();
    if (!moving && !m_player->playing())
        m_timer.stop();
}
