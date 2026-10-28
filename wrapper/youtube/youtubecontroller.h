#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QThreadPool>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <memory>

class LinkLibrary;
struct Track;
class PlaylistController;
class QJSEngine;
class QQmlEngine;
class SettingsController;

// Everything the YouTube Music panel shows and does (`YouTubeController`
// singleton). Songs are passed to QML as maps:
//   { videoId, title, artists, album, duration (ms), thumbnail }
// Network / yt-dlp work runs on worker threads; results arrive as property
// changes. `notice(text)` carries short messages for the panel's toast.
class YouTubeController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool available READ available NOTIFY availabilityChanged) // yt-dlp installed
    Q_PROPERTY(QString ytDlpVersion READ ytDlpVersion NOTIFY availabilityChanged)
    Q_PROPERTY(bool signedIn READ signedIn NOTIFY signedInChanged)

    Q_PROPERTY(QVariantList searchResults READ searchResults NOTIFY searchChanged)
    // Suggested searches for the text being typed (see suggest()).
    Q_PROPERTY(QStringList suggestions READ suggestions NOTIFY suggestionsChanged)
    Q_PROPERTY(bool searching READ searching NOTIFY searchChanged)
    Q_PROPERTY(QString searchError READ searchError NOTIFY searchChanged)

    // [{ title, items: [{ browseId, title, subtitle, thumbnail }] }]
    Q_PROPERTY(QVariantList homeShelves READ homeShelves NOTIFY homeChanged)
    Q_PROPERTY(bool homeLoading READ homeLoading NOTIFY homeChanged)
    Q_PROPERTY(QString homeError READ homeError NOTIFY homeChanged)

    Q_PROPERTY(bool collectionOpen READ collectionOpen NOTIFY collectionChanged)
    Q_PROPERTY(QString collectionTitle READ collectionTitle NOTIFY collectionChanged)
    Q_PROPERTY(QVariantList collectionSongs READ collectionSongs NOTIFY collectionChanged)
    Q_PROPERTY(bool collectionLoading READ collectionLoading NOTIFY collectionChanged)
    Q_PROPERTY(QString collectionError READ collectionError NOTIFY collectionChanged)

    Q_PROPERTY(QVariantList librarySongs READ librarySongs NOTIFY libraryChanged)
    Q_PROPERTY(bool libraryLoading READ libraryLoading NOTIFY libraryChanged)
    Q_PROPERTY(QString libraryError READ libraryError NOTIFY libraryChanged)

    Q_PROPERTY(QVariantList savedSongs READ savedSongs NOTIFY savedChanged)
    Q_PROPERTY(bool addingLink READ addingLink NOTIFY savedChanged)

public:
    YouTubeController(PlaylistController *playlist, SettingsController *settings, QObject *parent);
    ~YouTubeController() override;

    static YouTubeController *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    // A song map (see above) as a playlist track ("ytmusic:<id>").
    static ::Track trackFromSong(const QVariantMap &song);

    bool available() const { return m_available; }
    QString ytDlpVersion() const { return m_ytDlpVersion; }
    bool signedIn() const;

    QVariantList searchResults() const { return m_searchResults; }
    QStringList suggestions() const { return m_suggestions; }
    bool searching() const { return m_searching; }
    QString searchError() const { return m_searchError; }
    QVariantList homeShelves() const { return m_homeShelves; }
    bool homeLoading() const { return m_homeLoading; }
    QString homeError() const { return m_homeError; }
    bool collectionOpen() const { return m_collectionOpen; }
    QString collectionTitle() const { return m_collectionTitle; }
    QVariantList collectionSongs() const { return m_collectionSongs; }
    bool collectionLoading() const { return m_collectionLoading; }
    QString collectionError() const { return m_collectionError; }
    QVariantList librarySongs() const { return m_librarySongs; }
    bool libraryLoading() const { return m_libraryLoading; }
    QString libraryError() const { return m_libraryError; }
    QVariantList savedSongs() const;
    bool addingLink() const { return m_addingLink; }

    Q_INVOKABLE void search(const QString &query);
    // Asks for suggested searches; an answer to an older call is dropped. "" clears them.
    Q_INVOKABLE void suggest(const QString &input);
    Q_INVOKABLE void loadHome();
    Q_INVOKABLE void openCollection(const QString &browseId, const QString &title);
    Q_INVOKABLE void closeCollection();
    Q_INVOKABLE void loadLibrary();

    Q_INVOKABLE void play(const QVariantMap &song);       // add to the playlist and play now
    Q_INVOKABLE void enqueue(const QVariantMap &song);    // add to the playlist
    Q_INVOKABLE void save(const QVariantMap &song);       // add to Saved
    Q_INVOKABLE void playAll(const QVariantList &songs);  // add all, play the first
    Q_INVOKABLE void addLink(const QString &url);         // video or playlist link -> Saved
    Q_INVOKABLE void removeSaved(int index);

signals:
    void availabilityChanged();
    void signedInChanged();
    void searchChanged();
    void suggestionsChanged();
    void homeChanged();
    void collectionChanged();
    void libraryChanged();
    void savedChanged();
    void notice(const QString &text);

private:
    template<typename Work, typename Done>
    void runAsync(Work work, Done done);

    static inline YouTubeController *s_instance = nullptr;

    PlaylistController *m_playlist;
    SettingsController *m_settings;
    std::unique_ptr<LinkLibrary> m_links;

    bool m_available = false;
    QString m_ytDlpVersion;

    QVariantList m_searchResults;
    bool m_searching = false;
    QString m_searchError;
    int m_searchGeneration = 0;
    QStringList m_suggestions;
    int m_suggestGeneration = 0;

    QVariantList m_homeShelves;
    bool m_homeLoading = false;
    QString m_homeError;

    bool m_collectionOpen = false;
    QString m_collectionTitle;
    QVariantList m_collectionSongs;
    bool m_collectionLoading = false;
    QString m_collectionError;
    int m_collectionGeneration = 0;

    QVariantList m_librarySongs;
    bool m_libraryLoading = false;
    QString m_libraryError;

    bool m_addingLink = false;

    QThreadPool m_pool;
};
