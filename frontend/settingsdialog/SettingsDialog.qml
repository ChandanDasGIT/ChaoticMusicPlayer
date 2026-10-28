pragma ComponentBehavior: Bound

import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// App settings: a sidebar of pages (tabs along the top on narrow windows),
// each page made of cards. Every change is saved immediately (SettingsController).
// Reopens on the page used last.
Popup {
    id: root

    property int page: 0
    readonly property bool narrow: width < 640
    readonly property var pages: [
        { name: qsTr("General"), icon: "sliders" },
        { name: qsTr("Playback"), icon: "play" },
        { name: qsTr("Appearance"), icon: "palette" },
        { name: qsTr("Library"), icon: "compactDisc" },
        { name: qsTr("Album art"), icon: "image" },
        { name: qsTr("Lyrics"), icon: "lyrics" },
        { name: qsTr("YouTube Music"), icon: "cloud" },
        { name: qsTr("Keyboard shortcuts"), icon: "keyboard" },
        { name: qsTr("About"), icon: "info" }
    ]

    // Asks Main for the folder picker (it lives in the main window).
    signal excludeFolderRequested(string startFolder)

    anchors.centerIn: Overlay.overlay
    width: Math.min(Overlay.overlay ? Overlay.overlay.width - 2 * Theme.padding : 820, 820)
    height: Math.min(Overlay.overlay ? Overlay.overlay.height - 2 * Theme.padding : 640, 640)
    modal: true
    focus: true
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.anim }
        NumberAnimation { property: "scale"; from: 0.94; to: 1; duration: Theme.anim; easing.type: Theme.easing }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; to: 0; duration: Theme.animFast }
        NumberAnimation { property: "scale"; to: 0.96; duration: Theme.animFast }
    }

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radiusLarge * 1.5
        border.color: Theme.border
    }

    Overlay.modal: Rectangle {
        color: "#99000000"
        Behavior on opacity { NumberAnimation { duration: Theme.anim } }
    }

    contentItem: ColumnLayout {
        spacing: 0

        // ---- Title bar ----------------------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Theme.padding
            Layout.leftMargin: Theme.padding * 1.5

            Icon {
                name: "settings"
                size: 16
                color: Theme.accent
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Settings")
                color: Theme.text
                font.pixelSize: Theme.fontSizeTitle
                font.weight: Font.DemiBold
            }

            ControlButton {
                iconName: "close"
                iconSize: 13
                onClicked: root.close()
            }
        }

        // ---- Narrow windows: the pages as a row of tabs ---------------------------
        Flickable {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            Layout.bottomMargin: Theme.spacing
            visible: root.narrow
            contentWidth: tabRow.width + 2 * Theme.padding
            flickableDirection: Flickable.HorizontalFlick
            boundsBehavior: Flickable.StopAtBounds
            clip: true

            Row {
                id: tabRow
                x: Theme.padding
                spacing: 6

                Repeater {
                    model: root.pages

                    delegate: PillButton {
                        required property var modelData
                        required property int index

                        iconName: modelData.icon
                        iconSize: 11
                        text: modelData.name
                        implicitHeight: 34
                        active: root.page === index
                        onClicked: root.page = index
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // ---- Sidebar ------------------------------------------------------------
            Column {
                Layout.preferredWidth: 200
                Layout.fillHeight: true
                Layout.leftMargin: Theme.padding
                visible: !root.narrow
                spacing: 2

                Repeater {
                    model: root.pages

                    delegate: Rectangle {
                        id: navItem

                        required property var modelData
                        required property int index
                        readonly property bool current: root.page === index

                        width: parent.width
                        height: 38
                        radius: Theme.radius
                        color: current ? Theme.accentSoft : navHover.hovered ? Theme.hover : "transparent"

                        Behavior on color { ColorAnimation { duration: Theme.animFast } }

                        HoverHandler {
                            id: navHover
                            cursorShape: Qt.PointingHandCursor
                        }
                        TapHandler {
                            onTapped: root.page = navItem.index
                        }

                        Icon {
                            id: navIcon
                            x: 12
                            anchors.verticalCenter: parent.verticalCenter
                            width: 16
                            name: navItem.modelData.icon
                            size: 12
                            color: navItem.current ? Theme.accent : Theme.textMuted
                        }

                        Label {
                            anchors.left: navIcon.right
                            anchors.leftMargin: 12
                            anchors.right: parent.right
                            anchors.rightMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            text: navItem.modelData.name
                            color: navItem.current ? Theme.accent : Theme.text
                            font.pixelSize: Theme.fontSize
                            font.weight: navItem.current ? Font.DemiBold : Font.Normal
                            elide: Text.ElideRight
                        }
                    }
                }
            }

            // ---- Pages ----------------------------------------------------------------
            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: root.page

                // ------------------------------------------------------------ General
                SettingsPage {
                    title: qsTr("General")

                    SettingsCard {
                        title: qsTr("Startup and closing")

                        SettingsRow {
                            Layout.fillWidth: true
                            title: qsTr("Restore the Queue on startup")
                            description: qsTr("Start with the songs you had last time. Off: start empty.")

                            SettingsSwitch {
                                checked: SettingsController.restorePlaylist
                                onToggled: SettingsController.restorePlaylist = checked
                            }
                        }

                        SettingsRow {
                            Layout.fillWidth: true
                            title: qsTr("Keep playing after closing the window")
                            description: qsTr("Closing hides the window; open the app again to bring it back. Quit with the power button or Ctrl+Q.")

                            SettingsSwitch {
                                checked: SettingsController.backgroundPlayback
                                onToggled: SettingsController.backgroundPlayback = checked
                            }
                        }
                    }
                }

                // ------------------------------------------------------------ Playback
                SettingsPage {
                    title: qsTr("Playback")

                    SettingsCard {
                        title: qsTr("Output device")

                        SettingsRow {
                            Layout.fillWidth: true
                            description: qsTr("Where the music plays. Switching takes effect immediately.")

                            PillButton {
                                iconName: "refresh"
                                iconSize: 12
                                implicitWidth: 34
                                implicitHeight: 30
                                ToolTip.visible: hovered
                                ToolTip.text: qsTr("Refresh the list")
                                onClicked: PlayerController.refreshOutputDevices()
                            }
                        }

                        Repeater {
                            model: PlayerController.outputDevices

                            delegate: Rectangle {
                                id: deviceRow

                                required property var modelData
                                readonly property bool current: modelData.id === PlayerController.outputDevice

                                Layout.fillWidth: true
                                implicitHeight: 40
                                radius: Theme.radius
                                color: current ? Theme.accentSoft : deviceHover.hovered ? Theme.hover : "#0dffffff"

                                Behavior on color { ColorAnimation { duration: Theme.animFast } }

                                HoverHandler {
                                    id: deviceHover
                                    cursorShape: Qt.PointingHandCursor
                                }

                                TapHandler {
                                    onTapped: PlayerController.outputDevice = deviceRow.modelData.id
                                }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: Theme.padding
                                    anchors.rightMargin: Theme.padding
                                    spacing: Theme.spacing

                                    Icon {
                                        name: deviceRow.modelData.id === "" ? "settings" : "volume"
                                        size: 12
                                        color: deviceRow.current ? Theme.accent : Theme.textMuted
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: deviceRow.modelData.name
                                        color: deviceRow.current ? Theme.accent : Theme.text
                                        font.pixelSize: Theme.fontSize
                                        elide: Text.ElideRight
                                    }

                                    Icon {
                                        name: "check"
                                        size: 12
                                        color: Theme.accent
                                        scale: deviceRow.current ? 1 : 0
                                        visible: scale > 0

                                        Behavior on scale { NumberAnimation { duration: Theme.anim; easing.type: Theme.easingBack } }
                                    }
                                }
                            }
                        }
                    }

                    SettingsCard {
                        title: qsTr("Between songs")

                        SettingsRow {
                            Layout.fillWidth: true
                            title: qsTr("Crossfade")
                            description: qsTr("Fade each song into the next (skips too). Songs that run into each other on an album stay gapless.")

                            Stepper {
                                text: SettingsController.crossfadeSeconds > 0
                                      ? qsTr("%1 s").arg(SettingsController.crossfadeSeconds) : qsTr("Off")
                                canDecrease: SettingsController.crossfadeSeconds > 0
                                canIncrease: SettingsController.crossfadeSeconds < 12
                                onDecreased: SettingsController.crossfadeSeconds -= 1
                                onIncreased: SettingsController.crossfadeSeconds += 1
                            }
                        }
                    }

                    SettingsCard {
                        title: qsTr("Controls")

                        SettingsRow {
                            Layout.fillWidth: true
                            title: qsTr("Volume step")
                            description: qsTr("How much one mouse-wheel notch or Ctrl+↑ / ↓ changes the volume.")

                            Stepper {
                                text: SettingsController.volumeStep + "%"
                                canDecrease: SettingsController.volumeStep > 1
                                canIncrease: SettingsController.volumeStep < 25
                                onDecreased: SettingsController.volumeStep -= 1
                                onIncreased: SettingsController.volumeStep += 1
                            }
                        }

                        SettingsRow {
                            Layout.fillWidth: true
                            title: qsTr("Show remaining time")
                            description: qsTr("The time right of the seek bar counts down instead of showing the length.")

                            SettingsSwitch {
                                checked: SettingsController.showRemainingTime
                                onToggled: SettingsController.showRemainingTime = checked
                            }
                        }
                    }
                }

                // ------------------------------------------------------------ Appearance
                SettingsPage {
                    title: qsTr("Appearance")

                    SettingsCard {
                        title: qsTr("Background")

                        SettingsRow {
                            Layout.fillWidth: true
                            title: qsTr("Cover color strength")
                            description: qsTr("How strongly the background takes the album cover's color.")

                            RowLayout {
                                spacing: Theme.spacing

                                ThemedSlider {
                                    implicitWidth: 140
                                    from: 0
                                    to: 1
                                    value: SettingsController.coverTintStrength
                                    onMoved: SettingsController.coverTintStrength = value
                                }

                                Label {
                                    Layout.preferredWidth: 38
                                    horizontalAlignment: Text.AlignRight
                                    text: Math.round(SettingsController.coverTintStrength * 100) + "%"
                                    color: Theme.text
                                    font.pixelSize: Theme.fontSize
                                    font.features: { "tnum": 1 }
                                }
                            }
                        }
                    }

                    SettingsCard {
                        title: qsTr("Visualizer")

                        SettingsRow {
                            Layout.fillWidth: true
                            title: qsTr("Show the visualizer")
                            description: qsTr("The music's frequencies, under the volume bar.")

                            SettingsSwitch {
                                checked: SettingsController.visualizerEnabled
                                onToggled: SettingsController.visualizerEnabled = checked
                            }
                        }

                        Flow {
                            Layout.fillWidth: true
                            spacing: 6
                            enabled: SettingsController.visualizerEnabled
                            opacity: enabled ? 1 : 0.4

                            Behavior on opacity { NumberAnimation { duration: Theme.animFast } }

                            Repeater {
                                model: [qsTr("Bars"), qsTr("Mirrored bars"), qsTr("Wave")]

                                delegate: PillButton {
                                    required property string modelData
                                    required property int index

                                    text: modelData
                                    implicitHeight: 32
                                    active: SettingsController.visualizerStyle === index
                                    onClicked: SettingsController.visualizerStyle = index
                                }
                            }
                        }
                    }
                }

                // ------------------------------------------------------------ Library
                SettingsPage {
                    title: qsTr("Library")

                    SettingsCard {
                        title: qsTr("Excluded folders")

                        SettingsRow {
                            Layout.fillWidth: true
                            description: qsTr("Folders inside your library folders that are left out (e.g. ringtones, recordings). Library folders are added in the Library panel.")
                        }

                        Repeater {
                            model: SettingsController.libraryExcluded

                            delegate: Rectangle {
                                id: excludedRow

                                required property string modelData

                                Layout.fillWidth: true
                                implicitHeight: 40
                                radius: Theme.radius
                                color: "#14ffffff"

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 4
                                    spacing: Theme.spacing

                                    Icon {
                                        name: "eyeSlash"
                                        size: 12
                                        color: Theme.textMuted
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: excludedRow.modelData
                                        color: Theme.text
                                        font.pixelSize: Theme.fontSize
                                        elide: Text.ElideMiddle
                                    }

                                    PillButton {
                                        iconName: "close"
                                        iconSize: 11
                                        implicitWidth: 30
                                        implicitHeight: 30
                                        transparentIdle: true
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Include it again")
                                        onClicked: SettingsController.removeLibraryExcluded(excludedRow.modelData)
                                    }
                                }
                            }
                        }

                        PillButton {
                            iconName: "eyeSlash"
                            iconSize: 12
                            text: qsTr("Exclude a folder…")
                            implicitHeight: 32
                            onClicked: root.excludeFolderRequested(SettingsController.libraryFolders.length > 0
                                                                   ? "file://" + SettingsController.libraryFolders[0] : "")
                        }
                    }
                }

                // ------------------------------------------------------------ Album art
                SettingsPage {
                    title: qsTr("Album art")

                    SettingsCard {
                        title: qsTr("Missing covers")

                        SettingsRow {
                            Layout.fillWidth: true
                            title: qsTr("Find missing covers online")
                            description: qsTr("For songs without a cover, ask the sources below and show the sharpest match. Covers are cached; your files are not changed.")

                            SettingsSwitch {
                                checked: SettingsController.coversOnline
                                onToggled: SettingsController.coversOnline = checked
                            }
                        }
                    }

                    SettingsCard {
                        title: qsTr("Sources")

                        Repeater {
                            model: SettingsController.coverSources

                            delegate: SettingsRow {
                                id: coverSourceRow

                                required property var modelData

                                Layout.fillWidth: true
                                title: modelData.name
                                opacity: SettingsController.coversOnline || modelData.id === "folder" ? 1 : 0.4

                                SettingsSwitch {
                                    checked: coverSourceRow.modelData.enabled
                                    onToggled: SettingsController.setCoverSourceEnabled(coverSourceRow.modelData.id, checked)
                                }
                            }
                        }
                    }
                }

                // ------------------------------------------------------------ Lyrics
                SettingsPage {
                    title: qsTr("Lyrics")

                    SettingsCard {
                        title: qsTr("Sources")

                        SettingsRow {
                            Layout.fillWidth: true
                            description: qsTr("Tried from top to bottom until one has lyrics. Switch off the ones you don't want.")
                        }

                        Repeater {
                            model: SettingsController.lyricsSources

                            delegate: Rectangle {
                                id: sourceRow

                                required property var modelData
                                required property int index

                                Layout.fillWidth: true
                                implicitHeight: 44
                                radius: Theme.radius
                                color: "#0dffffff"

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: Theme.padding
                                    anchors.rightMargin: Theme.spacing
                                    spacing: Theme.spacing

                                    Label {
                                        text: (sourceRow.index + 1) + "."
                                        color: Theme.textMuted
                                        font.pixelSize: Theme.fontSize
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: sourceRow.modelData.name
                                        color: sourceRow.modelData.enabled ? Theme.text : Theme.textMuted
                                        font.pixelSize: Theme.fontSize
                                        elide: Text.ElideRight
                                    }

                                    PillButton {
                                        iconName: "arrowUp"
                                        iconSize: 11
                                        implicitWidth: 32
                                        implicitHeight: 28
                                        transparentIdle: true
                                        enabled: sourceRow.index > 0
                                        onClicked: SettingsController.moveLyricsSource(sourceRow.index, -1)
                                    }

                                    PillButton {
                                        iconName: "arrowDown"
                                        iconSize: 11
                                        implicitWidth: 32
                                        implicitHeight: 28
                                        transparentIdle: true
                                        enabled: sourceRow.index < SettingsController.lyricsSources.length - 1
                                        onClicked: SettingsController.moveLyricsSource(sourceRow.index, 1)
                                    }

                                    SettingsSwitch {
                                        checked: sourceRow.modelData.enabled
                                        onToggled: SettingsController.setLyricsSourceEnabled(sourceRow.index, checked)
                                    }
                                }
                            }
                        }
                    }

                    SettingsCard {
                        title: qsTr("Display")

                        SettingsRow {
                            Layout.fillWidth: true
                            title: qsTr("Synced lyrics by default")
                            description: qsTr("Highlight lines in time with the music when timing is available.")

                            SettingsSwitch {
                                checked: SettingsController.lyricsSyncedByDefault
                                onToggled: SettingsController.lyricsSyncedByDefault = checked
                            }
                        }

                        SettingsRow {
                            Layout.fillWidth: true
                            title: qsTr("Script")
                            description: qsTr("English letters: the song's own words written in English letters (e.g. “Aankhen band kar ke”), not a translation.")
                        }

                        Flow {
                            Layout.fillWidth: true
                            spacing: 6

                            Repeater {
                                model: [qsTr("Original script"), qsTr("English letters")]

                                delegate: PillButton {
                                    required property string modelData
                                    required property int index

                                    text: modelData
                                    implicitHeight: 32
                                    active: SettingsController.lyricsLatinScript === (index === 1)
                                    onClicked: SettingsController.lyricsLatinScript = index === 1
                                }
                            }
                        }
                    }
                }

                // ------------------------------------------------------------ YouTube Music
                SettingsPage {
                    title: qsTr("YouTube Music")

                    SettingsCard {
                        title: qsTr("Your account")

                        SettingsRow {
                            Layout.fillWidth: true
                            title: qsTr("Sign in through your browser")
                            description: qsTr("Pick the browser where you're signed in; yt-dlp reads your library through it. The app never sees your password. Close that browser if reading fails.")
                        }

                        Flow {
                            Layout.fillWidth: true
                            spacing: 6

                            Repeater {
                                model: [
                                    { id: "", name: qsTr("Not signed in") },
                                    { id: "firefox", name: "Firefox" },
                                    { id: "chrome", name: "Chrome" },
                                    { id: "chromium", name: "Chromium" },
                                    { id: "brave", name: "Brave" },
                                    { id: "edge", name: "Edge" },
                                    { id: "vivaldi", name: "Vivaldi" },
                                    { id: "opera", name: "Opera" }
                                ]

                                delegate: PillButton {
                                    required property var modelData

                                    text: modelData.name
                                    implicitHeight: 32
                                    active: SettingsController.ytCookiesBrowser === modelData.id
                                    onClicked: SettingsController.ytCookiesBrowser = modelData.id
                                }
                            }
                        }
                    }

                    SettingsCard {
                        title: qsTr("Your local songs")

                        SettingsRow {
                            Layout.fillWidth: true
                            title: qsTr("Look up track info in the background")
                            description: qsTr("Find your songs on YouTube Music and show that info in Track details.")

                            SettingsSwitch {
                                checked: SettingsController.ytBackgroundLookup
                                onToggled: SettingsController.ytBackgroundLookup = checked
                            }
                        }
                    }

                    SettingsCard {
                        title: qsTr("Playback")

                        SettingsRow {
                            Layout.fillWidth: true
                            title: qsTr("yt-dlp")
                            description: YouTubeController.available
                                         ? qsTr("Installed (version %1). It plays YouTube songs; keep it up to date.").arg(YouTubeController.ytDlpVersion)
                                         : qsTr("Not found. Install yt-dlp (e.g. sudo dnf install yt-dlp) to play YouTube songs.")
                        }
                    }
                }

                // ------------------------------------------------------------ Keyboard shortcuts
                SettingsPage {
                    title: qsTr("Keyboard shortcuts")

                    // Both lists mirror frontend/layout/AppShortcuts.qml.
                    SettingsCard {
                        title: qsTr("Playback")

                        ShortcutList {
                            model: [
                                [qsTr("Play / pause"), "Space"],
                                [qsTr("Next / previous song"), "Ctrl+→  /  Ctrl+←"],
                                [qsTr("Seek 5 s forward / back"), "→  /  ←"],
                                [qsTr("Seek 30 s forward / back"), "Shift+→  /  Shift+←"],
                                [qsTr("Volume up / down"), "Ctrl+↑  /  Ctrl+↓"],
                                [qsTr("Mute"), "M"],
                                [qsTr("Shuffle"), "Ctrl+S"],
                                [qsTr("Repeat (cycles)"), "Ctrl+R"]
                            ]
                        }
                    }

                    SettingsCard {
                        title: qsTr("Window")

                        ShortcutList {
                            model: [
                                [qsTr("Lyrics"), "Ctrl+L"],
                                [qsTr("Search (Library if open, else YouTube Music)"), "Ctrl+F"],
                                [qsTr("Queue / YouTube Music / Library"), "Ctrl+1  /  2  /  3"],
                                [qsTr("Settings"), "Ctrl+,"],
                                [qsTr("Close lyrics, panel or dialog"), "Esc"],
                                [qsTr("Quit"), "Ctrl+Q"]
                            ]
                        }
                    }
                }

                // ------------------------------------------------------------ About
                SettingsPage {
                    title: qsTr("About")

                    SettingsCard {
                        title: qsTr("ChaoticMusicPlayer")

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Version %1").arg(Qt.application.version)
                            color: Theme.text
                            font.pixelSize: Theme.fontSize
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("A music player for your local music and YouTube Music. Built with Qt, FFmpeg, PipeWire and TagLib.")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSizeSmall
                            wrapMode: Text.WordWrap
                        }

                        PillButton {
                            iconName: "link"
                            iconSize: 12
                            text: qsTr("Source code on GitHub")
                            implicitHeight: 32
                            onClicked: Qt.openUrlExternally("https://github.com/ChandanDasGIT/ChaoticMusicPlayer")
                        }
                    }

                    SettingsCard {
                        title: qsTr("Your data")

                        Repeater {
                            model: [
                                { name: qsTr("Settings"), base: StandardPaths.writableLocation(StandardPaths.GenericConfigLocation) },
                                { name: qsTr("Queue, playlists, library"), base: StandardPaths.writableLocation(StandardPaths.GenericDataLocation) },
                                { name: qsTr("Covers, lyrics, YouTube matches"), base: StandardPaths.writableLocation(StandardPaths.GenericCacheLocation) }
                            ]

                            delegate: SettingsRow {
                                id: dataRow

                                required property var modelData
                                readonly property string folder: modelData.base + "/ChaoticMusicPlayer"

                                Layout.fillWidth: true
                                title: modelData.name
                                description: folder.replace("file://", "")

                                PillButton {
                                    iconName: "folderOpen"
                                    iconSize: 12
                                    implicitWidth: 34
                                    implicitHeight: 30
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Open the folder")
                                    onClicked: Qt.openUrlExternally(dataRow.folder)
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // One page: a heading and its cards, scrolling; fades in when shown.
    component SettingsPage: ScrollView {
        id: settingsPage

        property string title
        default property alias content: pageColumn.data

        contentWidth: availableWidth
        clip: true

        onVisibleChanged: if (visible) pageFade.restart()

        NumberAnimation {
            id: pageFade
            target: settingsPage
            property: "opacity"
            from: 0
            to: 1
            duration: Theme.anim
            easing.type: Theme.easing
        }

        ColumnLayout {
            id: pageColumn
            width: settingsPage.availableWidth
            spacing: Theme.padding

            Label {
                Layout.leftMargin: Theme.padding
                Layout.topMargin: 2
                text: settingsPage.title
                color: Theme.text
                font.pixelSize: Theme.fontSizeLarge
                font.weight: Font.DemiBold
            }
        }
    }

    // A rounded card with a small accent heading.
    component SettingsCard: Rectangle {
        id: card

        property string title
        default property alias content: cardColumn.data

        Layout.fillWidth: true
        Layout.leftMargin: Theme.padding
        Layout.rightMargin: Theme.padding
        Layout.bottomMargin: 2
        implicitHeight: cardColumn.implicitHeight + 2 * Theme.padding
        radius: Theme.radiusLarge
        color: "#0dffffff"

        ColumnLayout {
            id: cardColumn
            anchors.fill: parent
            anchors.margins: Theme.padding
            spacing: Theme.padding

            Label {
                text: card.title.toUpperCase()
                color: Theme.accent
                font.pixelSize: Theme.fontSizeSmall
                font.bold: true
                font.letterSpacing: 1
            }
        }
    }

    // "−  value  +" for small numbers.
    component Stepper: RowLayout {
        id: stepper

        property string text
        property bool canDecrease: true
        property bool canIncrease: true

        signal decreased()
        signal increased()

        spacing: 4

        PillButton {
            iconName: "minus"
            iconSize: 11
            implicitWidth: 34
            implicitHeight: 30
            enabled: stepper.canDecrease
            onClicked: stepper.decreased()
        }

        Label {
            Layout.preferredWidth: 44
            horizontalAlignment: Text.AlignHCenter
            text: stepper.text
            color: Theme.text
            font.pixelSize: Theme.fontSize
            font.features: { "tnum": 1 }
        }

        PillButton {
            iconName: "plus"
            iconSize: 11
            implicitWidth: 34
            implicitHeight: 30
            enabled: stepper.canIncrease
            onClicked: stepper.increased()
        }
    }

    // Rows of "what it does ............ [keys]".
    component ShortcutList: ColumnLayout {
        id: shortcutList

        property var model: []

        Layout.fillWidth: true
        spacing: 6

        Repeater {
            model: shortcutList.model

            delegate: Item {
                id: shortcutRow

                required property var modelData

                Layout.fillWidth: true
                implicitHeight: 26

                Label {
                    anchors.left: parent.left
                    anchors.right: keys.left
                    anchors.rightMargin: Theme.padding
                    anchors.verticalCenter: parent.verticalCenter
                    text: shortcutRow.modelData[0]
                    color: Theme.text
                    font.pixelSize: Theme.fontSize
                    elide: Text.ElideRight
                }

                Rectangle {
                    id: keys
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    width: keysLabel.implicitWidth + 16
                    height: 24
                    radius: 6
                    color: Theme.surfaceAlt

                    Label {
                        id: keysLabel
                        anchors.centerIn: parent
                        text: shortcutRow.modelData[1]
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSizeSmall
                        font.weight: Font.DemiBold
                    }
                }
            }
        }
    }
}
