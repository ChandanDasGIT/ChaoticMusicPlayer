import QtQuick
import QtQuick.Layouts

// Compact player pinned to the bottom of a panel that covers the main player:
// a progress line, cover, title / artist and previous / play-pause / next.
// Tapping the cover or title asks to go back to the full player. With
// acceptsDrops, songs dragged from the Library or YouTube can be dropped on it
// (added to the Queue); it then also shows up while nothing is loaded.
Rectangle {
    id: root

    property bool active: true // false: never shown (e.g. the panel is docked next to the player)
    property bool acceptsDrops: false
    readonly property bool shown: active && (PlayerController.hasTrack || (acceptsDrops && SongDrag.active))

    signal openPlayerRequested()

    readonly property real fullHeight: 64
    implicitHeight: shown ? fullHeight : 0
    radius: Theme.radiusLarge
    color: Theme.miniPlayer
    border.color: Theme.border
    clip: true
    opacity: shown ? 1 : 0
    visible: implicitHeight > 0.5

    // In step with the panels docking beside it (DockSlot), so they move together.
    Behavior on implicitHeight { NumberAnimation { duration: Theme.animPanel; easing.type: Theme.easingPanel } }
    Behavior on opacity { NumberAnimation { duration: Theme.anim } }

    // Progress along the top edge
    Rectangle {
        x: root.radius / 2
        y: 0
        height: 2
        radius: 1
        width: (root.width - root.radius) * (PlayerController.duration > 0
                                              ? Math.min(1, PlayerController.position / PlayerController.duration) : 0)
        color: Theme.accent
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 8
        spacing: Theme.padding

        // Cover + text: tap to open the full player
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            HoverHandler {
                cursorShape: Qt.PointingHandCursor
            }

            TapHandler {
                onTapped: root.openPlayerRequested()
            }

            Rectangle {
                id: cover
                anchors.verticalCenter: parent.verticalCenter
                width: 44
                height: 44
                radius: Theme.radius
                color: Theme.surfaceAlt
                clip: true

                Icon {
                    anchors.centerIn: parent
                    name: "music"
                    size: 14
                    color: Theme.textMuted
                }

                Image {
                    anchors.fill: parent
                    source: Util.coverSource(PlayerController.source, PlaylistController.currentThumbnail)
                    sourceSize: Qt.size(88, 88)
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    opacity: status === Image.Ready ? 1 : 0

                    Behavior on opacity { NumberAnimation { duration: Theme.anim } }
                }
            }

            ColumnLayout {
                anchors.left: cover.right
                anchors.leftMargin: Theme.padding
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: 1

                FadeLabel {
                    Layout.fillWidth: true
                    text: PlaylistController.currentTitle !== "" ? PlaylistController.currentTitle : PlayerController.title
                    color: Theme.text
                    font.pixelSize: Theme.fontSize
                    font.weight: Font.DemiBold
                }

                FadeLabel {
                    Layout.fillWidth: true
                    visible: text !== ""
                    text: PlaylistController.currentSubtitle
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeSmall
                }
            }
        }

        PillButton {
            iconName: "prev"
            iconSize: 12
            implicitWidth: 36
            implicitHeight: 36
            transparentIdle: true
            onClicked: PlaylistController.previous()
        }

        PillButton {
            filled: true
            implicitWidth: 44
            implicitHeight: 44
            onClicked: PlaylistController.playPause()

            contentItem: Item {
                Icon {
                    anchors.centerIn: parent
                    anchors.horizontalCenterOffset: 1
                    name: "play"
                    size: 14
                    color: Theme.background
                    opacity: PlayerController.playing ? 0 : 1
                    rotation: PlayerController.playing ? 90 : 0

                    Behavior on opacity { NumberAnimation { duration: Theme.anim } }
                    Behavior on rotation { NumberAnimation { duration: Theme.anim; easing.type: Theme.easing } }
                }

                Icon {
                    anchors.centerIn: parent
                    name: "pause"
                    size: 14
                    color: Theme.background
                    opacity: PlayerController.playing ? 1 : 0
                    rotation: PlayerController.playing ? 0 : -90

                    Behavior on opacity { NumberAnimation { duration: Theme.anim } }
                    Behavior on rotation { NumberAnimation { duration: Theme.anim; easing.type: Theme.easing } }
                }
            }
        }

        PillButton {
            iconName: "next"
            iconSize: 12
            implicitWidth: 36
            implicitHeight: 36
            transparentIdle: true
            enabled: PlaylistController.hasNext
            onClicked: PlaylistController.next()
        }
    }

    DropHint {
        accepts: root.acceptsDrops && root.active
        radius: root.radius
        onDropped: SongDrag.deliver("", -1)
    }
}
