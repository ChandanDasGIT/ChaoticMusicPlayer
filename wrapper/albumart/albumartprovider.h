#pragma once

#include <QQuickImageProvider>

// Serves cover art to QML as image://albumart/<percent-encoded file path or
// https:// thumbnail URL (YouTube tracks)>.
// Covers come back square with rounded corners. Images load on a worker
// thread; a 1x1 transparent image means "no cover".
class AlbumArtProvider : public QQuickImageProvider
{
public:
    AlbumArtProvider();

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
};
