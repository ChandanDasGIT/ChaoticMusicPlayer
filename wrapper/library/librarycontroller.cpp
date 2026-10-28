#include "wrapper/library/librarycontroller.h"

#include "backend/library/metadata/metadata.h"
#include "backend/library/scanner/scanner.h"
#include "backend/youtube/ytmusicclient/ytmusicclient.h"
#include "wrapper/playlist/playlistcontroller.h"

#include <QJSEngine>

LibraryController::LibraryController(PlaylistController *playlist, QObject *parent)
    : QObject(parent)
    , m_playlist(playlist)
{
    Q_ASSERT(!s_instance);
    s_instance = this;

    // Runs `fn` on this object's (GUI) thread.
    auto queued = [this](auto fn) {
        QMetaObject::invokeMethod(this, std::move(fn), Qt::QueuedConnection);
    };

    Scanner::Callbacks callbacks;
    callbacks.progress = [this, queued](size_t done, size_t total) {
        queued([this, done, total] { onProgress(int(done), int(total)); });
    };
    callbacks.tracksFound = [this, queued](std::vector<Track> tracks) {
        queued([this, tracks] { m_addedThisScan += m_playlist->addTracks(tracks, &m_addedPaths); });
    };
    callbacks.finished = [this, queued] {
        queued([this] { onFinished(); });
    };

    m_scanner = std::make_unique<Scanner>(std::move(callbacks));
}

LibraryController::~LibraryController()
{
    m_scanner.reset(); // joins the scanner thread
    s_instance = nullptr;
}

LibraryController *LibraryController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

void LibraryController::addUrls(const QList<QUrl> &urls)
{
    std::vector<std::string> paths;
    for (const QUrl &url : urls) {
        if (url.isLocalFile())
            paths.push_back(url.toLocalFile().toStdString());
    }
    if (paths.empty())
        return;

    if (m_pendingScans++ == 0) {
        m_addedThisScan = 0;
        m_addedPaths.clear();
        emit scanningChanged();
    }
    m_scanner->scan(std::move(paths));
}

void LibraryController::onProgress(int done, int total)
{
    m_scannedCount = done;
    m_totalCount = total;
    emit progressChanged();
}

void LibraryController::onFinished()
{
    if (--m_pendingScans > 0)
        return;
    m_playlist->save();
    m_playlist->playFirstIfIdle(m_addedPaths);
    m_addedPaths.clear();
    emit scanningChanged();
    emit scanFinished(m_addedThisScan);
}

QVariantList LibraryController::trackDetails(const QString &path) const
{
    if (path.isEmpty())
        return {};

    // YouTube tracks have no file: show what the playlist knows.
    if (const Track *track = m_playlist->trackForPath(path); track && !track->videoId.empty()) {
        QVariantList fields;
        const auto add = [&](const QString &label, const QString &value) {
            if (!value.isEmpty())
                fields.append(QVariantMap{{QStringLiteral("label"), label}, {QStringLiteral("value"), value}});
        };
        add(tr("Title"), QString::fromStdString(track->title));
        add(tr("Artist"), QString::fromStdString(track->artist));
        add(tr("Album"), QString::fromStdString(track->album));
        if (track->durationMs > 0)
            add(tr("Duration"), QStringLiteral("%1:%2").arg(track->durationMs / 60000)
                                    .arg(track->durationMs / 1000 % 60, 2, 10, QLatin1Char('0')));
        add(tr("Link"), QString::fromStdString(YtMusicClient::watchUrl(track->videoId)));
        return {QVariantMap{
            {QStringLiteral("title"), tr("YouTube Music")},
            {QStringLiteral("collapsible"), false},
            {QStringLiteral("fields"), fields},
        }};
    }

    const TrackDetails details = Metadata::readDetails(path.toStdString());

    const auto section = [](const QString &title, const std::vector<TrackDetails::Field> &fields,
                            bool collapsible) {
        QVariantList list;
        for (const TrackDetails::Field &field : fields) {
            list.append(QVariantMap{
                {QStringLiteral("label"), QString::fromStdString(field.label)},
                {QStringLiteral("value"), QString::fromStdString(field.value)},
            });
        }
        return QVariantMap{
            {QStringLiteral("title"), title},
            {QStringLiteral("collapsible"), collapsible},
            {QStringLiteral("fields"), list},
        };
    };

    return {
        section(tr("Tags"), details.tags, false),
        section(tr("Audio"), details.audio, false),
        section(tr("File"), details.file, false),
        section(tr("All tags"), details.rawTags, true),
    };
}
