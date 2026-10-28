#pragma once

#include <QList>
#include <QObject>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <memory>
#include <string>
#include <vector>

class PlaylistController;
class QJSEngine;
class QQmlEngine;
class Scanner;

// Turns dropped folders/files into playlist entries (`LibraryController` singleton).
class LibraryController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(int scannedCount READ scannedCount NOTIFY progressChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY progressChanged)

public:
    LibraryController(PlaylistController *playlist, QObject *parent);
    ~LibraryController() override;

    static LibraryController *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    bool scanning() const { return m_pendingScans > 0; }
    int scannedCount() const { return m_scannedCount; }
    int totalCount() const { return m_totalCount; }

    // Local file:// URLs of folders or audio files; anything else is ignored.
    Q_INVOKABLE void addUrls(const QList<QUrl> &urls);

    // Details of one file for the details window, as a list of sections:
    // [{ title, collapsible, fields: [{ label, value }] }]
    Q_INVOKABLE QVariantList trackDetails(const QString &path) const;

signals:
    void scanningChanged();
    void progressChanged();
    void scanFinished(int added);

private:
    void onProgress(int done, int total);
    void onFinished();

    static inline LibraryController *s_instance = nullptr;

    PlaylistController *m_playlist;
    int m_pendingScans = 0;
    int m_scannedCount = 0;
    int m_totalCount = 0;
    int m_addedThisScan = 0;
    std::vector<std::string> m_addedPaths; // tracks added by the current drop(s)

    std::unique_ptr<Scanner> m_scanner;
};
