// Android: the media notification (MediaPlaybackService) already shows each new
// song, so no separate notification is sent.
#include "wrapper/notifications/notificationcontroller.h"

void NotificationController::send(const QString &, const QString &, const QString &)
{
}
