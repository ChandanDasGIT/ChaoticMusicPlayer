#pragma once

#include <QObject>
#include <QString>
#include <QThreadPool>
#include <QtQml/qqmlregistration.h>

#include <string>
#include <vector>

class PlayerController;
class QJSEngine;
class QQmlEngine;
class SettingsController;
class YtMetadataController;

// Album art for local songs without an embedded cover (`CoverController`
// singleton). When such a song plays, the sources chosen in Settings are asked
// in the background (once per album; the largest image wins) and the result is
// cached. `revision` changes whenever a new cover was found, so images reload
// (Theme.coverSource adds it to the address).
class CoverController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)
    // Where the playing song's cover comes from: "embedded", "folder", "itunes",
    // "musicbrainz", "ytmusic", or "" (none)
    Q_PROPERTY(QString currentSource READ currentSource NOTIFY currentChanged)
    Q_PROPERTY(bool searching READ searching NOTIFY currentChanged)

public:
    CoverController(PlayerController *player, YtMetadataController *ytMetadata, SettingsController *settings,
                    QObject *parent);
    ~CoverController() override;

    static CoverController *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    int revision() const { return m_revision; }
    QString currentSource() const { return m_currentSource; }
    bool searching() const { return m_searching; }

    // Best known cover of a local file: embedded, else found / folder image. Any thread.
    static std::vector<unsigned char> coverBytes(const std::string &path);

signals:
    void revisionChanged();
    void currentChanged();

private:
    void onSourceChanged();
    void improve(int generation, const QString &source); // a (better) cover was found and cached
    void finish(int generation, const QString &source, bool found);

    static inline CoverController *s_instance = nullptr;

    PlayerController *const m_player;
    YtMetadataController *const m_ytMetadata;
    SettingsController *const m_settings;
    QThreadPool m_pool;
    int m_generation = 0;
    int m_revision = 0;
    QString m_path;
    QString m_currentSource;
    bool m_searching = false;
};
