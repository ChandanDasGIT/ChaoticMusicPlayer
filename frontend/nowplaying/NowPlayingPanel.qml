import QtQuick
import QtQuick.Controls

// Now-playing panel: album art, track info and playback controls. Track
// details (and Quit, with background playing) sit at the top right; the way to
// the other panels is the navigation bar at the bottom of the window.
Rectangle {
    id: root

    // Lyrics replace the cover and title; the seek bar and controls stay.
    property bool lyricsOpen: false
    // 0 = cover .. 1 = lyrics. The cover flies into the lyrics' mini cover
    // while the titles fade out and the lyrics rise in; closing runs it backwards.
    property real lyricsProgress: lyricsOpen ? 1 : 0
    readonly property real titlesOpacity: 1 - Math.min(1, lyricsProgress * 2)

    Behavior on lyricsProgress { NumberAnimation { duration: 460; easing.type: Easing.InOutCubic } }

    signal detailsRequested()
    signal equalizerRequested()
    signal quitRequested()

    color: "transparent" // the window's cover-tinted gradient shows through

    Row {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: Theme.spacing
        spacing: 6

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
            id: art

            // Where it flies to: the lyrics' mini cover, in this column's coordinates.
            readonly property point target: {
                root.lyricsProgress // re-evaluated on every animation step
                return lyrics.coverSlot.mapToItem(content, 0, 0)
            }

            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.max(80, Math.min(parent.width, area.height - content.otherHeight))
            height: width
            path: PlayerController.source
            thumbnail: PlaylistController.currentThumbnail
            playing: PlayerController.playing || root.lyricsOpen // full size while it flies
            // (stays "visible": the column must keep its place, or the controls jump)
            opacity: 1 - lyrics.phase(0.85, 1) // the mini cover takes over at the end
            transform: [
                Scale {
                    xScale: 1 + (lyrics.coverSlot.width / art.width - 1) * root.lyricsProgress
                    yScale: xScale
                },
                Translate {
                    x: (art.target.x - art.x) * root.lyricsProgress
                    y: (art.target.y - art.y) * root.lyricsProgress
                }
            ]
        }

        Item { id: spacer; width: 1; height: Theme.spacing }

        FadeLabel {
            id: titleLabel
            width: parent.width
            opacity: root.titlesOpacity
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
            opacity: root.titlesOpacity
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
            onDetailsRequested: root.detailsRequested()
        }

        TransportControls {
            id: transport
            anchors.horizontalCenter: parent.horizontalCenter
            lyricsOpen: root.lyricsOpen
            onLyricsToggled: root.lyricsOpen = !root.lyricsOpen
        }

        VolumeSlider {
            id: volume
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(parent.width, 290)
            onEqualizerRequested: root.equalizerRequested()
        }

        Visualizer {
            id: visualizer
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(parent.width, 260)
        }
    }

    // Fills the space of the cover and title, above the seek bar.
    LyricsView {
        id: lyrics
        x: area.x + (area.width - width) / 2
        y: area.y
        width: Math.min(area.width - 2 * Theme.padding, 640)
        height: Math.max(0, content.y + seekBar.y - Theme.padding - y)
        progress: root.lyricsProgress
        visible: progress > 0
        onCloseRequested: root.lyricsOpen = false
    }
}
