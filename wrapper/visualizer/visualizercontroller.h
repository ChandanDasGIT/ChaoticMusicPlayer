#pragma once

#include <QList>
#include <QObject>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

class PlayerController;
class QJSEngine;
class QQmlEngine;
class SettingsController;

// Frequency bands of what is playing (`VisualizerController` singleton), for
// the visualizer under the volume bar. Updates at about 60 fps only while the
// visualizer is switched on in Settings, shown, and something is playing;
// after a pause the bands fall to 0 and updates stop.
class VisualizerController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QList<qreal> bands READ bands NOTIFY bandsChanged) // each 0..1, low to high
    Q_PROPERTY(int bandCount READ bandCount CONSTANT)
    // Set by the Visualizer item while it is on screen.
    Q_PROPERTY(bool shown READ shown WRITE setShown NOTIFY shownChanged)

public:
    VisualizerController(PlayerController *player, SettingsController *settings, QObject *parent);
    ~VisualizerController() override;

    static VisualizerController *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    QList<qreal> bands() const { return m_bands; }
    int bandCount() const;
    bool shown() const { return m_shown; }
    void setShown(bool shown);

signals:
    void bandsChanged();
    void shownChanged();

private:
    void update();       // start / stop following the audio
    void computeBands(); // one frame

    static inline VisualizerController *s_instance = nullptr;

    PlayerController *const m_player;
    SettingsController *const m_settings;
    QTimer m_timer;
    QList<qreal> m_bands;
    bool m_shown = false;
};
