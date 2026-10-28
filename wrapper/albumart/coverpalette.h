#pragma once

#include <QColor>
#include <QObject>
#include <QThreadPool>
#include <QtQml/qqmlregistration.h>

class PlayerController;
class PlaylistController;
class QJSEngine;
class QQmlEngine;

// Background color taken from the playing track's cover (`CoverPalette`
// singleton). Computed on a worker thread whenever the track changes.
class CoverPalette : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QColor color READ color NOTIFY colorChanged)
    Q_PROPERTY(bool hasColor READ hasColor NOTIFY colorChanged)

public:
    CoverPalette(PlayerController *player, PlaylistController *playlist, QObject *parent);
    ~CoverPalette() override;

    static CoverPalette *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    // Dark, readable background tone with the cover's dominant hue.
    QColor color() const { return m_color; }
    bool hasColor() const { return m_color.isValid(); }

    static QColor backgroundFromImage(const QImage &image);

    // Read the playing song's cover again (e.g. one was just found online).
    void refresh();

signals:
    void colorChanged();

private:
    void update();
    void apply(int generation, const QColor &color);

    static inline CoverPalette *s_instance = nullptr;

    PlayerController *m_player;
    PlaylistController *m_playlist;
    QString m_source;
    QColor m_color;
    int m_generation = 0; // ignores results for tracks that are no longer playing
    QThreadPool m_pool;
};
