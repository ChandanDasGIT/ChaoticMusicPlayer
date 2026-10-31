// Android: the media notification (lock screen, headset buttons) through
// MediaPlaybackService (android/src/...), which also keeps the app playing in
// the background. Asks for the permissions the app needs at start.
#include "wrapper/systemmedia/systemmedia.h"

#include "backend/library/track/track.h"
#include "backend/network/httpclient/httpclient.h"
#include "wrapper/albumart/covercontroller.h"
#include "wrapper/files/files.h"
#include "wrapper/player/playercontroller.h"
#include "wrapper/playlist/playlistcontroller.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJniEnvironment>
#include <QJniObject>
#include <QPointer>
#include <QThreadPool>
#include <QTimer>

namespace {

constexpr const char *ServiceClass = "io/github/chandandasgit/tunedchaos/MediaPlaybackService";

QPointer<SystemMedia> g_instance; // the one the buttons reach
PlayerController *g_player = nullptr;
PlaylistController *g_playlist = nullptr;

// A button of the notification / lock screen / headset (Java's thread)
void nativeAction(JNIEnv *, jclass, jint action)
{
    QMetaObject::invokeMethod(qApp, [action] {
        if (!g_instance)
            return;
        switch (action) {
        case 0: // play
            if (g_player->hasTrack())
                g_player->play();
            else
                g_playlist->playPause();
            break;
        case 1:
            g_player->pause();
            break;
        case 2:
            g_playlist->next();
            break;
        case 3:
            g_playlist->previous();
            break;
        case 4:
            g_player->stop();
            break;
        }
    }, Qt::QueuedConnection);
}

// The notification's seek bar was dragged (Java's thread)
void nativeSeek(JNIEnv *, jclass, jlong positionMs)
{
    QMetaObject::invokeMethod(qApp, [positionMs] {
        if (g_instance)
            g_player->seek(positionMs);
    }, Qt::QueuedConnection);
}

QJniObject context()
{
    return QNativeInterface::QAndroidApplication::context();
}

} // namespace

struct SystemMedia::Impl
{
    QTimer updateTimer;
    QString coverSource; // the song whose cover was saved
    QString coverFile;   // "" while there is none
};

SystemMedia::SystemMedia(PlayerController *player, PlaylistController *playlist, QObject *parent)
    : QObject(parent)
    , m_impl(std::make_unique<Impl>())
{
    g_instance = this;
    g_player = player;
    g_playlist = playlist;

    static const JNINativeMethod methods[] = {{"nativeAction", "(I)V", reinterpret_cast<void *>(nativeAction)},
                                              {"nativeSeek", "(J)V", reinterpret_cast<void *>(nativeSeek)}};
    QJniEnvironment env;
    env.registerNativeMethods(ServiceClass, methods, 2);

    // Reading music and showing the media notification need the user's yes
    QJniObject::callStaticMethod<void>(ServiceClass, "requestPermissions", "(Landroid/content/Context;)V",
                                       context().object());

    m_impl->updateTimer.setSingleShot(true);
    m_impl->updateTimer.setInterval(100);
    connect(&m_impl->updateTimer, &QTimer::timeout, this, [this] {
        const QString source = g_player->source();
        const Track *track = g_playlist->currentTrack();

        // The cover, saved as a file Android can read (in the background)
        if (source != m_impl->coverSource) {
            m_impl->coverSource = source;
            m_impl->coverFile.clear();
            const QString thumbnail = track ? QString::fromStdString(track->thumbnailUrl) : QString();
            QPointer<SystemMedia> self(this);
            QThreadPool::globalInstance()->start([self, source, thumbnail] {
                std::vector<unsigned char> data;
                if (!thumbnail.isEmpty()) {
                    const HttpResponse response = HttpClient::get(thumbnail.toStdString(), {}, 10000);
                    if (response.ok())
                        data.assign(response.body.begin(), response.body.end());
                } else if (!source.isEmpty()) {
                    data = CoverController::coverBytes(source.toStdString());
                }
                QString file;
                if (!data.empty()) {
                    QDir().mkpath(Files::cacheDir());
                    file = Files::cacheDir() + QStringLiteral("/media-notification-cover");
                    QFile out(file);
                    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)
                        || out.write(reinterpret_cast<const char *>(data.data()), qint64(data.size())) <= 0)
                        file.clear();
                }
                QMetaObject::invokeMethod(qApp, [self, source, file] {
                    if (!self || self->m_impl->coverSource != source)
                        return;
                    self->m_impl->coverFile = file;
                    self->m_impl->updateTimer.start();
                }, Qt::QueuedConnection);
            });
        }

        QJniObject::callStaticMethod<void>(
            ServiceClass, "update",
            "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;ZZJJ)V",
            context().object(),
            QJniObject::fromString(track ? QString::fromStdString(track->title) : g_player->title()).object<jstring>(),
            QJniObject::fromString(track ? QString::fromStdString(track->artist) : QString()).object<jstring>(),
            QJniObject::fromString(track ? QString::fromStdString(track->album) : QString()).object<jstring>(),
            QJniObject::fromString(m_impl->coverFile).object<jstring>(),
            jboolean(g_player->playing()), jboolean(g_player->hasTrack()),
            jlong(g_player->position()), jlong(g_player->duration()));
    });
    auto schedule = [this] { m_impl->updateTimer.start(); };
    connect(player, &PlayerController::stateChanged, this, schedule);
    connect(player, &PlayerController::sourceChanged, this, schedule);
    connect(player, &PlayerController::durationChanged, this, schedule);
    connect(player, &PlayerController::seeked, this, schedule);
    connect(playlist, &PlaylistController::currentChanged, this, schedule);
}

SystemMedia::~SystemMedia()
{
    // Ends the notification (and the service)
    QJniObject::callStaticMethod<void>(
        ServiceClass, "update",
        "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;ZZJJ)V",
        context().object(), QJniObject::fromString(QString()).object<jstring>(),
        QJniObject::fromString(QString()).object<jstring>(), QJniObject::fromString(QString()).object<jstring>(),
        QJniObject::fromString(QString()).object<jstring>(), jboolean(false), jboolean(false), jlong(0), jlong(0));
}
