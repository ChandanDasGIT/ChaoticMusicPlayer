import QtQuick
import QtQuick.Controls

// Small pill with the track's format, e.g. "44.1 kHz · 941 kbps · FLAC".
Rectangle {
    id: root

    property string text: PlayerController.qualityText

    implicitWidth: label.implicitWidth + 20
    implicitHeight: label.implicitHeight + 6
    radius: height / 2
    color: Theme.fill
    opacity: text !== "" ? 1 : 0
    visible: opacity > 0

    Behavior on opacity { NumberAnimation { duration: Theme.anim } }

    Label {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: Theme.textMuted
        font.pixelSize: Theme.fontSizeSmall
        font.features: { "tnum": 1 }
    }
}
