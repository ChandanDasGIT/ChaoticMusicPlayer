// Windows: the System Media Transport Controls (the overlay media keys and the

#include "wrapper/files/files.h"
// volume flyout show, with title, artist, cover and play / pause / next /
// previous), through C++/WinRT.
#include "wrapper/systemmedia/systemmedia.h"

#include "backend/library/track/track.h"
#include "wrapper/albumart/covercontroller.h"
#include "wrapper/player/playercontroller.h"
#include "wrapper/playlist/playlistcontroller.h"

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QPointer>
#include <QStandardPaths>
#include <QThreadPool>
#include <QTimer>
#include <QWindow>

#include <windows.h>
#include <systemmediatransportcontrolsinterop.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>

using namespace winrt::Windows::Media;
using winrt::Windows::Storage::StorageFile;
using winrt::Windows::Storage::Streams::RandomAccessStreamReference;

namespace {

winrt::hstring hs(const QString &text)
{
    return winrt::hstring(reinterpret_cast<const wchar_t *>(text.utf16()), uint32_t(text.size()));
}

} // namespace

struct SystemMedia::Impl
{
    PlayerController *player;
    PlaylistController *playlist;
    SystemMediaTransportControls controls{nullptr};
    winrt::event_token buttonToken{};
    QTimer updateTimer;
    QString coverSource; // the song whose cover is shown
    QString coverFile;   // its cover, saved for Windows to read
};

SystemMedia::SystemMedia(PlayerController *player, PlaylistController *playlist, QObject *parent)
    : QObject(parent)
    , m_impl(std::make_unique<Impl>())
{
    m_impl->player = player;
    m_impl->playlist = playlist;
    m_impl->updateTimer.setSingleShot(true);
    m_impl->updateTimer.setInterval(100);

    // The controls belong to a window: the app's, once it exists
    auto attach = [this] {
        if (m_impl->controls)
            return true;
        const QWindowList windows = QGuiApplication::topLevelWindows();
        if (windows.isEmpty())
            return false;
        try {
            auto interop = winrt::get_activation_factory<SystemMediaTransportControls, ISystemMediaTransportControlsInterop>();
            winrt::check_hresult(interop->GetForWindow(reinterpret_cast<HWND>(windows.constFirst()->winId()),
                                                       winrt::guid_of<SystemMediaTransportControls>(),
                                                       winrt::put_abi(m_impl->controls)));
        } catch (...) {
            m_impl->controls = nullptr;
            return true; // not available: give up quietly
        }
        auto &c = m_impl->controls;
        c.IsPlayEnabled(true);
        c.IsPauseEnabled(true);
        c.IsNextEnabled(true);
        c.IsPreviousEnabled(true);
        c.IsStopEnabled(true);
        c.DisplayUpdater().Type(MediaPlaybackType::Music);
        // Pressed on Windows' own thread: acted on in the app's
        QPointer<SystemMedia> self(this);
        m_impl->buttonToken = c.ButtonPressed([self](auto &&, const SystemMediaTransportControlsButtonPressedEventArgs &args) {
            const auto button = args.Button();
            QMetaObject::invokeMethod(qApp, [self, button] {
                if (!self)
                    return;
                Impl &impl = *self->m_impl;
                switch (button) {
                case SystemMediaTransportControlsButton::Play:
                    if (impl.player->hasTrack())
                        impl.player->play();
                    else
                        impl.playlist->playPause();
                    break;
                case SystemMediaTransportControlsButton::Pause:
                    impl.player->pause();
                    break;
                case SystemMediaTransportControlsButton::Stop:
                    impl.player->stop();
                    break;
                case SystemMediaTransportControlsButton::Next:
                    impl.playlist->next();
                    break;
                case SystemMediaTransportControlsButton::Previous:
                    impl.playlist->previous();
                    break;
                default:
                    break;
                }
            }, Qt::QueuedConnection);
        });
        m_impl->updateTimer.start();
        return true;
    };
    if (!attach()) {
        auto *retry = new QTimer(this);
        retry->setInterval(200);
        connect(retry, &QTimer::timeout, this, [retry, attach] {
            if (attach())
                retry->deleteLater();
        });
        retry->start();
    }

    // What is playing, and how
    connect(&m_impl->updateTimer, &QTimer::timeout, this, [this] {
        Impl &impl = *m_impl;
        if (!impl.controls)
            return;
        try {
            switch (impl.player->state()) {
            case PlayerController::Playing:
                impl.controls.PlaybackStatus(MediaPlaybackStatus::Playing);
                break;
            case PlayerController::Paused:
                impl.controls.PlaybackStatus(MediaPlaybackStatus::Paused);
                break;
            default:
                impl.controls.PlaybackStatus(impl.player->hasTrack() ? MediaPlaybackStatus::Stopped
                                                                     : MediaPlaybackStatus::Closed);
            }
            auto updater = impl.controls.DisplayUpdater();
            const QString source = impl.player->source();
            const Track *track = impl.playlist->currentTrack();
            updater.Type(MediaPlaybackType::Music);
            auto music = updater.MusicProperties();
            music.Title(hs(track ? QString::fromStdString(track->title) : impl.player->title()));
            music.Artist(hs(track ? QString::fromStdString(track->artist) : QString()));
            music.AlbumTitle(hs(track ? QString::fromStdString(track->album) : QString()));
            updater.Update();
            impl.controls.IsEnabled(impl.player->hasTrack());

            // The cover: YouTube's thumbnail address, or the file's cover saved for Windows
            if (source == impl.coverSource)
                return;
            impl.coverSource = source;
            if (track && !track->thumbnailUrl.empty()) {
                updater.Thumbnail(RandomAccessStreamReference::CreateFromUri(
                    winrt::Windows::Foundation::Uri(hs(QString::fromStdString(track->thumbnailUrl)))));
                updater.Update();
                return;
            }
            updater.Thumbnail(nullptr);
            updater.Update();
            QPointer<SystemMedia> self(this);
            auto controls = impl.controls;
            QThreadPool::globalInstance()->start([self, controls, source] {
                const std::vector<unsigned char> data = CoverController::coverBytes(source.toStdString());
                if (data.empty())
                    return;
                const QString dir = Files::cacheDir();
                QDir().mkpath(dir);
                const QString file = dir + QStringLiteral("/media-controls-cover");
                QFile out(file);
                if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
                    return;
                out.write(reinterpret_cast<const char *>(data.data()), qint64(data.size()));
                out.close();
                try {
                    winrt::init_apartment(winrt::apartment_type::multi_threaded);
                } catch (...) {
                }
                try {
                    const StorageFile storage = StorageFile::GetFileFromPathAsync(hs(QDir::toNativeSeparators(file))).get();
                    auto updater = controls.DisplayUpdater();
                    updater.Thumbnail(RandomAccessStreamReference::CreateFromFile(storage));
                    updater.Update();
                } catch (...) {
                }
            });
        } catch (...) {
        }
    });
    auto schedule = [this] { m_impl->updateTimer.start(); };
    connect(player, &PlayerController::stateChanged, this, schedule);
    connect(player, &PlayerController::sourceChanged, this, schedule);
    connect(playlist, &PlaylistController::currentChanged, this, schedule);
}

SystemMedia::~SystemMedia()
{
    if (m_impl->controls) {
        try {
            m_impl->controls.ButtonPressed(m_impl->buttonToken);
            m_impl->controls.IsEnabled(false);
        } catch (...) {
        }
    }
}
