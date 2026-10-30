// Windows: a toast (title, artist and album, with the cover), replacing the one
// before. Windows only shows toasts of apps with a registered app id, which the
// installer's Start menu shortcut provides; from the portable ZIP none show
// (the media controls' overlay still does).
#include "wrapper/notifications/notificationcontroller.h"

#include <QDir>
#include <QUrl>

#include <windows.h>
#include <winrt/Windows.Data.Xml.Dom.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Notifications.h>

using namespace winrt::Windows::UI::Notifications;
using winrt::Windows::Data::Xml::Dom::XmlDocument;

namespace {

winrt::hstring hs(const QString &text)
{
    return winrt::hstring(reinterpret_cast<const wchar_t *>(text.utf16()), uint32_t(text.size()));
}

QString escaped(QString text)
{
    return text.replace(QLatin1Char('&'), QLatin1String("&amp;"))
        .replace(QLatin1Char('<'), QLatin1String("&lt;"))
        .replace(QLatin1Char('>'), QLatin1String("&gt;"))
        .replace(QLatin1Char('"'), QLatin1String("&quot;"));
}

} // namespace

void NotificationController::send(const QString &title, const QString &subtitle, const QString &cover)
{
    QString xml = QStringLiteral("<toast><visual><binding template=\"ToastGeneric\">");
    if (!cover.isEmpty())
        xml += QStringLiteral("<image placement=\"appLogoOverride\" src=\"%1\"/>")
                   .arg(escaped(QUrl::fromLocalFile(cover).toString()));
    xml += QStringLiteral("<text>%1</text><text>%2</text></binding></visual><audio silent=\"true\"/></toast>")
               .arg(escaped(title), escaped(subtitle));
    try {
        XmlDocument document;
        document.LoadXml(hs(xml));
        ToastNotification toast(document);
        toast.Tag(L"song"); // each one replaces the one before
        toast.Group(L"song");
        toast.ExpirationTime(winrt::clock::now() + std::chrono::seconds(10));
        ToastNotificationManager::CreateToastNotifier(L"ChandanDas.ChaoticMusicPlayer").Show(toast);
    } catch (...) {
        // No registered app id (portable ZIP) or notifications off: nothing to show
    }
}
