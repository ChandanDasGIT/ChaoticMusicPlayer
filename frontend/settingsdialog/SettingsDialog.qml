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
        { name: qsTr("Equalizer"), icon: "equalizer" },
        { name: qsTr("Appearance"), icon: "palette" },
        { name: qsTr("Library"), icon: "compactDisc" },
        { name: qsTr("Album art"), icon: "image" },
        { name: qsTr("Lyrics"), icon: "lyrics" },
        { name: qsTr("YouTube Music"), icon: "cloud" },
        { name: qsTr("Keyboard shortcuts"), icon: "keyboard" },
        { name: qsTr("About"), icon: "info" }
    ]

    readonly property int equalizerPage: 2
    readonly property int libraryPage: 4

    // Opens on that page (e.g. the Library's music folders).
    function openPage(index) {
        page = index
        open()
    }

    // Ask Main for the folder picker (it lives in the main window).
    signal addFolderRequested()
    signal clearHistoryRequested() // asks first (the confirmation lives in the main window)
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
        color: Theme.scrim
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
                            title: qsTr("Resume where you left off")
                            description: qsTr("Reopen on the last song, paused where it was.")

                            SettingsSwitch {
                                checked: SettingsController.resumeSession
                                onToggled: SettingsController.resumeSession = checked
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

                    SettingsCard {
                        title: qsTr("Notifications")

                        SettingsRow {
                            Layout.fillWidth: true
                            title: qsTr("Show the song when it changes")
                            description: qsTr("A desktop notification with the title, artist and cover, while the app's window isn't in front.")

                            SettingsSwitch {
                                checked: SettingsController.notificationsEnabled
                                onToggled: SettingsController.notificationsEnabled = checked
                            }
                        }
                    }

                    SettingsCard {
                        title: qsTr("Listening history")

                        SettingsRow {
                            Layout.fillWidth: true
                            title: qsTr("Keep a listening history")
                            description: qsTr("Remember the songs you listen to (half a song, or 4 minutes, counts) for Home. It stays on this computer.")

                            SettingsSwitch {
                                checked: SettingsController.historyEnabled
                                onToggled: SettingsController.historyEnabled = checked
                            }
                        }

                        SettingsRow {
                            Layout.fillWidth: true
                            enabled: SettingsController.historyEnabled
                            opacity: enabled ? 1 : 0.5
                            title: qsTr("Include YouTube Music songs")
                            description: qsTr("Off: only songs from your library and your files are remembered.")

                            SettingsSwitch {
                                checked: SettingsController.historyYouTube
                                onToggled: SettingsController.historyYouTube = checked
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true

                            Label {
                                Layout.fillWidth: true
                                text: HistoryController.count === 1 ? qsTr("1 song listened to so far.")
                                                                    : qsTr("%1 songs listened to so far.").arg(HistoryController.count)
                                color: Theme.textMuted
                                font.pixelSize: Theme.fontSizeSmall
                            }

                            PillButton {
                                iconName: "trash"
                                iconSize: 11
                                text: qsTr("Clear history…")
                                implicitHeight: 32
                                enabled: HistoryController.count > 0
                                onClicked: root.clearHistoryRequested()
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
                                color: current ? Theme.accentSoft : deviceHover.hovered ? Theme.hover : Theme.fillSoft

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
                            title: qsTr("Fade between songs")
                            description: qsTr("Each song fades out at its end and the next fades in from its start: nothing overlaps or is cut. Skipping fades briefly too. Songs that run into each other on an album stay gapless.")

                            Stepper {
                                text: SettingsController.fadeSeconds > 0
                                      ? qsTr("%1 s").arg(SettingsController.fadeSeconds) : qsTr("Off")
                                canDecrease: SettingsController.fadeSeconds > 0
                                canIncrease: SettingsController.fadeSeconds < 12
                                onDecreased: SettingsController.fadeSeconds -= 1
                                onIncreased: SettingsController.fadeSeconds += 1
                            }
                        }

                        SettingsRow {
                            Layout.fillWidth: true
                            title: qsTr("ReplayGain")
                            description: SettingsController.replayGain === 0
                                ? qsTr("Off: songs play as loud as they were made.")
                                : SettingsController.replayGain === 1
                                  ? qsTr("Track: every song about equally loud, using the loudness stored in your files (songs without it play as they are).")
                                  : qsTr("Album: albums keep their own ups and downs, while albums are about equally loud.")
                        }

                        Flow {
                            Layout.fillWidth: true
                            spacing: 6

                            Repeater {
                                model: [qsTr("Off"), qsTr("Track"), qsTr("Album")]

                                delegate: PillButton {
                                    required property string modelData
                                    required property int index

                                    text: modelData
                                    implicitHeight: 32
                                    active: SettingsController.replayGain === index
                                    onClicked: SettingsController.replayGain = index
                                }
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

                // ------------------------------------------------------------ Equalizer
                SettingsPage {
                    id: eqPage
                    title: qsTr("Equalizer")

                    // dB for the bands 31 Hz ... 16 kHz
                    readonly property var presets: [
                        { id: "flat", name: qsTr("Flat"), gains: [0, 0, 0, 0, 0, 0, 0, 0, 0, 0] },
                        { id: "bass", name: qsTr("Bass boost"), gains: [6, 5, 4, 2, 0, 0, 0, 0, 0, 0] },
                        { id: "treble", name: qsTr("Treble boost"), gains: [0, 0, 0, 0, 0, 1, 2, 4, 5, 6] },
                        { id: "vocal", name: qsTr("Vocal"), gains: [-2, -2, -1, 1, 3, 4, 3, 1, 0, -1] },
                        { id: "rock", name: qsTr("Rock"), gains: [4, 3, 2, 0, -1, -1, 1, 2, 3, 4] },
                        { id: "pop", name: qsTr("Pop"), gains: [-1, 0, 2, 3, 4, 3, 1, 0, -1, -1] },
                        { id: "electronic", name: qsTr("Electronic"), gains: [5, 4, 1, 0, -2, 1, 0, 1, 4, 5] },
                        { id: "hiphop", name: qsTr("Hip-hop"), gains: [5, 4, 2, 3, -1, -1, 1, 0, 2, 3] },
                        { id: "jazz", name: qsTr("Jazz"), gains: [3, 2, 1, 2, -1, -1, 0, 1, 2, 3] },
                        { id: "classical", name: qsTr("Classical"), gains: [4, 3, 2, 1, -1, -1, 0, 2, 3, 4] },
                        { id: "acoustic", name: qsTr("Acoustic"), gains: [4, 3, 2, 1, 1, 1, 2, 2, 3, 2] }
                    ]
                    readonly property var bandLabels: ["31", "62", "125", "250", "500", "1k", "2k", "4k", "8k", "16k"]

                    SettingsCard {
                        title: qsTr("Equalizer")

                        SettingsRow {
                            Layout.fillWidth: true
                            title: qsTr("Equalizer")
                            description: qsTr("Shape the sound with 10 bands. Boosting doesn't lower the volume: only peaks that would distort are turned down for a moment. Changes are heard at once.")

                            SettingsSwitch {
                                checked: SettingsController.eqEnabled
                                onToggled: SettingsController.eqEnabled = checked
                            }
                        }

                        Flow {
                            Layout.fillWidth: true
                            spacing: 6

                            Repeater {
                                model: eqPage.presets

                                delegate: PillButton {
                                    required property var modelData

                                    text: modelData.name
                                    implicitHeight: 32
                                    active: SettingsController.eqPreset === modelData.id
                                    onClicked: {
                                        SettingsController.setEqPreset(modelData.id, modelData.gains)
                                        SettingsController.eqEnabled = true
                                    }
                                }
                            }

                            PillButton {
                                visible: SettingsController.eqPreset === "custom"
                                text: qsTr("Custom")
                                implicitHeight: 32
                                active: true
                            }
                        }

                        // The 10 bands. Double-click a slider to set it back to 0.
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.topMargin: Theme.spacing
                            spacing: 0
                            opacity: SettingsController.eqEnabled ? 1 : 0.5

                            Behavior on opacity { NumberAnimation { duration: Theme.anim } }

                            Repeater {
                                model: 10

                                delegate: EqBand {
                                    required property int index

                                    Layout.fillWidth: true
                                    label: eqPage.bandLabels[index]
                                    value: SettingsController.eqGains[index]
                                    onMoved: (db) => {
                                        SettingsController.setEqGain(index, db)
                                        SettingsController.eqEnabled = true
                                    }
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Hz · ±12 dB. Double-click a slider to set it back to 0.")
                                color: Theme.textMuted
                                font.pixelSize: Theme.fontSizeSmall
                                wrapMode: Text.WordWrap
                            }

                            PillButton {
                                iconName: "refresh"
                                iconSize: 11
                                text: qsTr("Reset")
                                implicitHeight: 32
                                onClicked: SettingsController.setEqPreset("flat", [0, 0, 0, 0, 0, 0, 0, 0, 0, 0])
                            }
                        }
                    }
                }

                // ------------------------------------------------------------ Appearance
                SettingsPage {
                    title: qsTr("Appearance")

                    SettingsCard {
                        title: qsTr("Theme")

                        SettingsRow {
                            Layout.fillWidth: true
                            title: qsTr("Colors")
                            description: qsTr("System follows your desktop's light or dark setting.")
                        }

                        Flow {
                            Layout.fillWidth: true
                            spacing: 6

                            Repeater {
                                model: [
                                    { id: "system", name: qsTr("System"), icon: "settings" },
                                    { id: "dark", name: qsTr("Dark"), icon: "moon" },
                                    { id: "light", name: qsTr("Light"), icon: "sun" }
                                ]

                                delegate: PillButton {
                                    required property var modelData

                                    iconName: modelData.icon
                                    iconSize: 11
                                    text: modelData.name
                                    implicitHeight: 32
                                    active: SettingsController.themeMode === modelData.id
                                    onClicked: SettingsController.themeMode = modelData.id
                                }
                            }
                        }
                    }

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
                        title: qsTr("Music folders")

                        SettingsRow {
                            Layout.fillWidth: true
                            description: qsTr("Your library is every song in these folders (and the folders inside them). Removing a folder leaves the files where they are.")
                        }

                        Repeater {
                            model: SettingsController.libraryFolders

                            delegate: Rectangle {
                                id: folderRow

                                required property string modelData

                                Layout.fillWidth: true
                                implicitHeight: 40
                                radius: Theme.radius
                                color: Theme.fill

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 4
                                    spacing: Theme.spacing

                                    Icon {
                                        name: "folder"
                                        size: 12
                                        color: Theme.accent
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: folderRow.modelData
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
                                        ToolTip.text: qsTr("Remove from the library (the files stay)")
                                        onClicked: LocalLibraryController.removeFolder(folderRow.modelData)
                                    }
                                }
                            }
                        }

                        PillButton {
                            iconName: "folderPlus"
                            iconSize: 12
                            text: qsTr("Add folder…")
                            implicitHeight: 32
                            filled: SettingsController.libraryFolders.length === 0
                            onClicked: root.addFolderRequested()
                        }
                    }

                    SettingsCard {
                        title: qsTr("Excluded folders")

                        SettingsRow {
                            Layout.fillWidth: true
                            description: qsTr("Folders inside your music folders that are left out (e.g. ringtones, recordings).")
                        }

                        Repeater {
                            model: SettingsController.libraryExcluded

                            delegate: Rectangle {
                                id: excludedRow

                                required property string modelData

                                Layout.fillWidth: true
                                implicitHeight: 40
                                radius: Theme.radius
                                color: Theme.fill

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
                                color: Theme.fillSoft

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
                            title: qsTr("Autoplay similar songs")
                            description: qsTr("After a song, an album or a playlist, keep playing what YouTube Music plays next (a Mix). Playing YouTube songs never adds them to the Queue.")

                            SettingsSwitch {
                                checked: SettingsController.ytAutoplay
                                onToggled: SettingsController.ytAutoplay = checked
                            }
                        }

                        SettingsRow {
                            Layout.fillWidth: true
                            title: qsTr("Continue with a Mix when the Queue ends")
                            description: qsTr("If the Queue's last song is from YouTube, its Mix plays after it. The Queue itself stays as it is.")

                            SettingsSwitch {
                                checked: SettingsController.ytQueueMix
                                onToggled: SettingsController.ytQueueMix = checked
                            }
                        }

                        SettingsRow {
                            Layout.fillWidth: true
                            title: qsTr("Audio quality")
                            description: SettingsController.ytQuality === SettingsController.YtAuto
                                         ? qsTr("The best sound, but the smallest streams on a metered connection (e.g. a phone hotspot).")
                                         : SettingsController.ytQuality === SettingsController.YtBest
                                           ? qsTr("Always the best sound YouTube Music has (up to 256 kbps with Premium).")
                                           : qsTr("Always the smallest streams, to save data.")
                        }

                        Flow {
                            Layout.fillWidth: true
                            spacing: 6

                            Repeater {
                                model: [
                                    { value: SettingsController.YtAuto, name: qsTr("Auto") },
                                    { value: SettingsController.YtBest, name: qsTr("Best") },
                                    { value: SettingsController.YtLowest, name: qsTr("Lowest") }
                                ]

                                delegate: PillButton {
                                    required property var modelData

                                    text: modelData.name
                                    implicitHeight: 32
                                    active: SettingsController.ytQuality === modelData.value
                                    onClicked: SettingsController.ytQuality = modelData.value
                                }
                            }
                        }

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
    // One equalizer band: its value (dB), a vertical slider filled from 0 dB
    // up or down, and its frequency. Double-click: back to 0.
    component EqBand: ColumnLayout {
        id: band

        property string label
        property real value: 0
        signal moved(real db)

        spacing: 6

        // (Labels fill the column, so the bands can share the row's width.)
        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: (band.value > 0 ? "+" : "") + Math.round(band.value)
            color: band.value !== 0 ? Theme.accent : Theme.textMuted
            font.pixelSize: Theme.fontSizeSmall
            font.weight: Font.DemiBold
        }

        Slider {
            id: slider
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredHeight: 160
            Layout.preferredWidth: 28
            orientation: Qt.Vertical
            from: -12
            to: 12
            stepSize: 1
            snapMode: Slider.SnapAlways
            value: band.value
            hoverEnabled: true
            focusPolicy: Qt.NoFocus
            onMoved: band.moved(value)

            TapHandler {
                onDoubleTapped: band.moved(0)
            }

            background: Item {
                x: slider.leftPadding + slider.availableWidth / 2 - 3
                y: slider.topPadding
                width: 6
                height: slider.availableHeight

                Rectangle {
                    anchors.fill: parent
                    radius: 3
                    color: Theme.border
                }

                // From the 0 dB mark to the handle
                Rectangle {
                    readonly property real zero: parent.height / 2
                    readonly property real at: slider.visualPosition * parent.height
                    width: parent.width
                    y: Math.min(zero, at)
                    height: Math.abs(at - zero)
                    radius: 3
                    color: slider.hovered || slider.pressed ? Theme.accent : Theme.text
                    opacity: 0.85

                    Behavior on color { ColorAnimation { duration: Theme.anim } }
                }

                // 0 dB mark
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: parent.height / 2 - 1
                    width: 14
                    height: 2
                    radius: 1
                    color: Theme.textDim
                }
            }

            handle: Rectangle {
                readonly property real size: slider.pressed ? 16 : slider.hovered ? 14 : 12

                x: slider.leftPadding + slider.availableWidth / 2 - width / 2
                y: slider.topPadding + slider.visualPosition * (slider.availableHeight - height)
                implicitWidth: size
                implicitHeight: size
                radius: width / 2
                color: Theme.text

                Behavior on implicitWidth { NumberAnimation { duration: Theme.animFast; easing.type: Theme.easing } }
                Behavior on implicitHeight { NumberAnimation { duration: Theme.animFast; easing.type: Theme.easing } }
            }
        }

        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: band.label
            color: Theme.textMuted
            font.pixelSize: Theme.fontSizeSmall
        }
    }

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
        color: Theme.fillSoft

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
