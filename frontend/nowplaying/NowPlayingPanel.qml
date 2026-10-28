import QtQuick
import QtQuick.Controls

// Now-playing panel: album art, track info and playback controls.
Rectangle {
    id: root

    property bool showPlaylistButton: false
    // Lyrics replace the cover and title; the seek bar and controls stay.
    property bool lyricsOpen: false
    property real coverOpacity: lyricsOpen ? 0 : 1

    Behavior on coverOpacity { NumberAnimation { duration: Theme.animSlow; easing.type: Theme.easingPanel } }

    signal playlistRequested()
    signal detailsRequested()
    signal settingsRequested()
    signal youtubeRequested()
    signal libraryRequested()
    signal quitRequested()

    property bool youtubeOpen: false
    property bool libraryOpen: false

    color: "transparent" // the window's cover-tinted gradient shows through

    PillButton {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: Theme.spacing
        iconName: "list"
        iconSize: 14
        implicitWidth: 40
        implicitHeight: 40
        visible: opacity > 0
        opacity: root.showPlaylistButton ? 1 : 0
        ToolTip.visible: hovered
        ToolTip.text: qsTr("Show playlist")
        onClicked: root.playlistRequested()
    }

    Row {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: Theme.spacing
        spacing: 6

        PillButton {
            iconName: "cloud"
            iconSize: 14
            implicitWidth: 40
            implicitHeight: 40
            active: root.youtubeOpen
            ToolTip.visible: hovered
            ToolTip.text: root.youtubeOpen ? qsTr("Hide YouTube Music") : qsTr("YouTube Music")
            onClicked: root.youtubeRequested()
        }

        PillButton {
            iconName: "info"
            iconSize: 14
            implicitWidth: 40
            implicitHeight: 40
            enabled: PlayerController.hasTrack
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Track details")
            onClicked: root.detailsRequested()
        }

        PillButton {
            iconName: "settings"
            iconSize: 14
            implicitWidth: 40
            implicitHeight: 40
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Settings")
            onClicked: root.settingsRequested()
        }

        // With background playing, closing the window doesn't quit: this does.
        PillButton {
            visible: SettingsController.backgroundPlayback
            iconName: "power"
            iconSize: 14
            implicitWidth: 40
            implicitHeight: 40
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Quit (Ctrl+Q)")
            onClicked: root.quitRequested()
        }
    }

    // Everything below the top button bar.
    Item {
        id: area
        anchors.fill: parent
        anchors.topMargin: 56
        anchors.bottomMargin: Theme.padding
    }

    Column {
        id: content

        // Height of everything except the cover, so the cover takes what is left.
        readonly property real otherHeight: spacer.height + titleLabel.height + subtitleLabel.height
                                            + seekBar.height + transport.height + volume.height
                                            + 6 * spacing
                                            + (visualizer.visible ? visualizer.height + spacing : 0)

        anchors.centerIn: area
        width: Math.min(area.width - 4 * Theme.padding, 420)
        spacing: Theme.padding

        // Square, shrinks to leave room for the controls.
        AlbumArt {
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.max(80, Math.min(parent.width, area.height - content.otherHeight))
            height: width
            path: PlayerController.source
            thumbnail: PlaylistController.currentThumbnail
            playing: PlayerController.playing
            opacity: root.coverOpacity
            scale: 0.9 + 0.1 * root.coverOpacity // shrinks away as the lyrics come in
        }

        Item { id: spacer; width: 1; height: Theme.spacing }

        FadeLabel {
            id: titleLabel
            width: parent.width
            opacity: root.coverOpacity
            text: PlaylistController.currentTitle !== "" ? PlaylistController.currentTitle
                : PlayerController.hasTrack ? PlayerController.title : qsTr("Nothing playing")
            color: Theme.text
            font.pixelSize: Theme.fontSizeTitle
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
        }

        FadeLabel {
            id: subtitleLabel
            width: parent.width
            opacity: root.coverOpacity
            text: PlayerController.errorString !== "" ? PlayerController.errorString
                : PlaylistController.currentTitle !== "" ? PlaylistController.currentSubtitle
                : PlayerController.hasTrack ? "" : qsTr("Drop a folder to start")
            color: PlayerController.errorString !== "" ? Theme.error : Theme.textMuted
            font.pixelSize: Theme.fontSize
            horizontalAlignment: Text.AlignHCenter
        }

        SeekBar {
            id: seekBar
            width: parent.width
        }

        TransportControls {
            id: transport
            anchors.horizontalCenter: parent.horizontalCenter
            lyricsOpen: root.lyricsOpen
            onLyricsToggled: root.lyricsOpen = !root.lyricsOpen
            libraryOpen: root.libraryOpen
            onLibraryToggled: root.libraryRequested()
        }

        VolumeSlider {
            id: volume
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(parent.width, 220)
        }

        Visualizer {
            id: visualizer
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(parent.width, 260)
        }
    }

    // Fills the space of the cover and title, above the seek bar.
    LyricsView {
        x: area.x + (area.width - width) / 2
        y: area.y
        width: Math.min(area.width - 2 * Theme.padding, 640)
        height: Math.max(0, content.y + seekBar.y - Theme.padding - y)
        opacity: 1 - root.coverOpacity
        visible: opacity > 0
        scale: 0.96 + 0.04 * opacity
        onCloseRequested: root.lyricsOpen = false
    }
}
