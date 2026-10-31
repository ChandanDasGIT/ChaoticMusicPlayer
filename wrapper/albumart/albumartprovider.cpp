#include "wrapper/albumart/albumartprovider.h"

#include "wrapper/albumart/covercontroller.h"

#include "backend/library/metadata/metadata.h"
#include "backend/network/httpclient/httpclient.h"

#include <QImage>
#include <QPainter>
#include <QUrl>

#include <cmath>

namespace {

// Corner radius as a fraction of the cover's side.
constexpr qreal CornerRadius = 0.04;

// Center-crops to a square, scales to fit `requested`, and rounds the corners
// (or cuts a circle), so QML can show it as a plain Image on any renderer.
QImage roundedSquare(const QImage &image, const QSize &requested, bool circle, bool sharp)
{
    const int side = qMin(image.width(), image.height());
    QImage square = image.copy((image.width() - side) / 2, (image.height() - side) / 2, side, side);
    if (requested.isValid() && (side > requested.width() || side > requested.height())) {
        const int target = qMin(requested.width(), requested.height());
        square = square.scaled(target, target, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    if (sharp)
        return square;

    QImage rounded(square.size(), QImage::Format_ARGB32_Premultiplied);
    rounded.fill(Qt::transparent);
    QPainter painter(&rounded);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QBrush(square));
    const qreal radius = square.width() * (circle ? 0.5 : CornerRadius);
    painter.drawRoundedRect(QRectF(rounded.rect()), radius, radius);
    return rounded;
}

// A cover filling a space of this width / height, its bottom dissolving along a
// gentle wave: truly transparent, so whatever is behind it flows up into it.
QImage riverCover(const QImage &square, qreal aspect)
{
    const int side = square.width();
    const int width = aspect < 1 ? qMax(1, int(side * aspect)) : side;
    const int height = aspect < 1 ? side : qMax(1, int(side / aspect));
    QImage image = square.copy((side - width) / 2, (side - height) / 2, width, height)
                       .convertToFormat(QImage::Format_ARGB32_Premultiplied);

    const qreal fade = height * 0.26;
    const qreal wave = height * 0.035;
    const int first = qMax(0, height - int(fade + 2 * wave) - 1);
    for (int y = first; y < height; ++y) {
        auto *line = reinterpret_cast<uchar *>(image.scanLine(y));
        for (int x = 0; x < width; ++x) {
            const qreal along = qreal(x) / width;
            const qreal edge = height - wave * (1 + 0.7 * std::sin(along * 9.0) + 0.3 * std::sin(along * 19.0 + 1.0));
            const qreal t = qBound(0.0, (edge - y) / fade, 1.0);
            const qreal alpha = t * t * (3 - 2 * t);
            for (int byte = 0; byte < 4; ++byte)
                line[x * 4 + byte] = uchar(line[x * 4 + byte] * alpha);
        }
    }
    return image;
}

} // namespace

AlbumArtProvider::AlbumArtProvider()
    : QQuickImageProvider(QQuickImageProvider::Image,
                          QQmlImageProviderBase::ForceAsynchronousImageLoading)
{
}

QImage AlbumArtProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    // "?r=<n>" after the encoded path only makes QML ask again once a cover was found;
    // "round=1" there asks for a circle (artists).
    const bool circle = id.section(QLatin1Char('?'), 1).contains(QLatin1String("round=1"));
    // "square=1": no rounded corners (a cover reaching the screen's edges).
    const QString query = id.section(QLatin1Char('?'), 1);
    const bool sharp = query.contains(QLatin1String("square=1"));
    // "river=<width / height in thousandths>": see riverCover().
    const int river = query.section(QLatin1String("river="), 1).section(QLatin1Char('&'), 0, 0).toInt();
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
        image = roundedSquare(image, requestedSize, circle, sharp);
        if (river > 0)
            image = riverCover(image, river / 1000.0);
    }

    if (size)
        *size = image.size();
    return image;
}
