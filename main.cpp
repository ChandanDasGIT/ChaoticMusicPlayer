#include "wrapper/albumart/albumartprovider.h"
#include "wrapper/albumart/covercontroller.h"
#include "wrapper/albumart/coverpalette.h"
#include "wrapper/library/librarycontroller.h"
#include "wrapper/locallibrary/locallibrarycontroller.h"
#include "wrapper/lyrics/lyricscontroller.h"
#include "wrapper/mpris/mprisadaptor.h"
#include "wrapper/player/playercontroller.h"
#include "wrapper/playlist/playlistcontroller.h"
#include "wrapper/playlists/playlistscontroller.h"
#include "wrapper/settings/settingscontroller.h"
#include "wrapper/visualizer/visualizercontroller.h"
#include "wrapper/singleinstance/singleinstance.h"
#include "wrapper/youtube/ytmetadatacontroller.h"
#include "wrapper/youtube/youtubecontroller.h"

#include <QFontDatabase>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QWindow>
#include <QtQml/qqmlextensionplugin.h>

// The app's controls style (frontend/style), linked in statically.
Q_IMPORT_QML_PLUGIN(ChaoticStylePlugin)

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("ChaoticMusicPlayer");
    QGuiApplication::setApplicationVersion(QStringLiteral(APP_VERSION)); // from CMake (project version)
    QGuiApplication::setOrganizationName("ChaoticMusicPlayer");
    QGuiApplication::setDesktopFileName("ChaoticMusicPlayer"); // app id; matches MPRIS DesktopEntry
    // Window / task bar icon (the launcher uses the installed copy, see scripts/install-desktop.sh).
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/qt/qml/ChaoticMusicPlayer/resources/icons/chaoticmusicplayer.svg")));

    // Only one copy per user: a second launch brings the first one forward.
    SingleInstance instance(nullptr);
    if (!instance.start())
        return 0;

    // Our own style: Basic (fully customizable from QML) with a nicer ToolTip.
    QQuickStyle::setStyle("ChaoticStyle");

    // Icon font (Font Awesome 6 Free Solid), used by frontend/Icon.qml.
    if (QFontDatabase::addApplicationFont(
            ":/qt/qml/ChaoticMusicPlayer/frontend/fonts/fa-solid-900.ttf") < 0)
        qWarning("Failed to load icon font fa-solid-900.ttf");

    // Controllers must exist before QML asks for the singletons.
    SettingsController settings(nullptr);
    PlayerController player(nullptr);
    PlaylistController playlist(&player, &settings, nullptr);
    LibraryController library(&playlist, nullptr);
    CoverPalette coverPalette(&player, &playlist, nullptr);
    LyricsController lyrics(&player, &playlist, &settings, nullptr);
    YtMetadataController ytMetadata(&playlist, &player, &settings, nullptr);
    CoverController covers(&player, &ytMetadata, &settings, nullptr);
    QObject::connect(&covers, &CoverController::revisionChanged, &coverPalette, &CoverPalette::refresh);
    YouTubeController youtube(&playlist, &settings, nullptr);
    PlaylistsController playlists(&playlist, &player, &settings, nullptr);
    LocalLibraryController localLibrary(&playlist, &playlists, &ytMetadata, &settings, nullptr);
    VisualizerController visualizer(&player, &settings, nullptr);
    // Crossfade on: switching tracks fades too, instead of cutting.
    player.setSkipFade(settings.crossfadeSeconds() > 0);
    QObject::connect(&settings, &SettingsController::crossfadeSecondsChanged, &player,
                     [&] { player.setSkipFade(settings.crossfadeSeconds() > 0); });

    // YouTube sign-in: yt-dlp reads this browser's session.
    player.setYtCookiesBrowser(settings.ytCookiesBrowser());
    QObject::connect(&settings, &SettingsController::ytCookiesBrowserChanged,
                     &player, [&] { player.setYtCookiesBrowser(settings.ytCookiesBrowser()); });

    // Remember the volume and the output device between sessions.
    player.setOutputDevice(settings.outputDevice());
    QObject::connect(&player, &PlayerController::outputDeviceChanged,
                     &settings, [&] { settings.setOutputDevice(player.outputDevice()); });
    player.setVolume(settings.volume());
    QObject::connect(&player, &PlayerController::volumeChanged,
                     &settings, [&] { settings.setVolume(player.volume()); });

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("albumart"), new AlbumArtProvider); // engine takes ownership
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, [] { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("ChaoticMusicPlayer", "Main");

    const auto raiseWindow = [&engine] {
        if (engine.rootObjects().isEmpty())
            return;
        if (auto *window = qobject_cast<QWindow *>(engine.rootObjects().constFirst())) {
            window->show();
            window->raise();
            window->requestActivate();
        }
    };
    QObject::connect(&instance, &SingleInstance::activationRequested, &app, raiseWindow);

    // Desktop media controls / media keys / playerctl.
    MprisAdaptor mpris(&player, &playlist, nullptr);
    QObject::connect(&mpris, &MprisAdaptor::raiseRequested, &app, raiseWindow);
    // Quit for real: with background playing on, closing the window would only hide it.
    QObject::connect(&mpris, &MprisAdaptor::quitRequested, &app, [&engine] {
        if (!engine.rootObjects().isEmpty())
            QMetaObject::invokeMethod(engine.rootObjects().constFirst(), "quitApp");
        else
            QCoreApplication::quit();
    });

    return app.exec();
}
