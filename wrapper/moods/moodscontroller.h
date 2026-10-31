#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <memory>

class PlayerController;
class PlaylistController;
class PlaylistLibrary;
class QJSEngine;
class QQmlEngine;

// The six moods (`MoodsController` singleton): playlists of a kind, shown only
// on Home and filled from the player's heart button. They are stored like
// playlists, in a folder of their own, so they stay out of the playlist menus.
class MoodsController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QVariantList moods READ moods NOTIFY changed)           // [{ id, name, count }]
    Q_PROPERTY(QStringList currentMoods READ currentMoods NOTIFY currentChanged) // ids holding the song playing

public:
    MoodsController(PlaylistController *queue, PlayerController *player, QObject *parent);
    ~MoodsController() override;

    static MoodsController *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    QVariantList moods() const;
    QStringList currentMoods() const;

    Q_INVOKABLE void toggleCurrent(const QString &id);       // the song playing, in or out of the mood
    Q_INVOKABLE QVariantList songs(const QString &id) const; // as HistoryController's songs
    Q_INVOKABLE void play(const QString &id, int index);
    Q_INVOKABLE void removeSong(const QString &id, int index);

signals:
    void changed();
    void currentChanged();

private:
    static inline MoodsController *s_instance = nullptr;

    PlaylistController *const m_queue;
    std::unique_ptr<PlaylistLibrary> m_library;
    QStringList m_ids; // in the order they show
};
