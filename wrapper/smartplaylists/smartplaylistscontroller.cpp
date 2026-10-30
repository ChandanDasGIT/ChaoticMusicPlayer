#include "wrapper/smartplaylists/smartplaylistscontroller.h"

#include "wrapper/files/files.h"
#include "backend/youtube/streamresolver/streamresolver.h"
#include "wrapper/history/historycontroller.h"
#include "wrapper/locallibrary/locallibrarycontroller.h"
#include "wrapper/playlist/playlistcontroller.h"

#include <QDateTime>
#include <QJSEngine>
#include <QStandardPaths>

#include <unordered_map>

namespace {

QString qs(const std::string &s)
{
    return QString::fromStdString(s);
}

} // namespace

SmartPlaylistsController::SmartPlaylistsController(HistoryController *history, LocalLibraryController *library,
                                                   PlaylistController *queue, QObject *parent)
    : QObject(parent)
    , m_history(history)
    , m_library(library)
    , m_queue(queue)
    , m_store(std::make_unique<SmartPlaylistStore>(
          (Files::dataDir()
           + QStringLiteral("/smartplaylists.json"))
              .toStdString()))
{
    Q_ASSERT(!s_instance);
    s_instance = this;
    m_store->load();
    connect(m_history, &HistoryController::changed, this, &SmartPlaylistsController::refresh);
    connect(m_library, &LocalLibraryController::contentsChanged, this, &SmartPlaylistsController::refresh);
    refresh();
}

SmartPlaylistsController::~SmartPlaylistsController()
{
    s_instance = nullptr;
}

SmartPlaylistsController *SmartPlaylistsController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

// Every song a smart playlist can pick: the library, and the YouTube songs in
// the history, each with its play count and last play.
std::vector<SmartCandidate> SmartPlaylistsController::candidates() const
{
    struct Played
    {
        int plays = 0;
        int64_t last = 0;
        const ListeningHistory::Play *latest = nullptr;
    };
    std::unordered_map<std::string, Played> played;
    for (const ListeningHistory::Play &play : m_history->plays()) {
        Played &p = played[play.path];
        ++p.plays;
        p.last = std::max(p.last, play.time);
        p.latest = &play;
    }

    std::vector<SmartCandidate> out;
    out.reserve(m_library->tracks().size());
    for (const ::Track &t : m_library->tracks()) {
        SmartCandidate c;
        c.track = t;
        if (const auto it = played.find(t.path); it != played.end()) {
            c.plays = it->second.plays;
            c.lastPlayed = it->second.last;
        }
        out.push_back(std::move(c));
    }
    for (const auto &[path, p] : played) {
        if (!StreamResolver::isYouTube(path))
            continue;
        SmartCandidate c;
        c.youTube = true;
        c.plays = p.plays;
        c.lastPlayed = p.last;
        c.track.path = path;
        c.track.videoId = StreamResolver::videoId(path);
        c.track.title = p.latest->title;
        c.track.artist = p.latest->artist;
        c.track.album = p.latest->album;
        c.track.durationMs = p.latest->durationMs;
        c.track.thumbnailUrl = p.latest->thumbnailUrl;
        out.push_back(std::move(c));
    }
    return out;
}

void SmartPlaylistsController::refresh()
{
    const std::vector<SmartCandidate> songs = candidates();
    const int64_t now = QDateTime::currentSecsSinceEpoch();
    m_list.clear();
    for (const SmartPlaylist &p : m_store->playlists()) {
        QVariantMap map = toMap(p);
        map.insert(QStringLiteral("count"), int(evaluate(p, songs, now).size()));
        m_list.append(map);
    }
    emit changed();
}

void SmartPlaylistsController::play(const QString &id)
{
    const SmartPlaylist *p = m_store->find(id.toStdString());
    if (!p)
        return;
    std::vector<::Track> tracks = evaluate(*p, candidates(), QDateTime::currentSecsSinceEpoch());
    if (!tracks.empty())
        m_queue->playTitled(std::move(tracks), 0, qs(p->name), false);
}

QString SmartPlaylistsController::save(const QVariantMap &playlist)
{
    const QString id = qs(m_store->put(fromMap(playlist)));
    refresh();
    return id;
}

void SmartPlaylistsController::remove(const QString &id)
{
    m_store->remove(id.toStdString());
    refresh();
}

int SmartPlaylistsController::preview(const QVariantMap &playlist) const
{
    return int(evaluate(fromMap(playlist), candidates(), QDateTime::currentSecsSinceEpoch()).size());
}

QVariantList SmartPlaylistsController::items(const QString &id) const
{
    QVariantList out;
    const SmartPlaylist *p = m_store->find(id.toStdString());
    if (!p)
        return out;
    for (const ::Track &t : evaluate(*p, candidates(), QDateTime::currentSecsSinceEpoch())) {
        if (StreamResolver::isYouTube(t.path))
            out.append(QVariantMap{{QStringLiteral("videoId"), qs(t.videoId)},
                                   {QStringLiteral("title"), qs(t.title)},
                                   {QStringLiteral("artists"), qs(t.artist)},
                                   {QStringLiteral("album"), qs(t.album)},
                                   {QStringLiteral("duration"), qint64(t.durationMs)},
                                   {QStringLiteral("thumbnail"), qs(t.thumbnailUrl)}});
        else
            out.append(QVariantMap{{QStringLiteral("path"), qs(t.path)}});
    }
    return out;
}

SmartPlaylist SmartPlaylistsController::fromMap(const QVariantMap &map)
{
    SmartPlaylist p;
    p.id = map.value(QStringLiteral("id")).toString().toStdString();
    p.name = map.value(QStringLiteral("name")).toString().trimmed().toStdString();
    p.matchAll = map.value(QStringLiteral("matchAll"), true).toBool();
    p.order = map.value(QStringLiteral("order"), QStringLiteral("plays")).toString().toStdString();
    p.limit = std::max(0, map.value(QStringLiteral("limit")).toInt());
    for (const QVariant &r : map.value(QStringLiteral("rules")).toList()) {
        const QVariantMap rule = r.toMap();
        p.rules.push_back({rule.value(QStringLiteral("field")).toString().toStdString(),
                           rule.value(QStringLiteral("op")).toString().toStdString(),
                           rule.value(QStringLiteral("value")).toString().toStdString()});
    }
    return p;
}

QVariantMap SmartPlaylistsController::toMap(const SmartPlaylist &p)
{
    QVariantList rules;
    for (const SmartRule &r : p.rules)
        rules.append(QVariantMap{{QStringLiteral("field"), qs(r.field)},
                                 {QStringLiteral("op"), qs(r.op)},
                                 {QStringLiteral("value"), qs(r.value)}});
    return {{QStringLiteral("id"), qs(p.id)},       {QStringLiteral("name"), qs(p.name)},
            {QStringLiteral("matchAll"), p.matchAll}, {QStringLiteral("rules"), rules},
            {QStringLiteral("order"), qs(p.order)}, {QStringLiteral("limit"), p.limit}};
}
