#include "wrapper/albumart/albumartprovider.h"

#include "wrapper/albumart/covercontroller.h"

#include "backend/library/metadata/metadata.h"
#include "backend/network/httpclient/httpclient.h"

#include <QImage>
#include <QPainter>
#include <QUrl>

namespace {

// Corner radius as a fraction of the cover's side.
constexpr qreal CornerRadius = 0.04;

// Center-crops to a square, scales to fit `requested`, and rounds the corners,
// so QML can show it as a plain Image on any renderer.
QImage roundedSquare(const QImage &image, const QSize &requested)
{
    const int side = qMin(image.width(), image.height());
    QImage square = image.copy((image.width() - side) / 2, (image.height() - side) / 2, side, side);
    if (requested.isValid() && (side > requested.width() || side > requested.height())) {
        const int target = qMin(requested.width(), requested.height());
        square = square.scaled(target, target, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    QImage rounded(square.size(), QImage::Format_ARGB32_Premultiplied);
    rounded.fill(Qt::transparent);
    QPainter painter(&rounded);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QBrush(square));
    const qreal radius = square.width() * CornerRadius;
    painter.drawRoundedRect(QRectF(rounded.rect()), radius, radius);
    return rounded;
}

} // namespace

AlbumArtProvider::AlbumArtProvider()
    : QQuickImageProvider(QQuickImageProvider::Image,
                          QQmlImageProviderBase::ForceAsynchronousImageLoading)
{
}

QImage AlbumArtProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    // "?r=<n>" after the encoded path only makes QML ask again once a cover was found.
    const QString path = QUrl::fromPercentEncoding(id.section(QLatin1Char('?'), 0, 0).toUtf8());

    QImage image;
    if (path.startsWith(QLatin1String("https://"))) {
        // YouTube thumbnail
        const HttpResponse response = HttpClient::get(path.toStdString(), {}, 15000);
        if (response.ok())
            image.loadFromData(reinterpret_cast<const uchar *>(response.body.data()), qsizetype(response.body.size()));
    } else {
        const std::vector<unsigned char> data = CoverController::coverBytes(path.toStdString()); // embedded or found
        if (!data.empty())
            image.loadFromData(data.data(), qsizetype(data.size()));
    }
    if (image.isNull()) {
        // "No cover": a 1x1 transparent image (a null image would make Qt log a warning).
        image = QImage(1, 1, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
    } else {
        image = roundedSquare(image, requestedSize);
    }

    if (size)
        *size = image.size();
    return image;
}
