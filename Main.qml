pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

ApplicationWindow {
    id: window

    // Phone-like layout below the breakpoint.
    readonly property bool compact: width < Theme.compactBreakpoint
    // Pinned panels are columns next to the player on big windows; unpinned ones
    // (and every panel on phone-size windows) slide in over it when opened.
    readonly property bool playlistDocked: !compact && SettingsController.playlistPinned
    readonly property bool youtubeDocked: !compact && SettingsController.youtubePanelPinned
    readonly property bool libraryDocked: !compact && SettingsController.libraryPanelPinned
    // The width a panel would have if it were pinned right now (the window split
    // equally between the open columns), so a sliding panel matches its pinned
    // size. The right-hand side holds one panel at a time (YouTube or Library).
    function pinnedWidth(rightSide, minimumWidth) {
        const otherSide = rightSide ? playlistDocked : (youtubeDocked || libraryDocked)
        const columns = 2 + (otherSide ? 1 : 0)
        return Math.max(minimumWidth, (split.width - (columns - 1) * Theme.cardGap) / columns)
    }
    // Drawer width for a panel: its pinned width plus the gap around its card.
    // Phone size: an opened panel covers the whole window, nothing peeks through.
    function drawerWidth(rightSide, minimumWidth) {
        return compact ? width : Math.min(width, pinnedWidth(rightSide, minimumWidth) + 2 * Theme.cardGap)
    }

    // Background tone from the playing track's cover (blended by the strength
    // set in Settings); fades on track change.
    property color tint: CoverPalette.hasColor
                         ? Qt.tint(Theme.background, Qt.rgba(CoverPalette.color.r, CoverPalette.color.g,
                                                             CoverPalette.color.b, SettingsController.coverTintStrength))
                         : Theme.background
    Behavior on tint { ColorAnimation { duration: 800; easing.type: Easing.InOutQuad } }

    width: 1100
    height: 680
    minimumWidth: 360
    minimumHeight: 480
    visible: true
    title: "ChaoticMusicPlayer"
    color: Theme.background

    // Background playing: closing only hides the window while a song is loaded
    // (launching the app again or the desktop's media controls bring it back).
    // Quitting closes the windows too, so a real quit is flagged first.
    property bool quitting: false

    function quitApp() {
        quitting = true
        Qt.quit()
    }

    onClosing: (close) => {
        if (!quitting && SettingsController.backgroundPlayback && PlayerController.hasTrack) {
            close.accepted = false
            window.hide()
        }
    }

    Shortcut {
        sequence: StandardKey.Quit
        context: Qt.ApplicationShortcut
        onActivated: window.quitApp()
    }

    onPlaylistDockedChanged: if (playlistDocked) playlistDrawer.close()
    onYoutubeDockedChanged: if (youtubeDocked) youtubeDrawer.close()
    onLibraryDockedChanged: if (libraryDocked) libraryDrawer.close()

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: window.tint }
            GradientStop { position: 1.0; color: Qt.darker(window.tint, 2.2) }
        }
    }

    // Panels float as rounded cards over the tinted background.
    SplitView {
        id: split

        // Equal widths for every open column (fractional while one docks / undocks).
        readonly property real openColumns: 1 + playlistSlot.openness + youtubeSlot.openness + librarySlot.openness
        readonly property real share: (width - (openColumns - 1) * Theme.cardGap) / openColumns

        anchors.fill: parent
        anchors.margins: Theme.cardGap
        orientation: Qt.Horizontal

        // The gap between cards; a small bar appears while hovered or dragged.
        handle: Item {
            implicitWidth: Theme.cardGap

            Rectangle {
                anchors.centerIn: parent
                width: 4
                height: Math.min(parent.height * 0.3, 72)
                radius: 2
                color: Theme.accent
                opacity: parent.SplitHandle.pressed ? 1 : parent.SplitHandle.hovered ? 0.6 : 0

                Behavior on opacity { NumberAnimation { duration: Theme.animFast } }
            }
        }

        DockSlot {
            id: playlistSlot
            docked: window.playlistDocked
            share: split.share
            minimumWidth: 240
            leftSide: true

            PlaylistPanel {
                id: dockedPlaylist
                anchors.fill: parent
                showPinButton: true
                pinned: true
                onPinToggled: SettingsController.playlistPinned = false
                onDetailsRequested: (path) => detailsDialog.show(path)
            }
        }

        NowPlayingPanel {
            objectName: "nowPlaying"
            SplitView.fillWidth: true
            SplitView.minimumWidth: 280
            showPlaylistButton: !window.playlistDocked
            onPlaylistRequested: playlistDrawer.open()
            onDetailsRequested: detailsDialog.show(PlayerController.source)
            onSettingsRequested: settingsDialog.open()
            onQuitRequested: window.quitApp()
            youtubeOpen: window.youtubeDocked || youtubeDrawer.opened
            libraryOpen: window.libraryDocked || libraryDrawer.opened
            onLibraryRequested: {
                if (window.libraryDocked)
                    SettingsController.libraryPanelPinned = false
                else if (libraryDrawer.opened) {
                    libraryDrawer.close()
                } else {
                    youtubeDrawer.close()
                    libraryDrawer.open()
                }
            }
            onYoutubeRequested: {
                if (window.youtubeDocked)
                    SettingsController.youtubePanelPinned = false
                else if (youtubeDrawer.opened)
                    youtubeDrawer.close()
                else {
                    libraryDrawer.close()
                    youtubeDrawer.open()
                }
            }
        }

        DockSlot {
            id: youtubeSlot
            docked: window.youtubeDocked
            share: split.share
            minimumWidth: 320
            leftSide: false

            YouTubePanel {
                id: dockedYoutube
                anchors.fill: parent
                pinned: true
                onPinToggled: SettingsController.youtubePanelPinned = false
                onSettingsRequested: settingsDialog.open()
            }
        }

        // Shares the right-hand column with YouTube: only one of them is pinned.
        DockSlot {
            id: librarySlot
            docked: window.libraryDocked
            share: split.share
            minimumWidth: 320
            leftSide: false

            LibraryPanel {
                anchors.fill: parent
                pinned: true
                showCloseButton: false
                onPinToggled: SettingsController.libraryPanelPinned = false
                onDetailsRequested: (path) => detailsDialog.show(path)
                onSettingsRequested: settingsDialog.open()
                onAddFolderRequested: folderPicker.pick(qsTr("Add a music folder"), qsTr("Add this folder"), "",
                                                        (folder) => LocalLibraryController.addFolder(folder))
            }
        }
    }

    // Unpinned / phone size: the playlist card slides in from the left.
    Drawer {
        id: playlistDrawer
        objectName: "playlistDrawer"
        edge: Qt.LeftEdge
        enter: Transition {
            NumberAnimation { property: "position"; to: 1.0; duration: Theme.animPanel; easing.type: Theme.easingPanel }
        }
        exit: Transition {
            NumberAnimation { property: "position"; to: 0.0; duration: Theme.anim; easing.type: Easing.OutCubic }
        }
        width: window.drawerWidth(false, playlistSlot.minimumWidth)
        height: window.height
        padding: 0
        interactive: !window.playlistDocked
        background: Item {}

        DrawerCard {
            drawer: playlistDrawer
            transformOrigin: Item.Left

            PlaylistPanel {
                anchors.fill: parent
                showPinButton: !window.compact
                showCloseButton: true
                showMiniPlayer: true
                pinned: false
                onPinToggled: SettingsController.playlistPinned = true
                onCloseRequested: playlistDrawer.close()
                onOpenPlayerRequested: playlistDrawer.close()
                onDetailsRequested: (path) => detailsDialog.show(path)
            }
        }
    }

    // The YouTube Music card, sliding in from the right.
    Drawer {
        id: youtubeDrawer
        objectName: "youtubeDrawer"
        edge: Qt.RightEdge
        enter: Transition {
            NumberAnimation { property: "position"; to: 1.0; duration: Theme.animPanel; easing.type: Theme.easingPanel }
        }
        exit: Transition {
            NumberAnimation { property: "position"; to: 0.0; duration: Theme.anim; easing.type: Easing.OutCubic }
        }
        width: window.drawerWidth(true, youtubeSlot.minimumWidth)
        height: window.height
        padding: 0
        interactive: !window.youtubeDocked && !drawerYoutube.swipeBlocked
        background: Item {}

        DrawerCard {
            drawer: youtubeDrawer
            transformOrigin: Item.Right

            YouTubePanel {
                id: drawerYoutube
                objectName: "youtubePanel"
                anchors.fill: parent
                showPinButton: !window.compact
                showCloseButton: true
                showMiniPlayer: true
                onPinToggled: SettingsController.youtubePanelPinned = true
                onCloseRequested: youtubeDrawer.close()
                onOpenPlayerRequested: youtubeDrawer.close()
                onSettingsRequested: {
                    youtubeDrawer.close()
                    settingsDialog.open()
                }
            }
        }
    }

    // The local library, sliding in from the right.
    Drawer {
        id: libraryDrawer
        objectName: "libraryDrawer"
        edge: Qt.RightEdge
        enter: Transition {
            NumberAnimation { property: "position"; to: 1.0; duration: Theme.animPanel; easing.type: Theme.easingPanel }
        }
        exit: Transition {
            NumberAnimation { property: "position"; to: 0.0; duration: Theme.anim; easing.type: Easing.OutCubic }
        }
        width: window.drawerWidth(true, librarySlot.minimumWidth)
        height: window.height
        padding: 0
        interactive: !window.libraryDocked
        background: Item {}

        DrawerCard {
            drawer: libraryDrawer
            transformOrigin: Item.Right

            LibraryPanel {
                objectName: "libraryPanel"
                anchors.fill: parent
                showMiniPlayer: true
                showPinButton: !window.compact
                onPinToggled: SettingsController.libraryPanelPinned = true
                onCloseRequested: libraryDrawer.close()
                onOpenPlayerRequested: libraryDrawer.close()
                onDetailsRequested: (path) => detailsDialog.show(path)
                onSettingsRequested: {
                    libraryDrawer.close()
                    settingsDialog.open()
                }
                onAddFolderRequested: folderPicker.pick(qsTr("Add a music folder"), qsTr("Add this folder"), "",
                                                        (folder) => LocalLibraryController.addFolder(folder))
            }
        }
    }

    // A drawer's panel: an inset rounded card on a solid tinted background that
    // grows slightly and brightens as the drawer opens.
    component DrawerCard: Rectangle {
        required property Drawer drawer

        anchors.fill: parent
        anchors.margins: Theme.cardGap
        radius: Theme.radiusCard
        color: Qt.darker(window.tint, 1.6)
        scale: 0.96 + 0.04 * drawer.position
        opacity: 0.4 + 0.6 * drawer.position
    }

    // Drag & drop folders anywhere on the window, also over open drawers and
    // dialogs (it sits in the popup layer, above them; it only takes drags, so
    // clicks still reach what is underneath). Songs always go to the Queue.
    DropOverlay {
        parent: Overlay.overlay
        anchors.fill: parent
        z: 1000
        onUrlsDropped: {
            PlaylistsController.view("") // show the Queue, whatever list was open
            if (!window.playlistDocked) {
                youtubeDrawer.close()
                libraryDrawer.close()
                playlistDrawer.open()
            }
        }
    }

    TrackDetailsDialog {
        id: detailsDialog
        objectName: "detailsDialog"
    }

    // Playlist naming / deleting (asked for by PlaylistsController)
    NameDialog {
        id: nameDialog
        onAccepted: (name) => PlaylistsController.submitName(name)
    }

    ConfirmDialog {
        id: confirmDialog
        onConfirmed: PlaylistsController.confirmDelete()
    }

    Connections {
        target: PlaylistsController
        function onNamePrompt(title, initialName) { nameDialog.ask(title, initialName) }
        function onDeletePrompt(name) {
            confirmDialog.ask(qsTr("Delete “%1”?").arg(name),
                              qsTr("The playlist is removed. The songs themselves stay where they are."))
        }
        function onNotice(text) { toast.show(text) }
    }

    Connections {
        target: LocalLibraryController
        function onNotice(text) { toast.show(text) }
    }

    Toast {
        id: toast
    }

    // The app's folder chooser (Library: add folder; Settings: exclude folder)
    FolderPicker {
        id: folderPicker
        objectName: "folderPicker"
    }

    // Playback problems (e.g. YouTube limiting requests) in full; the line
    // under the title only has room for the start.
    Connections {
        target: PlayerController
        function onErrorStringChanged() {
            if (PlayerController.errorString !== "")
                toast.show(PlayerController.errorString)
        }
    }

    SettingsDialog {
        id: settingsDialog
        objectName: "settingsDialog"
        onExcludeFolderRequested: (startFolder) => folderPicker.pick(
            qsTr("Leave a folder out of the library"), qsTr("Exclude this folder"), startFolder,
            (folder) => SettingsController.addLibraryExcluded(folder))
    }
}
