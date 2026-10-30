#include "wrapper/albumart/albumartprovider.h"
#include "wrapper/albumart/covercontroller.h"
#include "wrapper/albumart/coverpalette.h"
#include "wrapper/history/historycontroller.h"
#include "wrapper/library/librarycontroller.h"
#include "wrapper/locallibrary/locallibrarycontroller.h"
#include "wrapper/lyrics/lyricscontroller.h"
#include "wrapper/player/playercontroller.h"
#include "wrapper/notifications/notificationcontroller.h"
#include "wrapper/playlist/playlistcontroller.h"
#include "wrapper/playlists/playlistscontroller.h"
#include "wrapper/session/sessioncontroller.h"
#include "wrapper/settings/settingscontroller.h"
#include "wrapper/sleeptimer/sleeptimercontroller.h"
#include "wrapper/systemmedia/systemmedia.h"
#include "wrapper/smartplaylists/smartplaylistscontroller.h"
#include "wrapper/visualizer/visualizercontroller.h"
#include "wrapper/singleinstance/singleinstance.h"
#include "wrapper/youtube/ytmetadatacontroller.h"
#include "wrapper/youtube/youtubecontroller.h"

#include <QFontDatabase>
#include <QGuiApplication>
#include <QIcon>
#include <QNetworkInformation>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QWindow>
#include <QtQml/qqmlextensionplugin.h>

#ifdef Q_OS_ANDROID
#include "backend/network/httpclient/httpclient.h"
#include "wrapper/files/files.h"

#include <QDir>
#include <QFile>
#endif

#ifdef Q_OS_WIN
#include <shobjidl_core.h> // SetCurrentProcessExplicitAppUserModelID
#endif

// The app's controls style (frontend/style), linked in statically.
Q_IMPORT_QML_PLUGIN(ChaoticStylePlugin)

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    // Windows' app id: groups the taskbar button, and lets toasts show (the
    // installer's Start menu shortcut carries the same one)
    SetCurrentProcessExplicitAppUserModelID(L"ChandanDas.ChaoticMusicPlayer");
#endif

    // Interface size (Settings > Appearance): Qt scales the whole UI by it. It
    // must be set before the app starts, so a change applies after a restart.
    // A QT_SCALE_FACTOR set by the user wins.
    const bool scaleFromSettings = !qEnvironmentVariableIsSet("QT_SCALE_FACTOR");
    const qreal interfaceScale = scaleFromSettings ? SettingsController::storedInterfaceScale() : 1.0;
    if (scaleFromSettings && !qFuzzyCompare(interfaceScale, 1.0))
        qputenv("QT_SCALE_FACTOR", QByteArray::number(interfaceScale));

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("ChaoticMusicPlayer");
    QGuiApplication::setApplicationVersion(QStringLiteral(APP_VERSION)); // from CMake (project version)
    QGuiApplication::setOrganizationName("ChaoticMusicPlayer");
    // The app id: its desktop file's name (release/linux), for the window, media controls and notifications
    QGuiApplication::setDesktopFileName(QStringLiteral("io.github.chandandasgit.ChaoticMusicPlayer"));
    // Window / task bar icon (the launcher uses the installed copy, see scripts/install-desktop.sh).
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/qt/qml/ChaoticMusicPlayer/resources/icons/chaoticmusicplayer.svg")));

#ifdef Q_OS_ANDROID
    // Streams (FFmpeg's OpenSSL) check the server's certificate, but OpenSSL
    // finds none on Android: the phone's, saved as a file the decoder hands
    // FFmpeg (backend/audioengine/decoder: SSL_CERT_FILE)
    {
        const std::string &certificates = HttpClient::androidCertificates();
        QDir().mkpath(Files::cacheDir());
        QFile file(Files::cacheDir() + QStringLiteral("/certificates.pem"));
        if (!certificates.empty() && file.open(QIODevice::WriteOnly | QIODevice::Truncate)
            && file.write(certificates.data(), qint64(certificates.size())) == qint64(certificates.size())) {
            file.close();
            qputenv("SSL_CERT_FILE", QFile::encodeName(file.fileName()));
        }
    }
#endif

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
    settings.setActiveInterfaceScale(interfaceScale);
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
    // Reopen the last song, paused where it was (Settings > General).
    HistoryController history(&player, &playlist, &playlists, &localLibrary, &settings, nullptr);
    SmartPlaylistsController smartPlaylists(&history, &localLibrary, &playlist, nullptr);
    SleepTimerController sleepTimer(&player, nullptr);
    NotificationController notifications(&player, &playlist, &settings, nullptr);
    SessionController session(&player, &playlist, &playlists, &settings, nullptr);
    session.restore();
    VisualizerController visualizer(&player, &settings, nullptr);
    // Fades on: switching tracks fades too, instead of cutting.
    player.setSkipFade(settings.fadeSeconds() > 0);
    QObject::connect(&settings, &SettingsController::fadeSecondsChanged, &player,
                     [&] { player.setSkipFade(settings.fadeSeconds() > 0); });

    // YouTube sign-in: yt-dlp reads this browser's session.
    player.setYtCookiesBrowser(settings.ytCookiesBrowser());
    QObject::connect(&settings, &SettingsController::ytCookiesBrowserChanged,
                     &player, [&] { player.setYtCookiesBrowser(settings.ytCookiesBrowser()); });

    // YouTube audio quality. Auto takes the smallest streams on a metered
    // connection (e.g. a phone hotspot), as NetworkManager reports it.
    QNetworkInformation::loadBackendByFeatures(QNetworkInformation::Feature::Metered);
    const auto applyYtQuality = [&] {
        const QNetworkInformation *network = QNetworkInformation::instance();
        const bool metered = network && network->isMetered();
        player.setYtLowestQuality(settings.ytQuality() == SettingsController::YtLowest
                                  || (settings.ytQuality() == SettingsController::YtAuto && metered));
    };
    applyYtQuality();
    QObject::connect(&settings, &SettingsController::ytQualityChanged, &player, applyYtQuality);
    if (const QNetworkInformation *network = QNetworkInformation::instance())
        QObject::connect(network, &QNetworkInformation::isMeteredChanged, &player, applyYtQuality);

    // ReplayGain (from the next bit of audio decoded)
    player.setReplayGainMode(settings.replayGain());
    QObject::connect(&settings, &SettingsController::replayGainChanged, &player,
                     [&] { player.setReplayGainMode(settings.replayGain()); });

    // The equalizer (changes are heard at once, also while a slider is dragged).
    const auto applyEqualizer = [&] { player.setEqualizer(settings.eqGains(), settings.eqEnabled()); };
    applyEqualizer();
    QObject::connect(&settings, &SettingsController::eqChanged, &player, applyEqualizer);

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

    // The system's media controls / media keys (Linux: also playerctl).
    SystemMedia systemMedia(&player, &playlist, nullptr);
    QObject::connect(&systemMedia, &SystemMedia::raiseRequested, &app, raiseWindow);
    // Quit for real: with background playing on, closing the window would only hide it.
    QObject::connect(&systemMedia, &SystemMedia::quitRequested, &app, [&engine] {
        if (!engine.rootObjects().isEmpty())
            QMetaObject::invokeMethod(engine.rootObjects().constFirst(), "quitApp");
        else
            QCoreApplication::quit();
    });

    // Restart (Settings > Appearance > Interface size): quit as usual, then start again.
    bool restart = false;
    QObject::connect(&settings, &SettingsController::restartRequested, &app, [&] {
        restart = true;
        if (!engine.rootObjects().isEmpty())
            QMetaObject::invokeMethod(engine.rootObjects().constFirst(), "quitApp");
        else
            QCoreApplication::quit();
    });

    const int result = app.exec();
    if (restart) {
        settings.flush();
        instance.stop(); // the new copy must not hand over to this one
        QProcess process;
        process.setProgram(QCoreApplication::applicationFilePath());
        process.setArguments(QCoreApplication::arguments().mid(1));
        if (scaleFromSettings) { // the new copy reads the new size itself
            QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
            environment.remove(QStringLiteral("QT_SCALE_FACTOR"));
            process.setProcessEnvironment(environment);
        }
        process.startDetached();
    }
    return result;
}
