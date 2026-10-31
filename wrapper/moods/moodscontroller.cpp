#include "wrapper/moods/moodscontroller.h"

#include "backend/library/track/track.h"
#include "backend/playlist/playlistlibrary/playlistlibrary.h"
#include "backend/youtube/streamresolver/streamresolver.h"
#include "wrapper/files/files.h"
#include "wrapper/player/playercontroller.h"
#include "wrapper/playlist/playlistcontroller.h"

#include <QJSEngine>
#include <QVariantMap>

#include <algorithm>

namespace {

QString qs(const std::string &s)
{
    return QString::fromStdString(s);
}

QVariantMap songOf(const Track &track)
{
    QVariantMap song{{QStringLiteral("path"), qs(track.path)},
                     {QStringLiteral("title"), qs(track.title)},
                     {QStringLiteral("artist"), qs(track.artist)},
                     {QStringLiteral("album"), qs(track.album)},
                     {QStringLiteral("thumbnail"), qs(track.thumbnailUrl)},
                     {QStringLiteral("durationMs"), qint64(track.durationMs)}};
    if (StreamResolver::isYouTube(track.path)) {
        song.insert(QStringLiteral("item"),
                    QVariantMap{{QStringLiteral("videoId"), qs(StreamResolver::videoId(track.path))},
                                {QStringLiteral("title"), qs(track.title)},
                                {QStringLiteral("artists"), qs(track.artist)},
                                {QStringLiteral("album"), qs(track.album)},
                                {QStringLiteral("duration"), qint64(track.durationMs)},
                                {QStringLiteral("thumbnail"), qs(track.thumbnailUrl)}});
    } else {
        song.insert(QStringLiteral("item"), QVariantMap{{QStringLiteral("path"), qs(track.path)}});
    }
    return song;
}

} // namespace

MoodsController::MoodsController(PlaylistController *queue, PlayerController *player, QObject *parent)
    : QObject(parent)
    , m_queue(queue)
    , m_library(std::make_unique<PlaylistLibrary>((Files::dataDir() + QStringLiteral("/moods")).toStdString()))
{
    Q_ASSERT(!s_instance);
    s_instance = this;

    m_library->load();
    for (const char *name : {"Happy", "Sad", "Bored", "Suspense", "METAL", "Dance"}) {
        const auto &all = m_library->playlists();
        const auto known = std::find_if(all.begin(), all.end(), [&](const PlaylistLibrary::Entry &e) { return e.name == name; });
        m_ids << qs(known != all.end() ? known->id : m_library->create(name));
    }

    connect(queue, &PlaylistController::currentChanged, this, &MoodsController::currentChanged);
    connect(player, &PlayerController::sourceChanged, this, &MoodsController::currentChanged);
}

MoodsController::~MoodsController()
{
    s_instance = nullptr;
}

MoodsController *MoodsController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

QVariantList MoodsController::moods() const
{
    QVariantList out;
    for (const QString &id : m_ids) {
        if (const PlaylistLibrary::Entry *entry = m_library->find(id.toStdString()))
            out.append(QVariantMap{{QStringLiteral("id"), id},
                                   {QStringLiteral("name"), qs(entry->name)},
                                   {QStringLiteral("count"), int(entry->tracks.size())}});
    }
    return out;
}

QStringList MoodsController::currentMoods() const
{
    QStringList out;
    const Track *current = m_queue->currentTrack();
    if (!current)
        return out;
    for (const QString &id : m_ids) {
        const PlaylistLibrary::Entry *entry = m_library->find(id.toStdString());
        if (entry && std::any_of(entry->tracks.begin(), entry->tracks.end(),
                                 [&](const Track &t) { return t.path == current->path; }))
            out << id;
    }
    return out;
}

void MoodsController::toggleCurrent(const QString &id)
{
    const Track *current = m_queue->currentTrack();
    const PlaylistLibrary::Entry *entry = m_library->find(id.toStdString());
    if (!current || !entry)
        return;
    const auto found = std::find_if(entry->tracks.begin(), entry->tracks.end(),
                                    [&](const Track &t) { return t.path == current->path; });
    if (found != entry->tracks.end())
        m_library->removeTrack(id.toStdString(), size_t(found - entry->tracks.begin()));
    else
        m_library->addTracks(id.toStdString(), {*current});
    emit changed();
    emit currentChanged();
}

QVariantList MoodsController::songs(const QString &id) const
{
    QVariantList out;
    if (const PlaylistLibrary::Entry *entry = m_library->find(id.toStdString())) {
        for (const Track &track : entry->tracks)
            out.append(songOf(track));
    }
    return out;
}

void MoodsController::play(const QString &id, int index)
{
    const PlaylistLibrary::Entry *entry = m_library->find(id.toStdString());
    if (entry && index >= 0 && size_t(index) < entry->tracks.size())
        m_queue->playTitled(entry->tracks, index, qs(entry->name), false);
}

void MoodsController::removeSong(const QString &id, int index)
{
    const PlaylistLibrary::Entry *entry = m_library->find(id.toStdString());
    if (!entry || index < 0 || size_t(index) >= entry->tracks.size())
        return;
    m_library->removeTrack(id.toStdString(), size_t(index));
    emit changed();
    emit currentChanged();
}
