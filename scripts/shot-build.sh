#!/bin/bash
# Builds the screenshot harness (build/shot/shot) from the CURRENT main.cpp + the
# app's object files (build the app first). Run it with scripts/shot-run.sh.
# Env: SHOT_LYRICS=1 opens the lyrics view. Writes build/shot/{playing,paused,settings}.png
set -e
cd "$(dirname "$0")/.."
mkdir -p build/shot
python3 -I - <<'PY'
import pathlib
s = pathlib.Path("main.cpp").read_text()
s = "#include \"backend/library/metadata/metadata.h\"\n#include <QDir>\n#include <QTest>\n#include <cstdio>\n#include <QQuickWindow>\n#include <QTimer>\n#include <QImage>\n#include <functional>\n#include <QMouseEvent>\n#include <QQuickItem>\n#include <QUrl>\n#include <QMimeData>\n#include <QDropEvent>\n#include <QWheelEvent>\n" + s
hook = '''    // ---- screenshot harness
    if (qEnvironmentVariableIsSet("SHOT_SIZE")) { // e.g. 400x780
        const QStringList wh = qEnvironmentVariable("SHOT_SIZE").split(QLatin1Char('x'));
        auto *w = qobject_cast<QWindow *>(engine.rootObjects().constFirst());
        w->resize(wh.value(0).toInt(), wh.value(1).toInt());
    }
    if (qEnvironmentVariableIsSet("SHOT_MUTE"))
        player.setVolume(0);
    QTimer::singleShot(800, [&] {
        if (qEnvironmentVariableIsSet("SHOT_PLAY_YT")) // play a YouTube song by id
        {
            QVariantMap song;
            song.insert(QStringLiteral("videoId"), qEnvironmentVariable("SHOT_PLAY_YT"));
            song.insert(QStringLiteral("title"), QStringLiteral("Gulabi Ankhen (From The Train)"));
            song.insert(QStringLiteral("artists"), QStringLiteral("Mohammed Rafi, R. D. Burman"));
            song.insert(QStringLiteral("album"), QStringLiteral("Audiobiography"));
            song.insert(QStringLiteral("duration"), qint64(198000));
            song.insert(QStringLiteral("thumbnail"), qEnvironmentVariable("SHOT_THUMB"));
            youtube.play(song);
        }
        else if (!qEnvironmentVariableIsSet("SHOT_NO_PLAY")) // (SHOT_NO_PLAY: e.g. to see a resumed session)
            playlist.playIndex(qEnvironmentVariableIntValue("SHOT_PLAY_AT"));
        if (qEnvironmentVariableIsSet("SHOT_SEEK")) // ms, a moment after starting
            QTimer::singleShot(600, [&] { player.seek(qEnvironmentVariableIntValue("SHOT_SEEK")); });
        if (qEnvironmentVariableIsSet("SHOT_STATE")) // print what is loaded, now and later
            for (int t : {0, 1500})
                QTimer::singleShot(t, [&, t] {
                    std::printf("state +%dms: source=%s pos=%lld playing=%d title=%s fromList=%d\\n", t,
                                qPrintable(QFileInfo(player.source()).fileName()), qint64(player.position()),
                                int(player.playing()), qPrintable(playlist.currentTitle()), int(playlist.playingFromList()));
                    std::fflush(stdout);
                });
        if (qEnvironmentVariableIsSet("SHOT_MIX")) { // a pretend Mix of the files in that folder (no YouTube):
            // starts with the first; each "more, please" gets the next file
            static std::vector<::Track> pool;
            for (const QString &f : QDir(qEnvironmentVariable("SHOT_MIX")).entryList({QStringLiteral("*.flac")}, QDir::Files, QDir::Name))
                pool.push_back(Metadata::read((qEnvironmentVariable("SHOT_MIX") + QLatin1Char('/') + f).toStdString()));
            QObject::connect(&playlist, &PlaylistController::moreWanted, &playlist, [&playlist](const QString &seed, bool forQueue) {
                std::printf("moreWanted seed=%s forQueue=%d (pool %zu)\\n", qPrintable(QFileInfo(seed).baseName()), int(forQueue), pool.size());
                if (pool.empty())
                    return;
                std::vector<::Track> more = {pool.front()};
                pool.erase(pool.begin());
                QTimer::singleShot(300, &playlist, [&playlist, more] { playlist.extendList(more); });
            });
            std::vector<::Track> first = {pool.front()};
            pool.erase(pool.begin());
            playlist.playTitled(first, 0, QStringLiteral("Mix – test"), true);
        }
        if (qEnvironmentVariableIsSet("SHOT_LEVELS")) // "fromMs,toMs": every 0.2 s, the loudest band and where playback is
            for (int t = qEnvironmentVariable("SHOT_LEVELS").section(QLatin1Char(','), 0, 0).toInt();
                 t <= qEnvironmentVariable("SHOT_LEVELS").section(QLatin1Char(','), 1, 1).toInt(); t += 200)
                QTimer::singleShot(t, [&, t] {
                    qreal loudest = 0;
                    for (qreal band : visualizer.bands())
                        loudest = std::max(loudest, band);
                    std::printf("lvl t=%5d %-8s pos=%6lld level=%.2f %s\\n", t, qPrintable(QFileInfo(player.source()).baseName()),
                                qint64(player.position()), loudest, QString(int(loudest * 40), QLatin1Char('#')).toLatin1().constData());
                });
        if (qEnvironmentVariableIsSet("SHOT_RG")) // ReplayGain mode from the start
            settings.setReplayGain(qEnvironmentVariableIntValue("SHOT_RG"));
        if (qEnvironmentVariableIsSet("SHOT_LIB_DIR")) // a Library folder
            localLibrary.addFolder(qEnvironmentVariable("SHOT_LIB_DIR"));
        if (qEnvironmentVariableIsSet("SHOT_ADD_DIR")) // like dropping a folder
            library.addUrls({QUrl::fromLocalFile(qEnvironmentVariable("SHOT_ADD_DIR"))});
        if (qEnvironmentVariableIsSet("SHOT_VIEW")) {
            playlists.view(qEnvironmentVariable("SHOT_VIEW"));
            if (qEnvironmentVariableIsSet("SHOT_PLAY_VIEWED"))
                playlists.playViewed(qEnvironmentVariableIntValue("SHOT_PLAY_VIEWED"));
            if (qEnvironmentVariableIsSet("SHOT_BACK_TO_QUEUE"))
                playlists.view(QString());
        }
        if (qEnvironmentVariableIsSet("SHOT_YT_SEARCH")) {
            if (qEnvironmentVariableIsSet("SHOT_PLAY_FIRST")) { // play the first result once it arrives
                QObject::connect(&youtube, &YouTubeController::searchChanged, &youtube, [&youtube] {
                    static bool done = false;
                    if (!done && !youtube.searching() && !youtube.searchResults().isEmpty()) {
                        done = true;
                        youtube.play(youtube.searchResults().constFirst().toMap());
                    }
                });
            }
            youtube.search(qEnvironmentVariable("SHOT_YT_SEARCH"));
        }
    });
    QTimer::singleShot(1300, [&] {
        if (QObject *panel = engine.rootObjects().constFirst()->findChild<QObject *>("nowPlaying"))
            panel->setProperty("lyricsOpen", qEnvironmentVariableIsSet("SHOT_LYRICS"));
        if (qEnvironmentVariableIsSet("SHOT_LYRICS"))
            player.seek(9200);
        if (qEnvironmentVariableIsSet("SHOT_PLAYLIST"))
            if (QObject *drawer = engine.rootObjects().constFirst()->findChild<QObject *>("playlistDrawer"))
                QMetaObject::invokeMethod(drawer, "open");
        if (qEnvironmentVariableIsSet("SHOT_YT")) {
            QObject *root = engine.rootObjects().constFirst();
            if (QObject *panel = root->findChild<QObject *>("youtubePanel"))
                panel->setProperty("tab", qEnvironmentVariableIntValue("SHOT_YT"));
            if (QObject *drawer = root->findChild<QObject *>("youtubeDrawer"))
                QMetaObject::invokeMethod(drawer, "open");
        }
    });
    if (qEnvironmentVariableIsSet("SHOT_OPEN_MENU"))
        QTimer::singleShot(2500, [&] {
            // Every copy (docked panel + drawer): only the visible one shows up.
            for (QObject *menu : engine.rootObjects().constFirst()->findChildren<QObject *>(qEnvironmentVariable("SHOT_OPEN_MENU")))
                QMetaObject::invokeMethod(menu, "popup", Q_ARG(double, 120.0), Q_ARG(double, 40.0));
        });
    if (qEnvironmentVariableIsSet("SHOT_NEW_PLAYLIST")) { // ask for a name, then answer it
        QTimer::singleShot(2500, [&] {
            QVariantMap song;
            song.insert(QStringLiteral("videoId"), QStringLiteral("nWbBIf5_LTY"));
            song.insert(QStringLiteral("title"), QStringLiteral("Gulabi Ankhen (other version)"));
            song.insert(QStringLiteral("artists"), QStringLiteral("Mohammed Rafi"));
            playlists.requestNewPlaylist(QVariantList{song});
        });
        QTimer::singleShot(4200, [&] { playlists.submitName(QStringLiteral("Test mix")); });
    }
    // Visual-tree walk (list delegates are not in the QObject tree).
    const auto findItems = [&](const char *typePrefix) {
        QList<QQuickItem *> found;
        std::function<void(QQuickItem *)> walk = [&](QQuickItem *item) {
            if (QByteArray(item->metaObject()->className()).startsWith(typePrefix))
                found << item;
            for (QQuickItem *child : item->childItems())
                walk(child);
        };
        walk(qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst())->contentItem());
        return found;
    };
    if (qEnvironmentVariableIsSet("SHOT_SWIPE_ROW")) // reveal Delete on that row
        QTimer::singleShot(2000, [&, findItems] {
            for (QQuickItem *row : findItems("PlaylistRow"))
                if (row->isVisible() && row->property("index").toInt() == qEnvironmentVariableIntValue("SHOT_SWIPE_ROW"))
                    row->setProperty("swipeX", -96.0);
        });
    if (qEnvironmentVariableIsSet("SHOT_DRAG_ROW")) { // hold a row 0.6 s, then drag it down by SHOT_DRAG_DY pixels (real input path)
        static QPoint holdPos;
        const auto window = [&] { return qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst()); };
        QTimer::singleShot(2000, [&, findItems, window] {
            for (QQuickItem *row : findItems("PlaylistRow"))
                if (row->isVisible() && row->property("index").toInt() == qEnvironmentVariableIntValue("SHOT_DRAG_ROW"))
                    holdPos = row->mapToScene(QPointF(150, row->height() / 2)).toPoint(); // on the title
            std::printf("hold at %d,%d (queue has %d)\\n", holdPos.x(), holdPos.y(), playlist.count());
            QTest::mouseMove(window(), holdPos);
        });
        QTimer::singleShot(2100, [window] { QTest::mousePress(window(), Qt::LeftButton, {}, holdPos); });
        const int dy = qEnvironmentVariableIntValue("SHOT_DRAG_DY");
        for (int i = 1; i <= 10; ++i)
            QTimer::singleShot(2700 + i * 50, [window, i, dy] { QTest::mouseMove(window(), holdPos + QPoint(0, dy * i / 10)); });
        QTimer::singleShot(3500, [window, dy] { QTest::mouseRelease(window(), Qt::LeftButton, {}, holdPos + QPoint(0, dy)); });
        for (int t : {3400, 3700, 4200})
            QTimer::singleShot(t, [&, t] {
                std::printf("t=%d order:", t);
                for (int i = 0; i < playlist.count(); ++i)
                    std::printf(" %s", qPrintable(QFileInfo(playlist.pathAt(i)).baseName()));
                std::printf("\\n");
                std::fflush(stdout);
            });
        for (int t : {2400, 2650, 2900, 3300})
            QTimer::singleShot(t, [&, findItems, t] {
                for (QQuickItem *row : findItems("PlaylistRow"))
                    if (row->isVisible() && row->property("reordering").toBool())
                        std::printf("t=%d reordering row %d dragOffset=%.0f\\n", t, row->property("index").toInt(),
                                    row->property("dragOffset").toDouble());
                std::printf("t=%d probe\\n", t);
                std::fflush(stdout);
            });
    }
    if (qEnvironmentVariableIsSet("SHOT_MORE_ROW")) { // hover a row, click its "⋮", then move onto the menu (real input path)
        static QPoint morePos;
        const auto window = [&] { return qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst()); };
        QTimer::singleShot(2000, [&, findItems, window] {
            for (QQuickItem *row : findItems("PlaylistRow"))
                if (row->isVisible() && row->property("index").toInt() == qEnvironmentVariableIntValue("SHOT_MORE_ROW"))
                    morePos = row->mapToScene(QPointF(row->width() - 6 - 2 - 13, row->height() / 2)).toPoint();
            QTest::mouseMove(window(), morePos);
        });
        QTimer::singleShot(2300, [window] { QTest::mouseClick(window(), Qt::LeftButton, {}, morePos); });
        QTimer::singleShot(2700, [window] { QTest::mouseMove(window(), morePos + QPoint(-60, 70)); });
        QTimer::singleShot(3000, [&, findItems] {
            for (QQuickItem *row : findItems("PlaylistRow"))
                if (row->isVisible() && row->property("index").toInt() == qEnvironmentVariableIntValue("SHOT_MORE_ROW"))
                    std::printf("after moving onto the menu: row selected=%d menuOpen=%d\\n", row->property("selected").toBool(),
                                row->property("menuOpen").toBool());
            std::fflush(stdout);
        });
    }
    if (qEnvironmentVariableIsSet("SHOT_CLICK_ICON")) { // hover + click the first visible PillButton with that icon
        static QPointF clickPos;
        const auto send = [&](QEvent::Type type, QPointF p, Qt::MouseButtons buttons) {
            auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
            QMouseEvent e(type, p, p, window->mapToGlobal(p), type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton,
                          buttons, Qt::NoModifier);
            QCoreApplication::sendEvent(window, &e);
        };
        QTimer::singleShot(2000, [&, send] {
            std::function<void(QQuickItem *)> walk = [&](QQuickItem *item) {
                if (!item->isVisible() || !clickPos.isNull())
                    return;
                if (item->property("iconName").toString() == qEnvironmentVariable("SHOT_CLICK_ICON")
                    && !QByteArray(item->metaObject()->className()).startsWith("Icon"))
                    clickPos = item->mapToScene(QPointF(item->width() / 2, item->height() / 2));
                for (QQuickItem *c : item->childItems())
                    walk(c);
            };
            walk(qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst())->contentItem());
            std::printf("click icon at %.0f,%.0f pinned=%d\\n", clickPos.x(), clickPos.y(), int(settings.playlistPinned()));
            send(QEvent::MouseMove, clickPos, Qt::NoButton);
        });
        QTimer::singleShot(2300, [send] { send(QEvent::MouseButtonPress, clickPos, Qt::LeftButton); });
        QTimer::singleShot(2350, [send] { send(QEvent::MouseButtonRelease, clickPos, Qt::NoButton); });
        QTimer::singleShot(2600, [&] { std::printf("after click pinned=%d\\n", int(settings.playlistPinned())); });
    }
    if (qEnvironmentVariableIsSet("SHOT_SCREEN_W")) // pretend the screen is this wide (picks the layout)
        engine.rootObjects().constFirst()->setProperty("screenWidthOverride", qEnvironmentVariableIntValue("SHOT_SCREEN_W"));
    if (qEnvironmentVariableIsSet("SHOT_OPEN")) { // steps in turn: queue / youtube / library (toggle), pin:<name>, player (mini tap)
        const QStringList steps = qEnvironmentVariable("SHOT_OPEN").split(QLatin1Char(' '), Qt::SkipEmptyParts);
        const auto columns = [&, findItems](const QString &label) {
            QObject *root = engine.rootObjects().constFirst();
            std::printf("%-16s open=%s pinR=%s layout=%s queue=%d right=%s player=%d |", qPrintable(label), qPrintable(root->property("rightOpen").toString()), qPrintable(root->property("rightPinned").toString()),
                        qPrintable(root->property("layout").toString()), root->property("playlistDocked").toInt(),
                        qPrintable(root->property("rightShown").toString()), root->property("playerShown").toInt());
            for (const char *type : {"PlaylistPanel", "NowPlayingPanel", "YouTubePanel", "LibraryPanel", "MiniPlayer"})
                for (QQuickItem *item : findItems(type))
                    if (item->isVisible() && item->width() > 0 && item->height() > 0)
                        std::printf(" %s@%.0f+%.0f", type, item->mapToScene({0, 0}).x(), item->width());
            std::printf("\\n");
            std::fflush(stdout);
        };
        QTimer::singleShot(1800, [columns] { columns(QStringLiteral("start")); });
        for (int i = 0; i < steps.size(); ++i)
            QTimer::singleShot(2000 + i * 1300, [&, steps, i, columns, findItems] {
                QObject *root = engine.rootObjects().constFirst();
                const QString step = steps[i];
                if (step.startsWith(QLatin1String("click:"))) { // click that panel's pin button like a user
                    auto *w = qobject_cast<QQuickWindow *>(root);
                    const QString icon = step.section(QLatin1Char(':'), 2, 2).isEmpty() ? QStringLiteral("thumbtack") : step.section(QLatin1Char(':'), 2, 2);
                    for (QQuickItem *panel : findItems(qPrintable(step.section(QLatin1Char(':'), 1, 1))))
                        if (panel->isVisible())
                            for (QQuickItem *button : panel->findChildren<QQuickItem *>())
                                if (button->isVisible() && button->property("iconName").toString() == icon) {
                                    const QPoint p = button->mapToScene({button->width() / 2, button->height() / 2}).toPoint();
                                    std::printf("click pin at %d,%d\\n", p.x(), p.y());
                                    std::function<void(QQuickItem *, int)> hits = [&](QQuickItem *item, int depth) {
                                        if (!item->isVisible() || !item->contains(item->mapFromScene(p)))
                                            return;
                                        std::printf("  %*s%s %s z=%.0f\\n", depth, "", item->metaObject()->className(), qPrintable(item->objectName()), item->z());
                                        for (QQuickItem *child : item->childItems())
                                            hits(child, depth + 1);
                                    };
                                    if (qEnvironmentVariableIsSet("SHOT_HITS"))
                                        hits(w->contentItem(), 0);
                                    QTest::mouseMove(w, p);
                                    QTest::qWait(400); // hover like a person (tooltips show up)
                                    QTest::mousePress(w, Qt::LeftButton, {}, p);
                                    if (QQuickItem *g = w->mouseGrabberItem())
                                        std::printf("  grabber: %s %s\\n", g->metaObject()->className(), qPrintable(g->objectName()));
                                    else
                                        std::printf("  grabber: none\\n");
                                    QTest::mouseRelease(w, Qt::LeftButton, {}, p);
                                }
                } else if (step.startsWith(QLatin1String("pin:")))
                    QMetaObject::invokeMethod(root, "togglePin", Q_ARG(QVariant, step.mid(4)));
                else if (step.startsWith(QLatin1String("at:"))) { // press / release at x,y; print who grabbed it
                    auto *w = qobject_cast<QQuickWindow *>(root);
                    const QPoint p(step.mid(3).section(QLatin1Char(','), 0, 0).toInt(), step.mid(3).section(QLatin1Char(','), 1, 1).toInt());
                    QTest::mouseMove(w, p);
                    QTest::qWait(qEnvironmentVariableIsSet("SHOT_HOVER_MS") ? qEnvironmentVariableIntValue("SHOT_HOVER_MS") : 400); // hover like a person (tooltips show up)
                    if (qEnvironmentVariableIsSet("SHOT_HOVER_MS"))
                        w->grabWindow().save(QStringLiteral("build/shot/hover.png"));
                    QTest::mousePress(w, Qt::LeftButton, {}, p);
                    QQuickItem *g = w->mouseGrabberItem();
                    std::printf("at %d,%d grabber: %s\\n", p.x(), p.y(), g ? g->metaObject()->className() : "none");
                    if (qEnvironmentVariableIsSet("SHOT_CHAIN") && g)
                        for (QQuickItem *it = g; it; it = it->parentItem())
                            std::printf("  chain %s x=%.1f y=%.1f scale=%.2f transforms=%d sceneY=%.1f\\n", it->metaObject()->className(), it->x(), it->y(), it->scale(),
                                        int(it->property("transform").isValid()), it->mapToScene({0, 0}).y());
                    std::function<void(QQuickItem *, int)> dump = [&](QQuickItem *item, int depth) {
                        if (depth > 2 || !item->isVisible())
                            return;
                        const QRectF r = item->mapRectToScene(item->boundingRect());
                        std::printf("  %*s%s %s %.0f,%.0f %.0fx%.0f op=%.2f\\n", depth, "", item->metaObject()->className(), qPrintable(item->objectName()), r.x(), r.y(), r.width(), r.height(), item->opacity());
                        for (QQuickItem *child : item->childItems())
                            dump(child, depth + 1);
                    };
                    if (g && g->inherits("QQuickOverlay"))
                        dump(g, 0);
                    for (QQuickItem *overlayItem : w->contentItem()->childItems())
                        if (overlayItem->inherits("QQuickOverlay"))
                        for (QQuickItem *child : overlayItem->childItems())
                            if (QObject *popup = child->parent(); popup && popup->inherits("QQuickToolTip")) {
                                auto *pp = popup->property("parent").value<QQuickItem *>();
                                std::printf("  tooltip item x=%.0f y=%.0f h=%.0f parentH=%.0f roomAbove=%d y=%.0f implicitHeight=%.0f margins=%.0f parent=%s sceneY=%.0f\\n", child->x(), child->y(), child->height(), popup->property("parent").value<QQuickItem *>() ? popup->property("parent").value<QQuickItem *>()->height() : -1.0, popup->property("roomAbove").toInt(), popup->property("y").toDouble(),
                                            popup->property("implicitHeight").toDouble(), popup->property("margins").toDouble(), pp ? pp->metaObject()->className() : "-", pp ? pp->mapToScene({0, 0}).y() : -1.0);
                            }
                    for (QObject *o : root->findChildren<QObject *>())
                        if (o->inherits("QQuickPopup") && o->property("visible").toBool())
                            std::printf("  open popup: %s %s x=%.0f y=%.0f w=%.0f h=%.0f parent=%s\\n", o->metaObject()->className(), qPrintable(o->objectName()),
                                        o->property("x").toDouble(), o->property("y").toDouble(), o->property("width").toDouble(), o->property("height").toDouble(),
                                        o->parent() ? o->parent()->metaObject()->className() : "-");
                    QTest::mouseRelease(w, Qt::LeftButton, {}, p);
                }
                else if (step.startsWith(QLatin1String("drag:"))) { // drag:x,y,dx — press, move sideways in steps, release
                    auto *w = qobject_cast<QQuickWindow *>(root);
                    const QStringList v = step.mid(5).split(QLatin1Char(','));
                    const QPoint from(v.value(0).toInt(), v.value(1).toInt());
                    const int dx = v.value(2).toInt();
                    QTest::mouseMove(w, from);
                    QTest::mousePress(w, Qt::LeftButton, {}, from);
                    for (int k = 1; k <= 12; ++k) {
                        QTest::qWait(16);
                        QTest::mouseMove(w, from + QPoint(dx * k / 12, 0));
                    }
                    QTest::qWait(150); // slow release: no fling
                    QTest::mouseRelease(w, Qt::LeftButton, {}, from + QPoint(dx, 0));
                }
                else if (step.startsWith(QLatin1String("hold:"))) { // hold:x,y,dx,dy — long press, drag in steps, release
                    auto *w = qobject_cast<QQuickWindow *>(root);
                    const QStringList v = step.mid(5).split(QLatin1Char(','));
                    const QPoint from(v.value(0).toInt(), v.value(1).toInt());
                    const QPoint by(v.value(2).toInt(), v.value(3).toInt());
                    QTest::mouseMove(w, from);
                    QTest::mousePress(w, Qt::LeftButton, {}, from);
                    QTest::qWait(600);
                    for (int k = 1; k <= 15; ++k) {
                        QTest::mouseMove(w, from + by * k / 15);
                        QTest::qWait(20);
                    }
                    QTest::qWait(qEnvironmentVariableIsSet("SHOT_HOLD_SHOT") ? 300 : 100);
                    if (qEnvironmentVariableIsSet("SHOT_HOLD_SHOT")) // the screen while still dragging
                        w->grabWindow().save(QStringLiteral("build/shot/hold.png"));
                    QTest::mouseRelease(w, Qt::LeftButton, {}, from + by);
                } else if (step.startsWith(QLatin1String("type:"))) { // type that text where the focus is
                    const QString text = step.mid(5).replace(QLatin1Char('_'), QLatin1Char(' '));
                    for (const QChar c : text) {
                        const int key = c.isLetter() ? Qt::Key_A + (c.toUpper().unicode() - 'A') : c.unicode();
                        QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier, QString(c));
                        QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier, QString(c));
                        QCoreApplication::sendEvent(qobject_cast<QQuickWindow *>(root), &press);
                        QCoreApplication::sendEvent(qobject_cast<QQuickWindow *>(root), &release);
                    }
                } else if (step.startsWith(QLatin1String("key:"))) { // press a key, e.g. key:Return
                    QTest::keySequence(qobject_cast<QQuickWindow *>(root), QKeySequence(step.mid(4)));
                } else if (step == QLatin1String("printplays")) { // the listening history
                    std::printf("plays: %d\\n", history.count());
                    for (const auto &play : history.plays())
                        std::printf("  play: %s | %s | from %s | %llds\\n", play.title.c_str(), play.artist.c_str(),
                                    play.from.c_str(), (long long)(play.durationMs / 1000));
                } else if (step == QLatin1String("printlib")) { // the Library's albums and artists
                    for (const QVariant &a : localLibrary.albums()) {
                        const QVariantMap m = a.toMap();
                        std::printf("album: %s | %s | %d songs\\n", qPrintable(m.value(QStringLiteral("album")).toString()),
                                    qPrintable(m.value(QStringLiteral("artist")).toString()), m.value(QStringLiteral("count")).toInt());
                    }
                    for (const QVariant &a : localLibrary.recentlyAdded()) {
                        const QVariantMap m = a.toMap();
                        std::printf("recently added: %s (added %lld)\\n", qPrintable(m.value(QStringLiteral("album")).toString()),
                                    (long long)m.value(QStringLiteral("addedAt")).toLongLong());
                    }
                    for (const QVariant &a : localLibrary.artists()) {
                        const QVariantMap m = a.toMap();
                        std::printf("artist: %s | %d songs\\n", qPrintable(m.value(QStringLiteral("name")).toString()),
                                    m.value(QStringLiteral("count")).toInt());
                    }
                } else if (step == QLatin1String("printsmart")) { // the smart playlists
                    for (const QVariant &v : smartPlaylists.playlists()) {
                        const QVariantMap m = v.toMap();
                        std::printf("smart: %s | %d songs | %d rules\\n", qPrintable(m.value(QStringLiteral("name")).toString()),
                                    m.value(QStringLiteral("count")).toInt(), int(m.value(QStringLiteral("rules")).toList().size()));
                    }
                } else if (step == QLatin1String("edittags")) { // Track details of the song playing, ready to edit
                    if (QObject *dialog = root->findChild<QObject *>("detailsDialog"))
                        QMetaObject::invokeMethod(dialog, "show", Q_ARG(QVariant, QVariant(player.source())), Q_ARG(QVariant, QVariant(true)));
                } else if (step == QLatin1String("printsong")) { // what the Queue / library think the playing song is
                    std::printf("song: queue title=\\"%s\\" library title=\\"%s\\"\\n", qPrintable(playlist.currentTitle()),
                                localLibrary.trackForPath(player.source()) ? localLibrary.trackForPath(player.source())->title.c_str() : "-");
                } else if (step.startsWith(QLatin1String("sleep:"))) { // sleep:<minutes> or sleep:end
                    if (step.mid(6) == QLatin1String("end"))
                        sleepTimer.startEndOfSong();
                    else
                        sleepTimer.start(step.mid(6).toInt());
                } else if (step.startsWith(QLatin1String("rg:"))) { // ReplayGain mode 0 / 1 / 2
                    settings.setReplayGain(step.mid(3).toInt());
                } else if (step == QLatin1String("hide")) { // like closing with background playing
                    qobject_cast<QQuickWindow *>(root)->hide();
                } else if (step == QLatin1String("printstate")) {
                    std::printf("app state: %d (4 = active)\\n", int(QGuiApplication::applicationState()));
                } else if (step == QLatin1String("devices")) { // the output devices
                    for (const QVariant &d : player.outputDevices())
                        std::printf("device: %s | %s\\n", qPrintable(d.toMap().value(QStringLiteral("id")).toString()),
                                    qPrintable(d.toMap().value(QStringLiteral("name")).toString()));
                } else if (step.startsWith(QLatin1String("device:"))) { // device:<n>: switch to the nth, print the position around it
                    const QVariantList devices = player.outputDevices();
                    const int n = step.mid(7).toInt();
                    std::printf("before switch: pos=%lldms\\n", (long long)player.position());
                    if (n < devices.size())
                        player.setOutputDevice(devices[n].toMap().value(QStringLiteral("id")).toString());
                    for (int t : {50, 300, 1000})
                        QTimer::singleShot(t, &player, [&player, t] { std::printf("  +%dms: pos=%lldms\\n", t, (long long)player.position()); });
                } else if (step == QLatin1String("histoff")) {
                    settings.setHistoryEnabled(false);
                } else if (step == QLatin1String("printeq")) {
                    QStringList gains;
                    for (qreal g : settings.eqGains())
                        gains << QString::number(g);
                    std::printf("eq: on=%d preset=%s gains=%s\\n", int(settings.eqEnabled()), qPrintable(settings.eqPreset()),
                                qPrintable(gains.join(QLatin1Char(','))));
                } else if (step == QLatin1String("printhist")) {
                    std::printf("history: %s\\n", qPrintable(settings.ytSearchHistory().join(QStringLiteral(" | "))));
                } else if (step == QLatin1String("next")) {
                    playlist.next();
                } else if (step == QLatin1String("printctx")) { // what is playing, and from where
                    std::printf("ctx: playing=%d pos=%llds title=\\"%s\\" fromList=%d current=\\"%s\\" hasNext=%d upcomingMin=%.1f queue=%d\\n",
                                int(player.playing()), (long long)(player.position() / 1000),
                                qPrintable(playlist.contextTitle()), int(playlist.playingFromList()),
                                qPrintable(playlist.currentTitle()), int(playlist.hasNext()), playlist.upcomingMs() / 60000.0,
                                playlist.count());
                } else if (step == QLatin1String("printq")) { // the Queue's songs in order
                    std::printf("queue:");
                    for (int r = 0; r < playlist.count(); ++r)
                        std::printf(" %s", qPrintable(QFileInfo(playlist.pathAt(r)).baseName()));
                    std::printf("\\n");
                }
                else if (step == QLatin1String("wait"))
                    ;
                else if (step == QLatin1String("player"))
                    QMetaObject::invokeMethod(root, "showPlayer");
                else
                    QMetaObject::invokeMethod(root, qPrintable(QLatin1String("toggle") + step.at(0).toUpper() + step.mid(1)));
                QTimer::singleShot(700, [columns, step] { columns(step); });
            });
    }
    if (qEnvironmentVariableIsSet("SHOT_PANEL_SIZES")) // print the size of every visible panel card
        QTimer::singleShot(3000, [&, findItems] {
            for (const char *type : {"PlaylistPanel", "YouTubePanel", "LibraryPanel"})
                for (QQuickItem *panel : findItems(type))
                    if (panel->isVisible() && panel->opacity() > 0.99)
                        std::printf("%s: x=%.0f width=%.0f height=%.0f\\n", type, panel->mapToScene({0, 0}).x(), panel->width(),
                                    panel->height());
            std::fflush(stdout);
        });
    if (qEnvironmentVariableIsSet("SHOT_KEYS")) { // press these key sequences (space-separated), print the state after each
        const QStringList keys = qEnvironmentVariable("SHOT_KEYS").split(QLatin1Char(' '), Qt::SkipEmptyParts);
        for (int i = 0; i < keys.size(); ++i)
            QTimer::singleShot(2000 + i * 400, [&, keys, i] {
                auto *w = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
                QTest::keySequence(w, QKeySequence(keys[i]));
                QTimer::singleShot(250, [&, w, key = keys[i]] {
                    QObject *root = engine.rootObjects().constFirst();
                    const auto opened = [&](const char *name) {
                        QObject *o = root->findChild<QObject *>(name);
                        return o && o->property("opened").toBool() ? 1 : 0;
                    };
                    QObject *now = root->findChild<QObject *>("nowPlaying");
                    QQuickItem *focus = w->activeFocusItem();
                    std::printf("%-12s playing=%d pos=%5lld vol=%.2f muted=%d shuffle=%d repeat=%d index=%d title=%s lyrics=%d queueDrawer=%d yt=%d lib=%d focus=%s\\n",
                                qPrintable(key), int(player.playing()), qint64(player.position()), player.volume(), int(player.muted()),
                                int(playlist.shuffle()), int(playlist.repeatMode()), playlist.currentIndex(), qPrintable(playlist.currentTitle()),
                                now ? int(now->property("lyricsOpen").toBool()) : -1, opened("playlistDrawer"), opened("youtubeDrawer"),
                                opened("libraryDrawer"), focus ? focus->metaObject()->className() : "none");
                    std::fflush(stdout);
                });
            });
    }
    for (const char *mode : {"SHOT_PICK_M3U", "SHOT_SAVE_M3U"}) // the picker opening / saving a playlist file in that folder
        if (qEnvironmentVariableIsSet(mode))
            QTimer::singleShot(2600, [&, mode] {
                QVariantMap options{{QStringLiteral("files"), QStringList{QStringLiteral("*.m3u"), QStringLiteral("*.m3u8")}}};
                const bool save = QByteArray(mode) == "SHOT_SAVE_M3U";
                if (save) {
                    options.insert(QStringLiteral("save"), true);
                    options.insert(QStringLiteral("name"), qEnvironmentVariableIsSet("SHOT_SAVE_NAME") ? qEnvironmentVariable("SHOT_SAVE_NAME") : QStringLiteral("Queue.m3u8"));
                }
                if (QObject *picker = engine.rootObjects().constFirst()->findChild<QObject *>("folderPicker"))
                    QMetaObject::invokeMethod(picker, "pick", Q_ARG(QVariant, QVariant(save ? QStringLiteral("Export as M3U") : QStringLiteral("Import an M3U playlist"))),
                                              Q_ARG(QVariant, QVariant(save ? QStringLiteral("Save") : QString())),
                                              Q_ARG(QVariant, QVariant(QStringLiteral("file://") + qEnvironmentVariable(mode))),
                                              Q_ARG(QVariant, QVariant()), Q_ARG(QVariant, QVariant(options)));
            });
    if (qEnvironmentVariableIsSet("SHOT_PRINT_PLAYLISTS")) // the playlists, a while after start
        QTimer::singleShot(qEnvironmentVariableIntValue("SHOT_PRINT_PLAYLISTS"), [&] {
            for (const QVariant &p : playlists.property("playlists").toList())
                std::printf("playlist: %s (%d songs)\\n", qPrintable(p.toMap().value(QStringLiteral("name")).toString()),
                            p.toMap().value(QStringLiteral("count")).toInt());
            std::printf("viewing: %s\\n", qPrintable(playlists.property("viewedName").toString()));
            std::fflush(stdout);
        });
    if (qEnvironmentVariableIsSet("SHOT_TOOLTIP")) // hover the settings button like a real mouse
        QTimer::singleShot(2000, [&, findItems] {
            auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
            for (QQuickItem *item : findItems("PillButton")) {
                if (item->property("iconName").toString() == QStringLiteral("settings") && item->isVisible()) {
                    const QPointF pos = item->mapToScene(QPointF(item->width() / 2, item->height() / 2));
                    for (int i = 0; i < 3; ++i) { // enter, then move a little
                        const QPointF p = pos + QPointF(i, 0);
                        QMouseEvent move(QEvent::MouseMove, p, p, window->mapToGlobal(p), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
                        QCoreApplication::sendEvent(window, &move);
                    }
                    break;
                }
            }
        });
    if (qEnvironmentVariableIsSet("SHOT_SETTINGS_BOTTOM")) // scroll the settings dialog to its end
        QTimer::singleShot(6450, [&, findItems] {
            for (QQuickItem *f : findItems("QQuickFlickable"))
                if (f->isVisible())
                    f->setProperty("contentY", qMax(0.0, f->property("contentHeight").toDouble() - f->height()));
        });
    if (qEnvironmentVariableIsSet("SHOT_MID")) { // start a transition at 2.4 s, grab mid.png SHOT_MID ms later
        QTimer::singleShot(2400, [&] {
            const QString action = qEnvironmentVariable("SHOT_MID_ACTION");
            QObject *root = engine.rootObjects().constFirst();
            if (action == QStringLiteral("dock"))
                settings.setPlaylistPinned(!settings.playlistPinned());
            else if (action == QStringLiteral("drawer")) {
                if (QObject *drawer = root->findChild<QObject *>("playlistDrawer"))
                    QMetaObject::invokeMethod(drawer, "open");
            } else if (action == QStringLiteral("lyrics")) {
                if (QObject *panel = root->findChild<QObject *>("nowPlaying"))
                    panel->setProperty("lyricsOpen", true);
            } else if (action == QStringLiteral("view")) {
                playlists.view(qEnvironmentVariable("SHOT_MID_VIEW"));
            } else if (action == QStringLiteral("close")) { // close the window like its title-bar button
                auto *w = qobject_cast<QQuickWindow *>(root);
                std::printf("closing: background=%d hasTrack=%d\\n", int(settings.backgroundPlayback()), int(player.hasTrack()));
                w->close();
                std::printf("after close: visible=%d playing=%d\\n", int(w->isVisible()), int(player.playing()));
                std::fflush(stdout);
                QObject::connect(&instance, &SingleInstance::activationRequested, [w] {
                    std::printf("activation requested; visible now=%d\\n", int(w->isVisible()));
                    std::fflush(stdout);
                });
                QTimer::singleShot(4500, [w] { // e.g. after the app was launched again
                    std::printf("4.5 s after close: visible=%d\\n", int(w->isVisible()));
                    std::fflush(stdout);
                    QCoreApplication::quit(); // closing only hides it now
                });
            } else if (action == QStringLiteral("quit")) { // like the power button / Ctrl+Q / MPRIS Quit
                std::printf("quitting with background=%d hasTrack=%d\\n", int(settings.backgroundPlayback()), int(player.hasTrack()));
                std::fflush(stdout);
                QMetaObject::invokeMethod(root, "quitApp"); // what Ctrl+Q / the power button / MPRIS Quit call
                QTimer::singleShot(1000, [] { std::printf("STILL RUNNING 1 s after quit\\n"); std::fflush(stdout); });
            } else if (action == QStringLiteral("playnext")) { // play SHOT_PLAY_AT, then "Play next" SHOT_PLAYNEXT (indices)
                const auto order = [&] {
                    QStringList names;
                    for (int i = 0; i < playlist.count(); ++i)
                        names << QFileInfo(playlist.pathAt(i)).baseName();
                    return names.join(QLatin1Char(' '));
                };
                playlist.playIndex(qEnvironmentVariableIntValue("SHOT_PLAY_AT"));
                QVariantList items;
                for (const QString &i : qEnvironmentVariable("SHOT_PLAYNEXT").split(QLatin1Char(',')))
                    items << QVariantMap{{QStringLiteral("path"), playlist.pathAt(i.toInt())}};
                std::printf("before: %s | playing %s\\n", qPrintable(order()), qPrintable(playlist.currentTitle()));
                playlists.playNext(items);
                std::printf("after:  %s | playing %s\\n", qPrintable(order()), qPrintable(playlist.currentTitle()));
                playlist.next();
                std::printf("next -> %s\\n", qPrintable(playlist.currentTitle()));
                std::fflush(stdout);
            } else if (action == QStringLiteral("pinyt")) { // pin YouTube: the Library must unpin
                settings.setYoutubePanelPinned(true);
                std::printf("pinned youtube: youtube=%d library=%d\\n", int(settings.youtubePanelPinned()),
                            int(settings.libraryPanelPinned()));
            } else if (action == QStringLiteral("clear")) { // clear the Queue, report what still plays
                playlist.clear();
                std::printf("cleared: count=%d playing=%d title=%s\\n", playlist.count(), int(player.playing()),
                            qPrintable(playlist.currentTitle()));
            }
        });
        QTimer::singleShot(2400 + qEnvironmentVariableIntValue("SHOT_MID"), [&] {
            auto *w = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
            std::printf("mid.png %s\\n", w->grabWindow().save("build/shot/mid.png") ? "saved" : "FAILED");
            if (qEnvironmentVariable("SHOT_MID_ACTION") == QStringLiteral("close")) {
                std::printf("still running %d ms later: visible=%d playing=%d pos=%lld\\n", qEnvironmentVariableIntValue("SHOT_MID"),
                            int(w->isVisible()), int(player.playing()), qint64(player.position()));
                std::fflush(stdout);
            }
            if (qEnvironmentVariable("SHOT_MID_ACTION") == QStringLiteral("clear"))
                std::printf("later: playing=%d pos=%lld title=%s\\n", int(player.playing()), qint64(player.position()),
                            qPrintable(playlist.currentTitle()));
        });
    }
    if (qEnvironmentVariableIsSet("SHOT_DROP")) // drop that folder on the window centre at 2.6 s (like a file manager)
        QTimer::singleShot(2600, [&] {
            auto *w = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
            static QMimeData mime;
            mime.setUrls({QUrl::fromLocalFile(qEnvironmentVariable("SHOT_DROP"))});
            const QPointF p(w->width() / 2.0, w->height() / 2.0);
            QDragEnterEvent enter(p.toPoint(), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
            QCoreApplication::sendEvent(w, &enter);
            QDragMoveEvent move(p.toPoint(), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
            QCoreApplication::sendEvent(w, &move);
            QDropEvent drop(p, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
            QCoreApplication::sendEvent(w, &drop);
            std::printf("drop accepted: %d\\n", int(drop.isAccepted()));
        });
    if (qEnvironmentVariableIsSet("SHOT_WHEEL")) // mouse wheel over the middle of each visible list
        QTimer::singleShot(2600, [&, findItems] {
            auto *w = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
            if (qEnvironmentVariableIntValue("SHOT_WHEEL") == 2) // experiment: without the drop zone
                for (QQuickItem *drop : findItems("DropOverlay"))
                    drop->setVisible(false);
            if (qEnvironmentVariableIntValue("SHOT_WHEEL") == 3) // experiment: rows without their WheelHandler
                for (QQuickItem *row : findItems("PlaylistRow"))
                    for (QObject *child : row->children())
                        if (QByteArray(child->metaObject()->className()).startsWith("QQuickWheelHandler"))
                            child->setProperty("enabled", false);
            for (QQuickItem *view : findItems("QQuickListView")) {
                if (!view->isVisible() || view->height() < 100)
                    continue;
                const QPointF p = view->mapToScene(QPointF(view->width() / 2, view->height() / 2));
                const double before = view->property("contentY").toDouble();
                QQuickItem *hit = w->contentItem()->childAt(p.x(), p.y());
                for (int i = 0; i < 4; ++i) { // like a real mouse: notches 80 ms apart
                    QTimer::singleShot(i * 80, [w, p, i] {
                        // SHOT_WHEEL_TOUCHPAD: two-finger scroll (pixel deltas, scroll phases)
                        static QPointingDevice touchpad("test touchpad", 4242, QInputDevice::DeviceType::TouchPad,
                                                        QPointingDevice::PointerType::Finger,
                                                        QInputDevice::Capability::Position | QInputDevice::Capability::Scroll
                                                            | QInputDevice::Capability::PixelScroll,
                                                        2, 0);
                        const bool pad = qEnvironmentVariableIsSet("SHOT_WHEEL_TOUCHPAD");
                        const Qt::ScrollPhase phase = !pad ? Qt::NoScrollPhase : i == 0 ? Qt::ScrollBegin : Qt::ScrollUpdate;
                        QWheelEvent e(p, w->mapToGlobal(p), pad ? QPoint(0, -60) : QPoint(), QPoint(0, pad ? -60 : -120),
                                      Qt::NoButton, Qt::NoModifier, phase, false, Qt::MouseEventNotSynthesized,
                                      pad ? &touchpad : QPointingDevice::primaryPointingDevice());
                        e.setTimestamp(ulong(100000 + i * 80));
                        QCoreApplication::sendEvent(w, &e);
                    });
                }
                QTimer::singleShot(900, [view, before] {
                    std::printf("wheel: list %s contentY %.0f -> %.0f\\n", view->objectName().toUtf8().constData(),
                                before, view->property("contentY").toDouble());
                });
                (void)hit;
            }
        });
    if (qEnvironmentVariableIsSet("SHOT_LIBRARY")) // open the Library on that tab (album: SHOT_LIBRARY_ALBUM=1)
        QTimer::singleShot(2200, [&] {
            QObject *root = engine.rootObjects().constFirst();
            if (QObject *panel = root->findChild<QObject *>("libraryPanel"))
                panel->setProperty("tab", qEnvironmentVariableIntValue("SHOT_LIBRARY"));
            if (qEnvironmentVariableIsSet("SHOT_LIBRARY_ALBUM") && !localLibrary.albums().isEmpty())
                localLibrary.showAlbum(localLibrary.albums().constFirst().toMap().value(QStringLiteral("key")).toString());
            if (QObject *drawer = root->findChild<QObject *>("libraryDrawer"))
                QMetaObject::invokeMethod(drawer, "open");
            if (qEnvironmentVariableIsSet("SHOT_LIBRARY_PLAY")) { // play Songs row n (as a list), add the opened album to the Queue
                localLibrary.play(LocalLibraryController::SongsList, qEnvironmentVariableIntValue("SHOT_LIBRARY_PLAY"));
                localLibrary.addToQueue(LocalLibraryController::DetailList, -1);
                QTimer::singleShot(600, [&] {
                    std::printf("playing: %s | from list: %d | queue: %d songs\\n", player.source().toUtf8().constData(),
                                int(playlist.playingFromList()), playlist.count());
                });
            }
            std::printf("library: %d songs, %d albums, %d artists\\n", localLibrary.songCount(),
                        int(localLibrary.albums().size()), int(localLibrary.artists().size()));
        });
    if (qEnvironmentVariableIsSet("SHOT_FOLDER_PICKER")) // open the Library's folder picker at that folder
        QTimer::singleShot(2600, [&] {
            if (QObject *picker = engine.rootObjects().constFirst()->findChild<QObject *>("folderPicker"))
                QMetaObject::invokeMethod(picker, "pick", Q_ARG(QVariant, QVariant(QStringLiteral("Add a music folder"))),
                                          Q_ARG(QVariant, QVariant(QStringLiteral("Add this folder"))),
                                          Q_ARG(QVariant, QVariant(QStringLiteral("file://") + qEnvironmentVariable("SHOT_FOLDER_PICKER"))),
                                          Q_ARG(QVariant, QVariant()));
            if (QObject *picker = engine.rootObjects().constFirst()->findChild<QObject *>("folderPicker"))
                QTimer::singleShot(400, [picker] {
                    std::printf("picker opened=%d visible=%d w=%.0f h=%.0f parent=%s\\n", picker->property("opened").toBool(),
                                picker->property("visible").toBool(), picker->property("width").toDouble(),
                                picker->property("height").toDouble(),
                                picker->property("parent").value<QObject *>() ? picker->property("parent").value<QObject *>()->metaObject()->className() : "null");
                });
            if (false)
                std::printf("folder picker not found\\n");
        });
    const auto grab = [&](const char *file) {
        auto *w = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        QImage img = w->grabWindow();
        std::printf("%s: %dx%d %s\\n", file, img.width(), img.height(), img.save(file) ? "saved" : "FAILED");
    };
    QTimer::singleShot((qEnvironmentVariableIsSet("SHOT_WAIT") ? qEnvironmentVariableIntValue("SHOT_WAIT") : 3500), [&] { grab("build/shot/playing.png"); player.pause(); });
    QTimer::singleShot(6200, [&] {
        grab("build/shot/paused.png");
        QObject *root = engine.rootObjects().constFirst();
        if (qEnvironmentVariableIsSet("SHOT_DETAILS")) {
            if (QObject *dialog = root->findChild<QObject *>("detailsDialog"))
                QMetaObject::invokeMethod(dialog, "show", Q_ARG(QVariant, QVariant(player.source())));
        } else if (qEnvironmentVariableIsSet("SHOT_OPEN")) {
            // layout tests: no dialog over the panels
        } else if (QObject *dialog = root->findChild<QObject *>("settingsDialog")) {
            if (qEnvironmentVariableIsSet("SHOT_SETTINGS_PAGE")) // which page settings.png shows
                dialog->setProperty("page", qEnvironmentVariableIntValue("SHOT_SETTINGS_PAGE"));
            QMetaObject::invokeMethod(dialog, "open");
        }
    });
    QTimer::singleShot(qEnvironmentVariableIsSet("SHOT_QUIT_MS") ? qEnvironmentVariableIntValue("SHOT_QUIT_MS") : qEnvironmentVariableIsSet("SHOT_DETAILS") ? 9000 : 6500, [&] { grab("build/shot/settings.png"); QMetaObject::invokeMethod(engine.rootObjects().constFirst(), "quitApp"); });
    return app.exec();'''
assert s.count("    return app.exec();") == 1
pathlib.Path("build/shot/shotmain.cpp").write_text(s.replace("    return app.exec();", hook))
PY
OBJ=build/CMakeFiles/ChaoticMusicPlayer.dir
objs=$(find $OBJ -name '*.o' ! -name 'main.cpp.o')
version=$(sed -n 's/^project(ChaoticMusicPlayer VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)
g++ -std=c++20 -fPIC -I. -Ibuild/ChaoticMusicPlayer_autogen/include -DAPP_VERSION="\"$version\"" \
  $(pkg-config --cflags Qt6Gui Qt6Qml Qt6Quick Qt6QuickControls2 Qt6DBus Qt6Network Qt6Test) \
  build/shot/shotmain.cpp $objs build/qml/ChaoticStyle/libchaoticstyleplugin.a build/libchaoticstyle.a build/libbackend.a \
  $(pkg-config --libs Qt6Quick Qt6Qml Qt6Gui Qt6DBus Qt6Network Qt6QuickControls2 Qt6Test taglib libavformat libavcodec libavutil libswresample libpipewire-0.3 libcurl icu-uc icu-i18n) \
  -lpthread -o build/shot/shot
echo "harness built"
