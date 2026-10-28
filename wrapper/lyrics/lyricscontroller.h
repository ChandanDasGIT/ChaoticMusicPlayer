#pragma once

#include <QObject>
#include <QString>
#include <QThreadPool>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <memory>
#include <vector>

class LyricsProvider;
class PlayerController;
class PlaylistController;
class QJSEngine;
class QQmlEngine;
class SettingsController;

// Lyrics of the playing track (`LyricsController` singleton). Fetched on a
// worker thread whenever the track changes, from the sources (and in the
// order) chosen in Settings; follows the playback position for synced lyrics.
class LyricsController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(Status status READ status NOTIFY lyricsChanged)
    Q_PROPERTY(QVariantList lines READ lines NOTIFY lyricsChanged) // [{ time, text }]
    Q_PROPERTY(bool synced READ synced NOTIFY lyricsChanged)
    Q_PROPERTY(QString sourceName READ sourceName NOTIFY lyricsChanged)
    Q_PROPERTY(int currentLine READ currentLine NOTIFY currentLineChanged)

public:
    enum Status { NoTrack, Loading, Found, NotFound };
    Q_ENUM(Status)

    LyricsController(PlayerController *player, PlaylistController *playlist,
                     SettingsController *settings, QObject *parent);
    ~LyricsController() override;

    static LyricsController *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    Status status() const { return m_status; }
    QVariantList lines() const { return m_lines; }
    bool synced() const { return m_synced; }
    QString sourceName() const { return m_sourceName; }
    int currentLine() const { return m_currentLine; }

    Q_INVOKABLE void reload();

signals:
    void lyricsChanged();
    void currentLineChanged();

private:
    void onSourceChanged();
    void updateCurrentLine();

    static inline LyricsController *s_instance = nullptr;

    PlayerController *m_player;
    PlaylistController *m_playlist;
    SettingsController *m_settings;
    std::shared_ptr<LyricsProvider> m_provider;

    QString m_path;
    int m_generation = 0;
    Status m_status = NoTrack;
    QVariantList m_lines;
    std::vector<qint64> m_times; // line start times, for the position lookup
    bool m_synced = false;
    QString m_sourceName;
    int m_currentLine = -1;
    QThreadPool m_pool;
};
