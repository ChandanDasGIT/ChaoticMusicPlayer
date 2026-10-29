#include "wrapper/youtube/youtubecontroller.h"

#include "backend/library/track/track.h"
#include "backend/youtube/linklibrary/linklibrary.h"
#include "backend/youtube/streamresolver/streamresolver.h"
#include "backend/youtube/ytdlp/ytdlp.h"
#include "backend/youtube/ytmusicclient/ytmusicclient.h"
#include "wrapper/playlist/playlistcontroller.h"
#include "wrapper/settings/settingscontroller.h"

#include <QJSEngine>
#include <QStandardPaths>

namespace {

QVariantMap toVariant(const YtSong &song)
{
    QStringList artists;
    for (const std::string &artist : song.artists)
        artists << QString::fromStdString(artist);
    return {
        {QStringLiteral("videoId"), QString::fromStdString(song.videoId)},
        {QStringLiteral("title"), QString::fromStdString(song.title)},
        {QStringLiteral("artists"), artists.join(QStringLiteral(", "))},
        {QStringLiteral("album"), QString::fromStdString(song.album)},
        {QStringLiteral("duration"), qint64(song.durationMs)},
        {QStringLiteral("thumbnail"), QString::fromStdString(song.thumbnailUrl)},
    };
}

QVariantList toVariantList(const std::vector<YtSong> &songs)
{
    QVariantList list;
    for (const YtSong &song : songs)
        list.append(toVariant(song));
    return list;
}

YtSong fromVariant(const QVariantMap &map)
{
    YtSong song;
    song.videoId = map.value(QStringLiteral("videoId")).toString().toStdString();
    song.title = map.value(QStringLiteral("title")).toString().toStdString();
    for (const QString &artist : map.value(QStringLiteral("artists")).toString().split(QStringLiteral(", "), Qt::SkipEmptyParts))
        song.artists.push_back(artist.toStdString());
    song.album = map.value(QStringLiteral("album")).toString().toStdString();
    song.durationMs = map.value(QStringLiteral("duration")).toLongLong();
    song.thumbnailUrl = map.value(QStringLiteral("thumbnail")).toString().toStdString();
    return song;
}

::Track trackFromMap(const QVariantMap &map)
{
    ::Track track;
    track.videoId = map.value(QStringLiteral("videoId")).toString().toStdString();
    track.path = StreamResolver::pathFor(track.videoId);
    track.title = map.value(QStringLiteral("title")).toString().toStdString();
    track.artist = map.value(QStringLiteral("artists")).toString().toStdString();
    track.album = map.value(QStringLiteral("album")).toString().toStdString();
    track.durationMs = map.value(QStringLiteral("duration")).toLongLong();
    track.thumbnailUrl = map.value(QStringLiteral("thumbnail")).toString().toStdString();
    return track;
}

QString errorText(const char *what)
{
    return QObject::tr("Couldn't reach YouTube Music (%1). Check your connection.").arg(QLatin1String(what));
}

} // namespace

YouTubeController::YouTubeController(PlaylistController *playlist, SettingsController *settings, QObject *parent)
    : QObject(parent)
    , m_playlist(playlist)
    , m_settings(settings)
{
    Q_ASSERT(!s_instance);
    s_instance = this;
    m_pool.setMaxThreadCount(3);

    const QString file = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                         + QStringLiteral("/ChaoticMusicPlayer/savedlinks.tsv");
    m_links = std::make_unique<LinkLibrary>(file.toStdString());
    m_links->load();

    connect(m_playlist, &PlaylistController::moreWanted, this, &YouTubeController::fetchMore);
    connect(m_settings, &SettingsController::ytCookiesBrowserChanged, this, [this] {
        m_librarySongs.clear();
        m_libraryError.clear();
        emit libraryChanged();
        emit signedInChanged();
    });

    runAsync([] { return YtDlp::version(); },
             [this](const std::optional<std::string> &version) {
                 m_available = version.has_value();
                 m_ytDlpVersion = version ? QString::fromStdString(*version) : QString();
                 emit availabilityChanged();
             });
}

::Track YouTubeController::trackFromSong(const QVariantMap &song)
{
    return trackFromMap(song);
}

YouTubeController::~YouTubeController()
{
    m_pool.waitForDone(); // no task may post to a destroyed object
    s_instance = nullptr;
}

YouTubeController *YouTubeController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

// Runs `work` on the pool and hands its result to `done` on the GUI thread.
template<typename Work, typename Done>
void YouTubeController::runAsync(Work work, Done done)
{
    m_pool.start([this, work = std::move(work), done = std::move(done)]() mutable {
        auto result = work();
        QMetaObject::invokeMethod(this, [done = std::move(done), result = std::move(result)]() mutable {
            done(result);
        }, Qt::QueuedConnection);
    });
}

bool YouTubeController::signedIn() const
{
    return !m_settings->ytCookiesBrowser().isEmpty();
}

QVariantList YouTubeController::savedSongs() const
{
    return toVariantList(m_links->songs());
}

void YouTubeController::search(const QString &query)
{
    const QString q = query.trimmed();
    const int generation = ++m_searchGeneration;
    m_searchError.clear();
    if (q.isEmpty()) {
        m_searchResults.clear();
        m_searching = false;
        emit searchChanged();
        return;
    }
    m_searching = true;
    emit searchChanged();

    runAsync([q] { return YtMusicClient::searchSongs(q.toStdString()); },
             [this, generation](const std::optional<std::vector<YtSong>> &songs) {
                 if (generation != m_searchGeneration)
                     return; // a newer search is running
                 m_searching = false;
                 m_searchResults = songs ? toVariantList(*songs) : QVariantList();
                 m_searchError = songs ? QString() : errorText("search");
                 emit searchChanged();
             });
}

void YouTubeController::suggest(const QString &input)
{
    const QString text = input.trimmed();
    const int generation = ++m_suggestGeneration;
    if (text.isEmpty()) {
        if (!m_suggestions.isEmpty()) {
            m_suggestions.clear();
            emit suggestionsChanged();
        }
        return;
    }

    runAsync([text] { return YtMusicClient::searchSuggestions(text.toStdString()); },
             [this, generation](const std::optional<std::vector<std::string>> &found) {
                 if (generation != m_suggestGeneration || !found)
                     return; // a newer request is running, or no answer (keep the old ones)
                 m_suggestions.clear();
                 for (const std::string &suggestion : *found)
                     m_suggestions << QString::fromStdString(suggestion);
                 emit suggestionsChanged();
             });
}

void YouTubeController::loadHome()
{
    if (m_homeLoading)
        return;
    m_homeLoading = true;
    m_homeError.clear();
    emit homeChanged();

    runAsync([] { return YtMusicClient::home(); },
             [this](const std::optional<std::vector<YtShelf>> &shelves) {
                 m_homeLoading = false;
                 m_homeShelves.clear();
                 if (!shelves) {
                     m_homeError = errorText("home");
                 } else {
                     for (const YtShelf &shelf : *shelves) {
                         QVariantList items;
                         for (const YtCollection &item : shelf.items) {
                             items.append(QVariantMap{
                                 {QStringLiteral("browseId"), QString::fromStdString(item.browseId)},
                                 {QStringLiteral("title"), QString::fromStdString(item.title)},
                                 {QStringLiteral("subtitle"), QString::fromStdString(item.subtitle)},
                                 {QStringLiteral("thumbnail"), QString::fromStdString(item.thumbnailUrl)},
                             });
                         }
                         m_homeShelves.append(QVariantMap{
                             {QStringLiteral("title"), QString::fromStdString(shelf.title)},
                             {QStringLiteral("items"), items},
                         });
                     }
                 }
                 emit homeChanged();
             });
}

void YouTubeController::openCollection(const QString &browseId, const QString &title)
{
    const int generation = ++m_collectionGeneration;
    m_collectionOpen = true;
    m_collectionTitle = title;
    m_collectionSongs.clear();
    m_collectionError.clear();
    m_collectionLoading = true;
    emit collectionChanged();

    runAsync([id = browseId.toStdString()] { return YtMusicClient::collection(id); },
             [this, generation](const std::optional<std::vector<YtSong>> &songs) {
                 if (generation != m_collectionGeneration)
                     return;
                 m_collectionLoading = false;
                 m_collectionSongs = songs ? toVariantList(*songs) : QVariantList();
                 m_collectionError = songs ? QString() : errorText("playlist");
                 emit collectionChanged();
             });
}

void YouTubeController::fetchCollection(const QString &browseId, const QString &title)
{
    emit notice(tr("Adding “%1”…").arg(title));
    runAsync([id = browseId.toStdString()] { return YtMusicClient::collection(id); },
             [this, browseId](const std::optional<std::vector<YtSong>> &songs) {
                 if (!songs || songs->empty()) {
                     emit notice(songs ? tr("That playlist is empty") : errorText("playlist"));
                     return;
                 }
                 emit collectionFetched(browseId, toVariantList(*songs));
             });
}

void YouTubeController::closeCollection()
{
    ++m_collectionGeneration;
    m_collectionOpen = false;
    m_collectionLoading = false;
    emit collectionChanged();
}

void YouTubeController::loadLibrary()
{
    if (!signedIn()) {
        m_libraryError = tr("Choose your browser under Settings → YouTube Music to sign in.");
        emit libraryChanged();
        return;
    }
    if (m_libraryLoading)
        return;
    m_libraryLoading = true;
    m_libraryError.clear();
    emit libraryChanged();

    runAsync([browser = m_settings->ytCookiesBrowser().toStdString()] {
                 auto songs = YtDlp::likedSongs(browser);
                 return std::make_pair(songs, YtDlp::lastError());
             },
             [this](const std::pair<std::optional<std::vector<YtSong>>, std::string> &result) {
                 m_libraryLoading = false;
                 m_librarySongs = result.first ? toVariantList(*result.first) : QVariantList();
                 m_libraryError = result.first ? QString()
                                               : tr("Couldn't read your library: %1").arg(QString::fromStdString(result.second));
                 emit libraryChanged();
             });
}

void YouTubeController::play(const QVariantMap &song)
{
    const QString title = song.value(QStringLiteral("title")).toString();
    const bool mix = m_settings->ytAutoplay();
    m_playlist->playYouTube({trackFromMap(song)}, 0, mix ? tr("Mix – %1").arg(title) : title, mix);
}

void YouTubeController::playList(const QVariantList &songs, int index, const QString &title)
{
    std::vector<::Track> tracks;
    for (const QVariant &song : songs)
        tracks.push_back(trackFromMap(song.toMap()));
    m_playlist->playYouTube(std::move(tracks), index, title, m_settings->ytAutoplay());
}

// What YouTube plays after `seedPath`: more for the Mix playing, or a Mix for
// after the Queue's last song.
void YouTubeController::fetchMore(const QString &seedPath, bool forQueue)
{
    const std::string videoId = StreamResolver::videoId(seedPath.toStdString());
    const ::Track *seed = m_playlist->trackForPath(seedPath);
    const QString seedTitle = seed ? QString::fromStdString(seed->title) : QString();
    runAsync([videoId] { return YtMusicClient::upNext(videoId); },
             [this, videoId, seedTitle, forQueue](const std::optional<std::vector<YtSong>> &songs) {
                 if (!songs) {
                     emit notice(errorText("up next"));
                     return;
                 }
                 std::vector<::Track> tracks;
                 for (const YtSong &song : *songs)
                     if (song.videoId != videoId)
                         tracks.push_back(trackFromMap(toVariant(song)));
                 if (forQueue)
                     m_playlist->setFollowUp(std::move(tracks), tr("Mix – %1").arg(seedTitle));
                 else
                     m_playlist->extendList(tracks);
             });
}

void YouTubeController::enqueue(const QVariantMap &song)
{
    m_playlist->addAndPlay({trackFromMap(song)}, false);
    emit notice(tr("Added “%1” to the Queue").arg(song.value(QStringLiteral("title")).toString()));
}

void YouTubeController::save(const QVariantMap &song)
{
    if (m_links->add(fromVariant(song))) {
        m_links->save();
        emit savedChanged();
        emit notice(tr("Saved “%1”").arg(song.value(QStringLiteral("title")).toString()));
    } else {
        emit notice(tr("Already saved"));
    }
}

void YouTubeController::addLink(const QString &url)
{
    const QString link = url.trimmed();
    if (!link.contains(QLatin1String("youtu"))) {
        emit notice(tr("That doesn't look like a YouTube link."));
        return;
    }
    if (m_addingLink)
        return;
    m_addingLink = true;
    emit savedChanged();

    runAsync([link = link.toStdString(), browser = m_settings->ytCookiesBrowser().toStdString()] {
                 auto songs = YtDlp::entries(link, browser);
                 return std::make_pair(songs, YtDlp::lastError());
             },
             [this](const std::pair<std::optional<std::vector<YtSong>>, std::string> &result) {
                 m_addingLink = false;
                 int added = 0;
                 if (result.first) {
                     // Keep the link's order at the top of the list.
                     for (auto it = result.first->rbegin(); it != result.first->rend(); ++it)
                         added += m_links->add(*it) ? 1 : 0;
                     m_links->save();
                 }
                 emit savedChanged();
                 if (!result.first)
                     emit notice(tr("Couldn't read that link: %1").arg(QString::fromStdString(result.second)));
                 else if (added == 0)
                     emit notice(tr("Already saved"));
                 else
                     emit notice(added == 1 ? tr("Saved 1 song") : tr("Saved %1 songs").arg(added));
             });
}

void YouTubeController::removeSaved(int index)
{
    if (index < 0 || size_t(index) >= m_links->songs().size())
        return;
    m_links->remove(size_t(index));
    m_links->save();
    emit savedChanged();
}
