import QtQuick
import QtQuick.Controls

// Small pill with the track's format, e.g. "44.1 kHz · 941 kbps · FLAC".
// A click asks for the track's details.
Rectangle {
    id: root

    property string text: PlayerController.qualityText

    signal clicked()

    implicitWidth: label.implicitWidth + 20
    implicitHeight: label.implicitHeight + 6
    radius: height / 2
    color: chipHover.hovered ? Theme.hover : Theme.fill
    opacity: text !== "" ? 1 : 0
    visible: opacity > 0

    Behavior on opacity { NumberAnimation { duration: Theme.anim } }
    Behavior on color { ColorAnimation { duration: Theme.animFast } }

    HoverHandler {
        id: chipHover
        cursorShape: Qt.PointingHandCursor
    }
    TapHandler {
        margin: 18 // small to look at, but a finger needn't land exactly on it
        onTapped: root.clicked()
    }

    ToolTip.visible: chipHover.hovered
    ToolTip.text: qsTr("Track details")

    Label {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: chipHover.hovered ? Theme.text : Theme.textMuted
        font.pixelSize: Theme.fontSizeSmall
        font.features: { "tnum": 1 }
    }
}
