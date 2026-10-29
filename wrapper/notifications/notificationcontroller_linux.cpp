// Linux: notifications through the desktop's notification service (D-Bus,
// org.freedesktop.Notifications).
#include "wrapper/notifications/notificationcontroller.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
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

void NotificationController::send(const QString &title, const QString &subtitle, const QString &cover)
{
    QVariantMap hints{{QStringLiteral("desktop-entry"), QStringLiteral("ChaoticMusicPlayer")},
                      {QStringLiteral("category"), QStringLiteral("x-gnome.music")},
                      {QStringLiteral("urgency"), QVariant::fromValue(uchar(0))},
                      {QStringLiteral("transient"), true}};
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
