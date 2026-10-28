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
s = "#include <QTest>\n#include <cstdio>\n#include <QQuickWindow>\n#include <QTimer>\n#include <QImage>\n#include <functional>\n#include <QMouseEvent>\n#include <QQuickItem>\n#include <QUrl>\n#include <QMimeData>\n#include <QDropEvent>\n#include <QWheelEvent>\n" + s
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
        else
            playlist.playIndex(0);
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
            if (qEnvironmentVariableIsSet("SHOT_LIBRARY_PLAY")) { // play Songs row n, add the opened album to the Queue
                localLibrary.playInQueue(false, qEnvironmentVariableIntValue("SHOT_LIBRARY_PLAY"));
                localLibrary.addToQueue(true, -1);
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
        } else if (QObject *dialog = root->findChild<QObject *>("settingsDialog")) {
            QMetaObject::invokeMethod(dialog, "open");
        }
    });
    QTimer::singleShot(qEnvironmentVariableIsSet("SHOT_DETAILS") ? 9000 : 6500, [&] { grab("build/shot/settings.png"); QMetaObject::invokeMethod(engine.rootObjects().constFirst(), "quitApp"); });
    return app.exec();'''
assert s.count("    return app.exec();") == 1
pathlib.Path("build/shot/shotmain.cpp").write_text(s.replace("    return app.exec();", hook))
PY
OBJ=build/CMakeFiles/ChaoticMusicPlayer.dir
objs=$(find $OBJ -name '*.o' ! -name 'main.cpp.o')
g++ -std=c++20 -fPIC -I. -Ibuild/ChaoticMusicPlayer_autogen/include \
  $(pkg-config --cflags Qt6Gui Qt6Qml Qt6Quick Qt6QuickControls2 Qt6DBus Qt6Network Qt6Test) \
  build/shot/shotmain.cpp $objs build/qml/ChaoticStyle/libchaoticstyleplugin.a build/libchaoticstyle.a build/libbackend.a \
  $(pkg-config --libs Qt6Quick Qt6Qml Qt6Gui Qt6DBus Qt6Network Qt6QuickControls2 Qt6Test taglib libavformat libavcodec libavutil libswresample libpipewire-0.3 libcurl icu-uc icu-i18n) \
  -lpthread -o build/shot/shot
echo "harness built"
