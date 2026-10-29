#include "wrapper/locallibrary/locallibrarycontroller.h"

#include "backend/library/musiclibrary/musiclibrary.h"
#include "wrapper/playlist/playlistcontroller.h"
#include "wrapper/playlists/playlistscontroller.h"
#include "wrapper/youtube/ytmetadatacontroller.h"
#include "wrapper/settings/settingscontroller.h"

#include <QCollator>
#include <QDir>
#include <QFileInfo>
#include <QJSEngine>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QUrl>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <map>
#include <set>

namespace {

std::string lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return s;
}

// Same album name in the same folder = one album.
std::string albumKey(const Track &track)
{
    return lower(track.album) + '\x1f' + std::filesystem::path(track.path).parent_path().string();
}

// "A, B & C feat. D" -> A, B, C, D
std::vector<std::string> artistNames(const std::string &artist)
{
    std::vector<std::string> names{artist};
    for (const char *separator : {", ", "; ", " & ", " feat. ", " ft. ", " Feat. ", " Ft. "}) {
        std::vector<std::string> split;
        for (const std::string &name : names) {
            size_t start = 0;
            for (size_t at; (at = name.find(separator, start)) != std::string::npos; start = at + std::strlen(separator))
                split.push_back(name.substr(start, at - start));
            split.push_back(name.substr(start));
        }
        names = std::move(split);
    }
    names.erase(std::remove_if(names.begin(), names.end(), [](const std::string &n) { return n.empty(); }),
                names.end());
    return names;
}

bool byTitle(const Track &a, const Track &b)
{
    const std::string la = lower(a.title), lb = lower(b.title);
    return la != lb ? la < lb : a.path < b.path;
}

bool byAlbumOrder(const Track &a, const Track &b)
{
    if (a.album != b.album)
        return lower(a.album) < lower(b.album);
    if (a.disc != b.disc)
        return a.disc < b.disc;
    if (a.track != b.track)
        return a.track < b.track;
    return byTitle(a, b);
}

// "1 song" / "9 songs"
QString songsText(int count)
{
    return count == 1 ? LocalLibraryController::tr("1 song") : LocalLibraryController::tr("%1 songs").arg(count);
}

QString lengthText(int64_t ms)
{
    if (ms > 0 && ms < 30000)
        return LocalLibraryController::tr("<1 min");
    return LocalLibraryController::tr("%1 min").arg((ms + 30000) / 60000);
}

std::vector<std::string> toStd(const QStringList &list)
{
    std::vector<std::string> out;
    for (const QString &s : list)
        out.push_back(s.toStdString());
    return out;
}

} // namespace

LocalLibraryController::LocalLibraryController(PlaylistController *queue, PlaylistsController *playlists,
                                               YtMetadataController *ytMetadata, SettingsController *settings,
                                               QObject *parent)
    : QObject(parent)
    , m_queue(queue)
    , m_playlists(playlists)
    , m_ytMetadata(ytMetadata)
    , m_settings(settings)
    , m_library(std::make_shared<MusicLibrary>(
          (QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
           + QStringLiteral("/ChaoticMusicPlayer/library.tsv"))
              .toStdString()))
    , m_cancel(std::make_shared<std::atomic<bool>>(false))
    , m_songs(std::make_unique<LibrarySongsModel>())
    , m_detail(std::make_unique<LibrarySongsModel>())
    , m_folderSongs(std::make_unique<LibrarySongsModel>())
{
    Q_ASSERT(!s_instance);
    s_instance = this;
    QJSEngine::setObjectOwnership(m_songs.get(), QJSEngine::CppOwnership);
    QJSEngine::setObjectOwnership(m_detail.get(), QJSEngine::CppOwnership);
    QJSEngine::setObjectOwnership(m_folderSongs.get(), QJSEngine::CppOwnership);
    m_pool.setMaxThreadCount(1);

    m_playlists->setTrackLookup([this](const QString &path) { return trackForPath(path); });
    m_ytMetadata->setTrackLookup([this](const QString &path) { return trackForPath(path); });

    connect(m_settings, &SettingsController::libraryFoldersChanged, this, &LocalLibraryController::refresh);
    connect(m_settings, &SettingsController::libraryFoldersChanged, this, &LocalLibraryController::rebuildFolder);
    connect(m_settings, &SettingsController::libraryExcludedChanged, this, &LocalLibraryController::refresh);
    startRefresh(true); // the saved library at once, then a check for changes
}

LocalLibraryController::~LocalLibraryController()
{
    m_cancel->store(true);
    m_pool.waitForDone();
    m_playlists->setTrackLookup({});
    m_ytMetadata->setTrackLookup({});
    s_instance = nullptr;
}

LocalLibraryController *LocalLibraryController::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

const Track *LocalLibraryController::trackForPath(const QString &path) const
{
    const auto it = m_byPath.find(path.toStdString());
    return it != m_byPath.end() ? &m_tracks[it->second] : nullptr;
}

void LocalLibraryController::refresh()
{
    startRefresh(false);
}

void LocalLibraryController::addFolder(const QString &folder)
{
    if (!m_settings->addLibraryFolder(folder))
        emit notice(tr("That folder is already in the library"));
    // (libraryFoldersChanged starts the refresh)
}

void LocalLibraryController::removeFolder(const QString &folder)
{
    m_settings->removeLibraryFolder(folder);
}

void LocalLibraryController::startRefresh(bool loadIndexFirst)
{
    if (m_scanning) {
        // Folders changed meanwhile: stop this pass and start over with the new ones.
        m_cancel->store(true);
        m_refreshAgain = true;
        return;
    }
    m_scanning = true;
    m_scanDone = m_scanTotal = 0;
    emit scanChanged();

    auto cancel = std::make_shared<std::atomic<bool>>(false);
    m_cancel = cancel;
    const std::vector<std::string> folders = toStd(m_settings->libraryFolders());
    const std::vector<std::string> excluded = toStd(m_settings->libraryExcluded());
    const std::shared_ptr<MusicLibrary> library = m_library;

    m_pool.start([this, library, cancel, folders, excluded, loadIndexFirst] {
        if (loadIndexFirst) {
            library->load();
            QMetaObject::invokeMethod(this, [this, tracks = library->tracks()]() mutable {
                m_loaded = true;
                setTracks(std::move(tracks));
            }, Qt::QueuedConnection);
        }
        const auto progress = [this](size_t done, size_t total) {
            QMetaObject::invokeMethod(this, [this, done, total] {
                m_scanDone = int(done);
                m_scanTotal = int(total);
                emit scanChanged();
            }, Qt::QueuedConnection);
        };
        const MusicLibrary::RefreshResult result = library->refresh(folders, excluded, progress, *cancel);
        if (!result.cancelled)
            library->save();

        QMetaObject::invokeMethod(this, [this, result, tracks = library->tracks()]() mutable {
            if (!result.cancelled) {
                m_loaded = true;
                setTracks(std::move(tracks));
            }
            m_scanning = false;
            emit scanChanged();
            if (m_refreshAgain) {
                m_refreshAgain = false;
                startRefresh(false);
            }
        }, Qt::QueuedConnection);
    });
}

void LocalLibraryController::setTracks(std::vector<Track> tracks)
{
    std::sort(tracks.begin(), tracks.end(), byTitle);
    m_tracks = std::move(tracks);
    m_byPath.clear();
    for (size_t i = 0; i < m_tracks.size(); ++i)
        m_byPath[m_tracks[i].path] = i;

    // Albums
    struct Album
    {
        QString name;
        std::set<std::string> artists;
        QString coverPath;
        int count = 0;
    };
    std::map<std::string, Album> albums; // by key
    for (const Track &t : m_tracks) {
        if (t.album.empty())
            continue;
        Album &album = albums[albumKey(t)];
        album.name = QString::fromStdString(t.album);
        album.artists.insert(t.artist);
        if (album.coverPath.isEmpty() || (t.hasCover && album.count == 0))
            album.coverPath = QString::fromStdString(t.path);
        ++album.count;
    }
    std::vector<std::pair<std::string, Album>> sortedAlbums(albums.begin(), albums.end());
    std::sort(sortedAlbums.begin(), sortedAlbums.end(), [](const auto &a, const auto &b) {
        return a.second.name.compare(b.second.name, Qt::CaseInsensitive) < 0;
    });
    m_albums.clear();
    for (const auto &[key, album] : sortedAlbums) {
        const QString artist = album.artists.size() == 1 ? QString::fromStdString(*album.artists.begin())
                                                         : tr("Various artists");
        m_albums.append(QVariantMap{{QStringLiteral("key"), QString::fromStdString(key)},
                                    {QStringLiteral("album"), album.name},
                                    {QStringLiteral("artist"), artist},
                                    {QStringLiteral("coverPath"), album.coverPath},
                                    {QStringLiteral("count"), album.count}});
    }

    // Artists
    std::map<std::string, std::pair<QString, int>> artists; // lowercase -> (name, songs)
    for (const Track &t : m_tracks) {
        for (const std::string &name : artistNames(t.artist)) {
            auto &entry = artists[lower(name)];
            if (entry.first.isEmpty())
                entry.first = QString::fromStdString(name);
            ++entry.second;
        }
    }
    m_artists.clear();
    for (const auto &[key, entry] : artists)
        m_artists.append(QVariantMap{{QStringLiteral("name"), entry.first}, {QStringLiteral("count"), entry.second}});

    emit contentsChanged();
    applySearch();
    rebuildDetail();
    rebuildFolder();
}

void LocalLibraryController::setSearch(const QString &search)
{
    if (search == m_search)
        return;
    m_search = search;
    emit searchChanged();
    applySearch();
}

void LocalLibraryController::applySearch()
{
    const QStringList words = m_search.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (words.isEmpty()) {
        m_songs->setTracks(m_tracks);
        return;
    }
    std::vector<Track> found;
    for (const Track &t : m_tracks) {
        const QString text = QString::fromStdString(t.title + ' ' + t.artist + ' ' + t.album);
        if (std::all_of(words.begin(), words.end(),
                        [&](const QString &w) { return text.contains(w, Qt::CaseInsensitive); }))
            found.push_back(t);
    }
    m_songs->setTracks(std::move(found));
}

void LocalLibraryController::showAlbum(const QString &key)
{
    m_detailKind = 1;
    m_detailKey = key;
    rebuildDetail();
}

void LocalLibraryController::showArtist(const QString &name)
{
    m_detailKind = 2;
    m_detailKey = name;
    rebuildDetail();
}

void LocalLibraryController::closeDetail()
{
    m_detailKind = 0;
    m_detailKey.clear();
    rebuildDetail();
}

void LocalLibraryController::rebuildDetail()
{
    std::vector<Track> tracks;
    const std::string key = m_detailKey.toStdString();
    if (m_detailKind == 1) {
        for (const Track &t : m_tracks)
            if (albumKey(t) == key)
                tracks.push_back(t);
    } else if (m_detailKind == 2) {
        const std::string wanted = lower(key);
        for (const Track &t : m_tracks) {
            const auto names = artistNames(t.artist);
            if (std::any_of(names.begin(), names.end(), [&](const std::string &n) { return lower(n) == wanted; }))
                tracks.push_back(t);
        }
    }
    std::sort(tracks.begin(), tracks.end(), byAlbumOrder);

    m_detailTitle.clear();
    m_detailSubtitle.clear();
    m_detailCoverPath.clear();
    if (!tracks.empty()) {
        int64_t totalMs = 0;
        std::set<std::string> artists;
        for (const Track &t : tracks) {
            totalMs += t.durationMs;
            artists.insert(t.artist);
        }
        const QString songs = songsText(int(tracks.size()));
        const QString length = lengthText(totalMs);
        if (m_detailKind == 1) {
            m_detailTitle = QString::fromStdString(tracks.front().album);
            const QString artist = artists.size() == 1 ? QString::fromStdString(*artists.begin()) : tr("Various artists");
            m_detailSubtitle = artist + QStringLiteral(" · ") + songs + QStringLiteral(" · ") + length;
            m_detailCoverPath = QString::fromStdString(tracks.front().path);
        } else {
            m_detailTitle = m_detailKey;
            m_detailSubtitle = songs + QStringLiteral(" · ") + length;
        }
    } else if (m_detailKind != 0) {
        m_detailKind = 0; // the album / artist is gone (folder removed)
        m_detailKey.clear();
    }
    m_detail->setTracks(std::move(tracks));
    emit detailChanged();
}

std::vector<Track> LocalLibraryController::listFor(int list) const
{
    return list == DetailList ? m_detail->tracks() : list == FolderList ? m_folderSongs->tracks() : m_songs->tracks();
}

void LocalLibraryController::play(int list, int index)
{
    std::vector<Track> tracks = listFor(list);
    if (tracks.empty() || index >= int(tracks.size()))
        return;
    const QString title = list == DetailList ? m_detailTitle : list == FolderList ? folderTitle() : tr("All songs");
    m_queue->playTitled(std::move(tracks), std::max(index, 0), title, false);
}

void LocalLibraryController::addToQueue(int list, int index)
{
    std::vector<Track> tracks = listFor(list);
    if (index >= 0) {
        if (size_t(index) >= tracks.size())
            return;
        tracks = {tracks[size_t(index)]};
    }
    if (tracks.empty())
        return;
    const int count = int(tracks.size());
    m_queue->addAndPlay(std::move(tracks), false);
    emit notice(tr("Added %1 to the Queue").arg(songsText(count)));
}

QVariantList LocalLibraryController::items(int list, int index) const
{
    const std::vector<Track> tracks = listFor(list);
    QVariantList out;
    for (size_t i = 0; i < tracks.size(); ++i) {
        if (index < 0 || size_t(index) == i)
            out.append(QVariantMap{{QStringLiteral("path"), QString::fromStdString(tracks[i].path)}});
    }
    return out;
}

// ---- Folders tab

namespace {

QString cleanFolder(const QString &path)
{
    return QDir::cleanPath(path.startsWith(QStringLiteral("file:")) ? QUrl(path).toLocalFile() : path);
}

// Names as a file manager sorts them: "2 Foo" before "10 Bar", case ignored.
QCollator &natural()
{
    static QCollator collator = [] {
        QCollator c;
        c.setNumericMode(true);
        c.setCaseSensitivity(Qt::CaseInsensitive);
        return c;
    }();
    return collator;
}

} // namespace

QString LocalLibraryController::folderTitle() const
{
    return m_folder.isEmpty() ? tr("Folders") : QFileInfo(m_folder).fileName();
}

// From the music folder the open folder is in, down to it.
QVariantList LocalLibraryController::folderCrumbs() const
{
    QVariantList crumbs;
    if (m_folder.isEmpty())
        return crumbs;
    QString top;
    for (const QString &folder : m_settings->libraryFolders()) {
        const QString clean = cleanFolder(folder);
        if ((m_folder == clean || m_folder.startsWith(clean + QLatin1Char('/'))) && clean.size() > top.size())
            top = clean;
    }
    if (top.isEmpty())
        top = m_folder;
    crumbs.append(QVariantMap{{QStringLiteral("name"), QFileInfo(top).fileName()}, {QStringLiteral("path"), top}});
    QString path = top;
    for (const QString &part : m_folder.mid(top.size()).split(QLatin1Char('/'), Qt::SkipEmptyParts)) {
        path += QLatin1Char('/') + part;
        crumbs.append(QVariantMap{{QStringLiteral("name"), part}, {QStringLiteral("path"), path}});
    }
    return crumbs;
}

void LocalLibraryController::openFolder(const QString &path)
{
    m_folder = path.isEmpty() ? QString() : cleanFolder(path);
    rebuildFolder();
}

void LocalLibraryController::folderUp()
{
    if (m_folder.isEmpty())
        return;
    for (const QString &folder : m_settings->libraryFolders()) {
        if (cleanFolder(folder) == m_folder) {
            openFolder({}); // a music folder: back to the list of them
            return;
        }
    }
    openFolder(QFileInfo(m_folder).path());
}

std::vector<Track> LocalLibraryController::folderTracks(const QString &folder, bool withSubfolders) const
{
    std::vector<Track> out;
    if (folder.isEmpty())
        return out;
    const std::string prefix = (folder + QLatin1Char('/')).toStdString();
    for (const Track &t : m_tracks) {
        if (t.path.compare(0, prefix.size(), prefix) != 0)
            continue;
        if (withSubfolders || t.path.find('/', prefix.size()) == std::string::npos)
            out.push_back(t);
    }
    std::sort(out.begin(), out.end(), [](const Track &a, const Track &b) {
        return natural().compare(QString::fromStdString(a.path), QString::fromStdString(b.path)) < 0;
    });
    return out;
}

// What the open folder holds: its subfolders (with how many songs are in each,
// subfolders included) and its own songs. At the top, the music folders.
void LocalLibraryController::rebuildFolder()
{
    m_subfolders.clear();
    if (m_folder.isEmpty()) {
        for (const QString &folder : m_settings->libraryFolders()) {
            const QString clean = cleanFolder(folder);
            const std::string prefix = (clean + QLatin1Char('/')).toStdString();
            const int count = int(std::count_if(m_tracks.begin(), m_tracks.end(), [&](const Track &t) {
                return t.path.compare(0, prefix.size(), prefix) == 0;
            }));
            m_subfolders.append(QVariantMap{{QStringLiteral("name"), QFileInfo(clean).fileName()},
                                            {QStringLiteral("path"), clean},
                                            {QStringLiteral("count"), count},
                                            {QStringLiteral("location"), QFileInfo(clean).path()}});
        }
        m_folderSongs->setTracks({});
        emit folderChanged();
        return;
    }

    const std::string prefix = (m_folder + QLatin1Char('/')).toStdString();
    std::map<QString, int> counts; // subfolder name -> songs in it
    for (const Track &t : m_tracks) {
        if (t.path.compare(0, prefix.size(), prefix) != 0)
            continue;
        const size_t slash = t.path.find('/', prefix.size());
        if (slash != std::string::npos)
            ++counts[QString::fromStdString(t.path.substr(prefix.size(), slash - prefix.size()))];
    }
    QStringList names;
    for (const auto &entry : counts)
        names.append(entry.first);
    std::sort(names.begin(), names.end(), [](const QString &a, const QString &b) { return natural().compare(a, b) < 0; });
    for (const QString &name : names)
        m_subfolders.append(QVariantMap{{QStringLiteral("name"), name},
                                        {QStringLiteral("path"), m_folder + QLatin1Char('/') + name},
                                        {QStringLiteral("count"), counts[name]}});
    m_folderSongs->setTracks(folderTracks(m_folder, false));
    emit folderChanged();
}

void LocalLibraryController::playFolder()
{
    std::vector<Track> tracks = folderTracks(m_folder, true);
    if (!tracks.empty())
        m_queue->playTitled(std::move(tracks), 0, folderTitle(), false);
}

void LocalLibraryController::addFolderToQueue()
{
    std::vector<Track> tracks = folderTracks(m_folder, true);
    if (tracks.empty())
        return;
    const int added = m_queue->insertTracks(tracks, m_queue->count()); // in file order, at the end
    emit notice(tr("Added %1 to the Queue").arg(songsText(added)));
}

QVariantList LocalLibraryController::folderItems(const QString &path) const
{
    QVariantList out;
    for (const Track &t : folderTracks(cleanFolder(path), true))
        out.append(QVariantMap{{QStringLiteral("path"), QString::fromStdString(t.path)}});
    return out;
}

void LocalLibraryController::playAlbumOf(const QString &path)
{
    const Track *song = trackForPath(path);
    if (!song || song->album.empty())
        return;
    const std::string key = albumKey(*song);
    std::vector<Track> tracks;
    for (const Track &t : m_tracks)
        if (albumKey(t) == key)
            tracks.push_back(t);
    std::sort(tracks.begin(), tracks.end(), byAlbumOrder);
    m_queue->playTitled(std::move(tracks), 0, QString::fromStdString(song->album), false);
}

QVariantList LocalLibraryController::albumItems(const QString &key) const
{
    std::vector<Track> tracks;
    const std::string wanted = key.toStdString();
    for (const Track &t : m_tracks)
        if (albumKey(t) == wanted)
            tracks.push_back(t);
    std::sort(tracks.begin(), tracks.end(), byAlbumOrder);
    QVariantList out;
    for (const Track &t : tracks)
        out.append(QVariantMap{{QStringLiteral("path"), QString::fromStdString(t.path)}});
    return out;
}

QVariantList LocalLibraryController::drives() const
{
    QVariantList out;
    for (const QStorageInfo &volume : QStorageInfo::mountedVolumes()) {
        const QString root = volume.rootPath();
        const bool removableOrExtra = root.startsWith(QStringLiteral("/mnt/")) || root.startsWith(QStringLiteral("/media/"))
                                      || root.startsWith(QStringLiteral("/run/media/"));
        if (!volume.isValid() || !volume.isReady() || !removableOrExtra)
            continue;
        QString name = volume.displayName();
        if (name.isEmpty() || name == root)
            name = root.section(QLatin1Char('/'), -1);
        out.append(QVariantMap{{QStringLiteral("name"), name}, {QStringLiteral("url"), QUrl::fromLocalFile(root)}});
    }
    return out;
}
