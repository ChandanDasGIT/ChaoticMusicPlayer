pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// App settings, grouped into cards: Playback, Library, Lyrics, YouTube, Appearance.
// Every change is saved immediately (SettingsController).
Popup {
    id: root

    // Asks Main for the folder picker (it lives in the main window).
    signal excludeFolderRequested(string startFolder)

    anchors.centerIn: Overlay.overlay
    width: Math.min(Overlay.overlay ? Overlay.overlay.width - 2 * Theme.padding : 600, 600)
    height: Math.min(Overlay.overlay ? Overlay.overlay.height - 2 * Theme.padding : 700, 700)
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

        // Title bar
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

        ScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true

            ColumnLayout {
                width: scroll.availableWidth
                spacing: Theme.padding

                // ---------------------------------------------------------- Playback
                SettingsCard {
                    title: qsTr("Playback")

                    SettingsRow {
                        Layout.fillWidth: true
                        title: qsTr("Restore playlist on startup")
                        description: qsTr("Open the app with the playlist you had last time. Off: start with an empty playlist.")

                        SettingsSwitch {
                            checked: SettingsController.restorePlaylist
                            onToggled: SettingsController.restorePlaylist = checked
                        }
                    }

                    SettingsRow {
                        Layout.fillWidth: true
                        title: qsTr("Keep playing after closing the window")
                        description: qsTr("Closing the window hides it and the music goes on. Open the app again (or use your desktop's media controls) to bring it back; quit with the power button or Ctrl+Q.")

                        SettingsSwitch {
                            checked: SettingsController.backgroundPlayback
                            onToggled: SettingsController.backgroundPlayback = checked
                        }
                    }

                    SettingsRow {
                        Layout.fillWidth: true
                        title: qsTr("Output device")
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

                    SettingsRow {
                        Layout.fillWidth: true
                        title: qsTr("Volume step")
                        description: qsTr("How much one mouse-wheel notch over the volume slider changes the volume.")

                        RowLayout {
                            spacing: 4

                            PillButton {
                                iconName: "minus"
                                iconSize: 11
                                implicitWidth: 34
                                implicitHeight: 30
                                enabled: SettingsController.volumeStep > 1
                                onClicked: SettingsController.volumeStep -= 1
                            }

                            Label {
                                Layout.preferredWidth: 44
                                horizontalAlignment: Text.AlignHCenter
                                text: SettingsController.volumeStep + "%"
                                color: Theme.text
                                font.pixelSize: Theme.fontSize
                                font.features: { "tnum": 1 }
                            }

                            PillButton {
                                iconName: "plus"
                                iconSize: 11
                                implicitWidth: 34
                                implicitHeight: 30
                                enabled: SettingsController.volumeStep < 25
                                onClicked: SettingsController.volumeStep += 1
                            }
                        }
                    }

                    SettingsRow {
                        Layout.fillWidth: true
                        title: qsTr("Show remaining time")
                        description: qsTr("The time on the right of the seek bar counts down instead of showing the length.")

                        SettingsSwitch {
                            checked: SettingsController.showRemainingTime
                            onToggled: SettingsController.showRemainingTime = checked
                        }
                    }

                    SettingsRow {
                        Layout.fillWidth: true
                        title: qsTr("Crossfade")
                        description: qsTr("The end of a song fades into the next one. Skipping fades quickly too. Songs that run into each other on an album stay gapless.")

                        RowLayout {
                            spacing: Theme.spacing

                            PillButton {
                                iconName: "minus"
                                iconSize: 11
                                implicitWidth: 34
                                implicitHeight: 30
                                enabled: SettingsController.crossfadeSeconds > 0
                                onClicked: SettingsController.crossfadeSeconds -= 1
                            }

                            Label {
                                Layout.preferredWidth: 44
                                horizontalAlignment: Text.AlignHCenter
                                text: SettingsController.crossfadeSeconds > 0
                                      ? qsTr("%1 s").arg(SettingsController.crossfadeSeconds) : qsTr("Off")
                                color: Theme.text
                                font.pixelSize: Theme.fontSize
                                font.features: { "tnum": 1 }
                            }

                            PillButton {
                                iconName: "plus"
                                iconSize: 11
                                implicitWidth: 34
                                implicitHeight: 30
                                enabled: SettingsController.crossfadeSeconds < 12
                                onClicked: SettingsController.crossfadeSeconds += 1
                            }
                        }
                    }
                }

                // ---------------------------------------------------------- Album art
                SettingsCard {
                    title: qsTr("Album art")

                    SettingsRow {
                        Layout.fillWidth: true
                        title: qsTr("Find missing covers online")
                        description: qsTr("When a song without a cover plays, look one up for its album. Every source below is asked and the best-quality image is shown. Covers are kept in the app's cache; your files are not changed.")

                        SettingsSwitch {
                            checked: SettingsController.coversOnline
                            onToggled: SettingsController.coversOnline = checked
                        }
                    }

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

                // ---------------------------------------------------------- Library
                SettingsCard {
                    title: qsTr("Library")

                    SettingsRow {
                        Layout.fillWidth: true
                        title: qsTr("Excluded folders")
                        description: qsTr("Folders inside your library folders whose songs are left out of the library (e.g. ringtones, recordings). Library folders are added in the Library panel.")
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

                // ---------------------------------------------------------- Lyrics
                SettingsCard {
                    title: qsTr("Lyrics")

                    SettingsRow {
                        Layout.fillWidth: true
                        title: qsTr("Sources")
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

                    SettingsRow {
                        Layout.fillWidth: true
                        title: qsTr("Synced lyrics by default")
                        description: qsTr("Highlight lines in time with the music when timing is available; otherwise show the plain text.")

                        SettingsSwitch {
                            checked: SettingsController.lyricsSyncedByDefault
                            onToggled: SettingsController.lyricsSyncedByDefault = checked
                        }
                    }

                    SettingsRow {
                        Layout.fillWidth: true
                        title: qsTr("Script")
                        description: qsTr("English letters: the same words in the song's own language, written in English letters (e.g. a Hindi song as \u201cAankhen band kar ke\u201d), not a translation.")
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

                // ---------------------------------------------------------- YouTube
                SettingsCard {
                    title: qsTr("YouTube Music")

                    SettingsRow {
                        Layout.fillWidth: true
                        title: qsTr("Look up track info in the background")
                        description: qsTr("Find your local songs on YouTube Music and show that info in Track details.")

                        SettingsSwitch {
                            checked: SettingsController.ytBackgroundLookup
                            onToggled: SettingsController.ytBackgroundLookup = checked
                        }
                    }

                    SettingsRow {
                        Layout.fillWidth: true
                        title: qsTr("Sign in through your browser")
                        description: qsTr("Pick the browser where you're signed in to YouTube Music; yt-dlp uses that session to read your library. The app never sees your password. Close that browser if reading fails.")
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

                    SettingsRow {
                        Layout.fillWidth: true
                        title: qsTr("yt-dlp")
                        description: YouTubeController.available
                                     ? qsTr("Installed (version %1). Used to play YouTube songs; keep it up to date.").arg(YouTubeController.ytDlpVersion)
                                     : qsTr("Not found. Install yt-dlp (e.g. sudo dnf install yt-dlp) to play YouTube songs.")
                    }
                }

                // ---------------------------------------------------------- Appearance
                SettingsCard {
                    title: qsTr("Appearance")

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

                    SettingsRow {
                        Layout.fillWidth: true
                        title: qsTr("Visualizer")
                        description: qsTr("Show the music's frequencies under the volume bar.")

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

                Item { implicitHeight: Theme.spacing }
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
}
