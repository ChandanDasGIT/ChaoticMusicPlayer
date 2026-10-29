pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

ApplicationWindow {
    id: window

    // ---- Layout -------------------------------------------------------------------
    // Picked by how much of the screen the window covers, so a full-screen
    // window behaves the same on any monitor:
    //  "wide" (most of the screen): columns Queue | Player | YouTube or Library;
    //  "half": two columns at most, so an opened panel can push the player out
    //          (a floating mini player then stands in for it);
    //  "compact" (phone size): the player alone, panels slide over it as drawers.
    // The columns share the width equally. YouTube and Library take turns in the
    // right-hand one. A pinned panel stays open (also after a restart).
    property real screenWidthOverride: 0 // tests: pretend the screen is this wide
    readonly property real screenWidth: screenWidthOverride > 0 ? screenWidthOverride : Screen.width > 0 ? Screen.width : width
    readonly property real minimumColumn: 300
    readonly property string layout: {
        const share = width / screenWidth
        if (width < Theme.compactBreakpoint || share < 0.4 || width < 2 * minimumColumn + 3 * Theme.cardGap)
            return "compact"
        return share >= 0.7 && width >= 3 * minimumColumn + 4 * Theme.cardGap ? "wide" : "half"
    }
    readonly property bool compact: layout === "compact"

    // Opened without being pinned (wide / half layouts; compact uses the drawers).
    property bool queueOpen: false
    property string rightOpen: ""      // "youtube" / "library", shown over a pinned one
    property bool playerRecalled: false // half: the player in place of two pinned panels
    property string lastOpened: ""     // the panel opened last pushes the player away from its side
    readonly property bool queuePinned: SettingsController.playlistPinned
    readonly property string rightPinned: SettingsController.youtubePanelPinned ? "youtube"
                                        : SettingsController.libraryPanelPinned ? "library" : ""
    // What the columns show
    readonly property bool playlistDocked: !compact && (queueOpen || queuePinned)
    readonly property string rightShown: compact || playerRecalled ? "" : rightOpen !== "" ? rightOpen : rightPinned
    readonly property bool youtubeDocked: rightShown === "youtube"
    readonly property bool libraryDocked: rightShown === "library"
    readonly property bool playerShown: layout !== "half" || !(playlistDocked && rightShown !== "")

    function openPanel(name) {
        playerRecalled = false
        lastOpened = name
        if (name === "queue") {
            queueOpen = true
            if (layout === "half")
                rightOpen = "" // only one unpinned panel at a time
        } else {
            rightOpen = name === rightPinned ? "" : name
            if (layout === "half" && !queuePinned)
                queueOpen = false
        }
    }
    // Closing a pinned panel unpins it.
    function closePanel(name) {
        if (name === "queue") {
            queueOpen = false
            SettingsController.playlistPinned = false
        } else if (rightOpen === name) {
            rightOpen = "" // a pinned panel under it comes back
        } else if (name === "youtube") {
            SettingsController.youtubePanelPinned = false
        } else {
            SettingsController.libraryPanelPinned = false
        }
    }
    // A column's pin button: the panel stays open either way.
    function togglePin(name) {
        if (name === "queue") {
            queueOpen = true
            SettingsController.playlistPinned = !queuePinned
        } else if (rightPinned === name) {
            rightOpen = name
            name === "youtube" ? (SettingsController.youtubePanelPinned = false)
                               : (SettingsController.libraryPanelPinned = false)
        } else {
            rightOpen = ""
            name === "youtube" ? (SettingsController.youtubePanelPinned = true)
                               : (SettingsController.libraryPanelPinned = true)
        }
    }
    // Half layout, the floating mini player tapped: make room for the player.
    function showPlayer() {
        if (rightOpen !== "")
            rightOpen = ""
        else if (queueOpen && !queuePinned)
            queueOpen = false
        else
            playerRecalled = true
    }

    // Panels opened in one layout stay open in the next.
    onLayoutChanged: {
        playerRecalled = false
        if (compact) {
            queueOpen = false
            rightOpen = ""
            return
        }
        if (playlistDrawer.opened)
            queueOpen = true
        if (youtubeDrawer.opened)
            rightOpen = rightPinned === "youtube" ? "" : "youtube"
        else if (libraryDrawer.opened)
            rightOpen = rightPinned === "library" ? "" : "library"
        playlistDrawer.close()
        youtubeDrawer.close()
        libraryDrawer.close()
        if (layout === "half" && queueOpen && !queuePinned && rightOpen !== "")
            queueOpen = false
    }

    // Background tone from the playing track's cover (blended by the strength
    // set in Settings); fades on track change. Light mode blends a pastel of
    // the cover's color (same hue, kept light), so dark covers can't muddy it.
    readonly property color coverTone: Theme.isDark ? CoverPalette.color
                                       : Qt.hsla(CoverPalette.color.hslHue, CoverPalette.color.hslSaturation, 0.86, 1)
    property color tint: CoverPalette.hasColor
                         ? Qt.tint(Theme.background, Qt.rgba(coverTone.r, coverTone.g, coverTone.b,
                                                             SettingsController.coverTintStrength))
                         : Theme.background
    Behavior on tint { ColorAnimation { duration: 800; easing.type: Easing.InOutQuad } }

    // The tooltip style (frontend/style) can't read Theme: it takes these.
    palette.toolTipBase: Theme.tooltip
    palette.toolTipText: Theme.text

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

    // The Now Playing buttons and the keyboard shortcuts (Ctrl+1 / 2 / 3) share these.
    function toggleQueue() {
        if (compact)
            playlistDrawer.opened ? playlistDrawer.close() : playlistDrawer.open()
        else
            playlistDocked ? closePanel("queue") : openPanel("queue")
    }
    function toggleYoutube() {
        if (!compact)
            youtubeDocked ? closePanel("youtube") : openPanel("youtube")
        else if (youtubeDrawer.opened)
            youtubeDrawer.close()
        else {
            libraryDrawer.close()
            youtubeDrawer.open()
        }
    }
    function toggleLibrary() {
        if (!compact)
            libraryDocked ? closePanel("library") : openPanel("library")
        else if (libraryDrawer.opened)
            libraryDrawer.close()
        else {
            youtubeDrawer.close()
            libraryDrawer.open()
        }
    }
    // Ctrl+F: the Library's search if the Library is showing, else YouTube Music's.
    // A drawer takes the focus itself once it has opened, so a drawer that is
    // still opening gets the search focused then (searchOnOpen).
    property bool searchOnOpen: false
    function focusSearch() {
        if (libraryDocked) {
            dockedLibrary.focusSearch()
        } else if (libraryDrawer.visible) {
            drawerLibrary.focusSearch()
            searchOnOpen = !libraryDrawer.opened
        } else if (youtubeDocked) {
            dockedYoutube.focusSearch()
        } else if (!compact) {
            openPanel("youtube")
            dockedYoutube.focusSearch()
        } else {
            if (!youtubeDrawer.visible)
                toggleYoutube()
            drawerYoutube.focusSearch()
            searchOnOpen = !youtubeDrawer.opened
        }
    }

    AppShortcuts {
        dialogOpen: settingsDialog.visible || detailsDialog.visible || nameDialog.visible || confirmDialog.visible
                    || folderPicker.visible
        escapeEnabled: nowPlaying.lyricsOpen && !dialogOpen && !playlistDrawer.opened && !youtubeDrawer.opened
                       && !libraryDrawer.opened
        onEscapePressed: nowPlaying.lyricsOpen = false
        onToggleLyrics: nowPlaying.lyricsOpen = !nowPlaying.lyricsOpen
        onFocusSearch: window.focusSearch()
        onToggleQueue: window.toggleQueue()
        onToggleYoutube: window.toggleYoutube()
        onToggleLibrary: window.toggleLibrary()
        onOpenSettings: settingsDialog.open()
        onQuit: window.quitApp()
    }

    onPlaylistDockedChanged: if (playlistDocked) playlistDrawer.close()
    onYoutubeDockedChanged: if (youtubeDocked) youtubeDrawer.close()
    onLibraryDockedChanged: if (libraryDocked) libraryDrawer.close()

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: window.tint }
            // Dark mode: deepens to near black; light mode: only a slight shade.
            GradientStop { position: 1.0; color: Qt.darker(window.tint, Theme.isDark ? 2.2 : 1.12) }
        }
    }

    // Panels float as rounded cards over the tinted background.
    SplitView {
        id: split

        // Equal widths for every open column (fractional while one docks / undocks).
        readonly property real openColumns: Math.max(1, playlistSlot.openness + playerSlot.openness
                                                     + rightSlot.openness)
        readonly property real share: (width - (openColumns - 1) * Theme.cardGap) / openColumns
        // A column opened or closed: back to equal widths, even after the dividers were dragged.
        readonly property string columns: [window.playlistDocked, window.playerShown, window.rightShown].join()
        onColumnsChanged: [playlistSlot, playerSlot, rightSlot].forEach((slot) => slot.equalize())

        anchors.fill: parent
        anchors.margins: Theme.cardGap
        // Room for the floating mini player under the columns
        anchors.bottomMargin: Theme.cardGap + (floatingMini.visible ? floatingMini.height + Theme.cardGap : 0)
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
            SplitView.fillWidth: !window.playerShown && window.rightShown === ""
            // Song rows keep their own swipe (delete): the header and empty space put the Queue away.
            swipeEnabled: window.layout === "half"
            swipeAnywhere: false
            onSwipedAway: window.closePanel("queue")

            PlaylistPanel {
                id: dockedPlaylist
                anchors.fill: parent
                acceptsDrops: true
                showPinButton: true
                pinned: window.queuePinned
                showCloseButton: !pinned
                onPinToggled: window.togglePin("queue")
                onCloseRequested: window.closePanel("queue")
                onDetailsRequested: (path) => detailsDialog.show(path)
                onImportM3uRequested: window.importM3u()
                onExportM3uRequested: (id, name) => window.exportM3u(id, name)
            }
        }

        // The player, pushed out by the panel opened last when there's no room (half layout)
        DockSlot {
            id: playerSlot
            docked: window.playerShown
            share: split.share
            minimumWidth: 280
            leftSide: window.lastOpened !== "queue"
            SplitView.fillWidth: window.playerShown

            NowPlayingPanel {
                id: nowPlaying
                objectName: "nowPlaying"
                anchors.fill: parent
                showPlaylistButton: !window.playlistDocked
                onPlaylistRequested: window.toggleQueue()
                onDetailsRequested: detailsDialog.show(PlayerController.source)
                onSettingsRequested: settingsDialog.open()
                onQuitRequested: window.quitApp()
                youtubeOpen: window.youtubeDocked || youtubeDrawer.opened
                libraryOpen: window.libraryDocked || libraryDrawer.opened
                onLibraryRequested: window.toggleLibrary()
                onYoutubeRequested: window.toggleYoutube()
            }

            // Songs dragged from the Library or YouTube: dropped here, they join the Queue.
            // (Phone size: the drawers' mini players take them instead.)
            DropHint {
                accepts: !window.compact
                onDropped: SongDrag.deliver("", -1)
            }
        }

        // YouTube or Library, one at a time: swapping them cross-fades in place.
        DockSlot {
            id: rightSlot
            docked: window.rightShown !== ""
            share: split.share
            minimumWidth: 320
            leftSide: false
            SplitView.fillWidth: !window.playerShown && docked
            swipeEnabled: window.layout === "half" && !(rightPanel === "youtube" && dockedYoutube.swipeBlocked)
            onSwipedAway: window.closePanel(rightPanel)

            // What the column shows (kept while it closes)
            property string rightPanel: "youtube"
            readonly property string shown: window.rightShown
            onShownChanged: if (shown !== "") rightPanel = shown

            YouTubePanel {
                id: dockedYoutube
                anchors.fill: parent
                pinned: window.rightPinned === "youtube"
                showCloseButton: !pinned
                onPinToggled: window.togglePin("youtube")
                onCloseRequested: window.closePanel("youtube")
                onSettingsRequested: settingsDialog.open()

                // Swapping: the outgoing panel fades out while this one fades and slides in.
                readonly property bool current: rightSlot.rightPanel === "youtube"
                opacity: current ? 1 : 0
                visible: current || opacity > 0
                z: current ? 1 : 0
                transform: Translate { x: (1 - dockedYoutube.opacity) * 24 }

                Behavior on opacity { NumberAnimation { duration: Theme.animPanel; easing.type: Theme.easingPanel } }
            }

            LibraryPanel {
                id: dockedLibrary
                anchors.fill: parent
                pinned: window.rightPinned === "library"
                showCloseButton: !pinned
                onPinToggled: window.togglePin("library")
                onCloseRequested: window.closePanel("library")
                onDetailsRequested: (path) => detailsDialog.show(path)
                onSettingsRequested: settingsDialog.open()
                onFoldersSettingsRequested: settingsDialog.openPage(settingsDialog.libraryPage)

                // Swapping: the outgoing panel fades out while this one fades and slides in.
                readonly property bool current: rightSlot.rightPanel === "library"
                opacity: current ? 1 : 0
                visible: current || opacity > 0
                z: current ? 1 : 0
                transform: Translate { x: (1 - dockedLibrary.opacity) * 24 }

                Behavior on opacity { NumberAnimation { duration: Theme.animPanel; easing.type: Theme.easingPanel } }
            }
        }
    }

    // Half layout: drag in from an empty window edge to bring that side's panel back.
    EdgeSwipe {
        anchors.left: parent.left
        enabled: window.layout === "half" && !window.playlistDocked
        inward: 1
        onTriggered: window.openPanel("queue")
    }
    EdgeSwipe {
        anchors.right: parent.right
        enabled: window.layout === "half" && window.rightShown === ""
        inward: -1
        onTriggered: window.openPanel(rightSlot.rightPanel)
    }

    // Half layout with the player pushed out: a floating mini player under the
    // columns stands in for it (tap the cover or title to bring the player back).
    MiniPlayer {
        id: floatingMini
        objectName: "floatingMini"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Theme.cardGap
        height: implicitHeight
        radius: Theme.radiusCard
        active: window.layout === "half" && !window.playerShown
        acceptsDrops: true
        onOpenPlayerRequested: window.showPlayer()
    }

    // Phone size: the playlist card slides in from the left, over the player.
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
        width: window.width
        height: window.height
        padding: 0
        interactive: window.compact
        background: Item {}

        DrawerCard {
            drawer: playlistDrawer
            transformOrigin: Item.Left

            PlaylistPanel {
                anchors.fill: parent
                showCloseButton: true
                showMiniPlayer: true
                onCloseRequested: playlistDrawer.close()
                onOpenPlayerRequested: playlistDrawer.close()
                onDetailsRequested: (path) => detailsDialog.show(path)
                onImportM3uRequested: window.importM3u()
                onExportM3uRequested: (id, name) => window.exportM3u(id, name)
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
        width: window.width
        height: window.height
        padding: 0
        interactive: window.compact && !drawerYoutube.swipeBlocked
        onOpened: if (window.searchOnOpen) { window.searchOnOpen = false; drawerYoutube.focusSearch() }
        onClosed: window.searchOnOpen = false
        background: Item {}

        DrawerCard {
            drawer: youtubeDrawer
            transformOrigin: Item.Right

            YouTubePanel {
                id: drawerYoutube
                objectName: "youtubePanel"
                anchors.fill: parent
                showPinButton: false
                showCloseButton: true
                showMiniPlayer: true
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
        width: window.width
        height: window.height
        padding: 0
        interactive: window.compact
        onOpened: if (window.searchOnOpen) { window.searchOnOpen = false; drawerLibrary.focusSearch() }
        onClosed: window.searchOnOpen = false
        background: Item {}

        DrawerCard {
            drawer: libraryDrawer
            transformOrigin: Item.Right

            LibraryPanel {
                id: drawerLibrary
                objectName: "libraryPanel"
                anchors.fill: parent
                showMiniPlayer: true
                showPinButton: false
                onCloseRequested: libraryDrawer.close()
                onOpenPlayerRequested: libraryDrawer.close()
                onDetailsRequested: (path) => detailsDialog.show(path)
                onSettingsRequested: {
                    libraryDrawer.close()
                    settingsDialog.open()
                }
                onFoldersSettingsRequested: {
                    libraryDrawer.close()
                    settingsDialog.openPage(settingsDialog.libraryPage)
                }
            }
        }
    }

    // A strip along a window edge: dragging inward from it opens that side's panel.
    component EdgeSwipe: Item {
        property int inward: 1 // +1: drag right (left edge), -1: drag left (right edge)
        signal triggered()

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: Theme.cardGap + 6
        z: 2

        DragHandler {
            property bool done: false

            enabled: !SongDrag.active

            target: null
            xAxis.enabled: true
            yAxis.enabled: false
            onActiveChanged: done = false
            onTranslationChanged: {
                if (active && !done && translation.x * parent.inward > 40) {
                    done = true
                    parent.triggered()
                }
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
        color: Theme.isDark ? Qt.darker(window.tint, 1.6) : Qt.tint(window.tint, "#b3ffffff")
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
            window.showPlaylistPanel()
        }
        onPlaylistsDropped: window.showPlaylistPanel() // the import opens the new playlist
    }

    // The card following the pointer while songs are dragged (over drawers too)
    DragCard {
        parent: Overlay.overlay
        z: 2000
    }

    function showPlaylistPanel() {
        if (!compact) {
            if (!playlistDocked)
                openPanel("queue")
        } else {
            youtubeDrawer.close()
            libraryDrawer.close()
            playlistDrawer.open()
        }
    }

    // M3U playlists through the app's own file picker.
    function importM3u() {
        folderPicker.pick(qsTr("Import an M3U playlist"), "", "", (file) => PlaylistsController.importM3u(file),
                          { files: ["*.m3u", "*.m3u8"] })
    }
    function exportM3u(id, name) {
        folderPicker.pick(qsTr("Export as M3U"), qsTr("Save"), "", (file) => PlaylistsController.exportM3u(id, file),
                          { save: true, files: ["*.m3u", "*.m3u8"], name: name.replace(/[\/]/g, "-") + ".m3u8" })
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
        onAddFolderRequested: folderPicker.pick(qsTr("Add a music folder"), qsTr("Add this folder"), "",
                                                (folder) => LocalLibraryController.addFolder(folder))
        onExcludeFolderRequested: (startFolder) => folderPicker.pick(
            qsTr("Leave a folder out of the library"), qsTr("Exclude this folder"), startFolder,
            (folder) => SettingsController.addLibraryExcluded(folder))
    }
}
