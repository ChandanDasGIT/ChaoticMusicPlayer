#include "wrapper/notifications/notificationcontroller.h"

#include "backend/library/metadata/metadata.h"
#include "backend/youtube/streamresolver/streamresolver.h"
#include "wrapper/player/playercontroller.h"
#include "wrapper/playlist/playlistcontroller.h"
#include "wrapper/settings/settingscontroller.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QUrl>

namespace {

constexpr int ShownMs = 5000;

// The notification body may hold simple markup: show text as text.
QString plain(QString text)
{
    return text.replace(QLatin1Char('&'), QLatin1String("&amp;"))
        .replace(QLatin1Char('<'), QLatin1String("&lt;"))
        .replace(QLatin1Char('>'), QLatin1String("&gt;"));
}

} // namespace

NotificationController::NotificationController(PlayerController *player, PlaylistController *playlist,
                                               SettingsController *settings, QObject *parent)
    : QObject(parent)
    , m_player(player)
    , m_playlist(playlist)
    , m_settings(settings)
{
    connect(m_player, &PlayerController::sourceChanged, this, [this] { m_pending = m_player->hasTrack(); });
    connect(m_player, &PlayerController::positionChanged, this, &NotificationController::onPosition);
}

// Told once the new song is actually playing (not when it is only loaded, e.g. resumed paused).
void NotificationController::onPosition()
{
    if (!m_pending || !m_player->playing())
        return;
    m_pending = false;
    if (m_settings->notificationsEnabled() && QGuiApplication::applicationState() != Qt::ApplicationActive)
        notify();
}

void NotificationController::notify()
{
    const QString title = !m_playlist->currentTitle().isEmpty() ? m_playlist->currentTitle() : m_player->title();
    const QString subtitle = m_playlist->currentSubtitle();

    QVariantMap hints{{QStringLiteral("desktop-entry"), QStringLiteral("ChaoticMusicPlayer")},
                      {QStringLiteral("category"), QStringLiteral("x-gnome.music")},
                      {QStringLiteral("urgency"), QVariant::fromValue(uchar(0))},
                      {QStringLiteral("transient"), true}};
    const QString cover = coverFile(m_player->source());
    if (!cover.isEmpty())
        hints.insert(QStringLiteral("image-path"), QUrl::fromLocalFile(cover).toString());

    QDBusMessage call = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.Notifications"),
                                                       QStringLiteral("/org/freedesktop/Notifications"),
                                                       QStringLiteral("org.freedesktop.Notifications"),
                                                       QStringLiteral("Notify"));
    call << QStringLiteral("ChaoticMusicPlayer") << m_lastId << QStringLiteral("chaoticmusicplayer") << title
         << plain(subtitle) << QStringList() << hints << ShownMs;
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        const QDBusPendingReply<uint> reply = *w;
        if (reply.isValid())
            m_lastId = reply.value();
        w->deleteLater();
    });
}

QString NotificationController::coverFile(const QString &path) const
{
    if (path.isEmpty() || StreamResolver::isYouTube(path.toStdString()))
        return {};
    const std::vector<unsigned char> bytes = Metadata::readCover(path.toStdString());
    if (bytes.empty())
        return {};
    const bool png = bytes.size() > 4 && bytes[0] == 0x89 && bytes[1] == 'P';
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir().mkpath(dir);
    const QString file = dir + (png ? QStringLiteral("/notification-cover.png") : QStringLiteral("/notification-cover.jpg"));
    QFile out(file);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return {};
    out.write(reinterpret_cast<const char *>(bytes.data()), qint64(bytes.size()));
    return file;
}
