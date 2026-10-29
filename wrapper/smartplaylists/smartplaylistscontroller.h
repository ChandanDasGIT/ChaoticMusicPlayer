#pragma once

#include "backend/playlist/smartplaylist/smartplaylist.h"

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <memory>

class HistoryController;
class LocalLibraryController;
class PlaylistController;
class QJSEngine;
class QQmlEngine;

// Smart playlists for QML (`SmartPlaylistsController` singleton): lists that
// fill themselves from rules, over the library and the YouTube songs in the
// listening history (play counts and last plays come from the history). They
// are kept up to date as the history and the library change, and play like
// any list (never touching the Queue).
//
// A playlist as QML sees it: { id, name, matchAll, rules: [{ field, op, value }],
// order, limit, count } (see SmartRule / SmartPlaylist for the values).
class SmartPlaylistsController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QVariantList playlists READ playlists NOTIFY changed)

public:
    SmartPlaylistsController(HistoryController *history, LocalLibraryController *library, PlaylistController *queue,
                             QObject *parent);
    ~SmartPlaylistsController() override;

    static SmartPlaylistsController *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    QVariantList playlists() const { return m_list; }

    Q_INVOKABLE void play(const QString &id);
    Q_INVOKABLE QString save(const QVariantMap &playlist); // new (no id) or changed; returns its id
    Q_INVOKABLE void remove(const QString &id);
    Q_INVOKABLE int preview(const QVariantMap &playlist) const; // how many songs it would have
    Q_INVOKABLE QVariantList items(const QString &id) const;    // for dragging onto the Queue

signals:
    void changed();

private:
    void refresh();
    std::vector<SmartCandidate> candidates() const;
    static SmartPlaylist fromMap(const QVariantMap &map);
    static QVariantMap toMap(const SmartPlaylist &playlist);

    static inline SmartPlaylistsController *s_instance = nullptr;

    HistoryController *const m_history;
    LocalLibraryController *const m_library;
    PlaylistController *const m_queue;
    std::unique_ptr<SmartPlaylistStore> m_store;
    QVariantList m_list;
};
