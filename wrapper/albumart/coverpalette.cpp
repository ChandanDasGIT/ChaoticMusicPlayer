#include "wrapper/albumart/coverpalette.h"

#include "wrapper/albumart/covercontroller.h"

#include "backend/library/metadata/metadata.h"
#include "backend/network/httpclient/httpclient.h"
#include "wrapper/player/playercontroller.h"
#include "wrapper/playlist/playlistcontroller.h"

#include <QImage>
#include <QJSEngine>

CoverPalette::CoverPalette(PlayerController *player, PlaylistController *playlist, QObject *parent)
    : QObject(parent)
    , m_player(player)
    , m_playlist(playlist)
{
    Q_ASSERT(!s_instance);
    s_instance = this;

    m_pool.setMaxThreadCount(1);
    connect(m_player, &PlayerController::sourceChanged, this, &CoverPalette::update);
    update();
}

CoverPalette::~CoverPalette()
{
    m_pool.waitForDone(); // no task may post to a destroyed object
    s_instance = nullptr;
}

CoverPalette *CoverPalette::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

// Average of a downscaled cover, weighting colorful pixels more (so a small
// bright logo beats a large grey area), then turned into a dark background
// tone that keeps the hue. Grey covers stay grey.
QColor CoverPalette::backgroundFromImage(const QImage &image)
{
    const QImage small = image.scaled(24, 24, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                             .convertToFormat(QImage::Format_RGB32);

    double r = 0, g = 0, b = 0, total = 0;
    for (int y = 0; y < small.height(); ++y) {
        for (int x = 0; x < small.width(); ++x) {
            const QColor c = small.pixelColor(x, y);
            const double saturation = c.hsvSaturationF();
            const double value = c.valueF();
            const double weight = 0.1 + saturation * saturation * (value > 0.15 ? 1.0 : 0.2);
            r += c.redF() * weight;
            g += c.greenF() * weight;
            b += c.blueF() * weight;
            total += weight;
        }
    }
    if (total <= 0)
        return {};

    float hue, saturation, lightness;
    QColor::fromRgbF(float(r / total), float(g / total), float(b / total)).getHslF(&hue, &saturation, &lightness);
    if (saturation > 0.08f)
        saturation = qBound(0.25f, saturation * 1.15f, 0.6f);
    return QColor::fromHslF(qMax(hue, 0.0f), saturation, 0.2f);
}

void CoverPalette::update()
{
    const QString source = m_player->source();
    if (source == m_source)
        return;
    m_source = source;

    const int generation = ++m_generation;
    if (source.isEmpty()) {
        apply(generation, {});
        return;
    }

    // YouTube tracks have a thumbnail URL instead of a file with a cover.
    const ::Track *track = m_playlist->trackForPath(source);
    const QString thumbnail = track ? QString::fromStdString(track->thumbnailUrl) : QString();

    m_pool.start([this, source, thumbnail, generation] {
        QImage image;
        if (!thumbnail.isEmpty()) {
            const HttpResponse response = HttpClient::get(thumbnail.toStdString(), {}, 15000);
            if (response.ok())
                image.loadFromData(reinterpret_cast<const uchar *>(response.body.data()), qsizetype(response.body.size()));
        } else {
            const std::vector<unsigned char> data = CoverController::coverBytes(source.toStdString());
            if (!data.empty())
                image.loadFromData(data.data(), qsizetype(data.size()));
        }
        const QColor color = image.isNull() ? QColor() : backgroundFromImage(image);
        QMetaObject::invokeMethod(this, [this, generation, color] { apply(generation, color); },
                                  Qt::QueuedConnection);
    });
}

void CoverPalette::refresh()
{
    m_source.clear();
    update();
}

void CoverPalette::apply(int generation, const QColor &color)
{
    if (generation != m_generation || color == m_color)
        return;
    m_color = color;
    emit colorChanged();
}
