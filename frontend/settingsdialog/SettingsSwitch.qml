import QtQuick
import QtQuick.Controls

// On/off switch: a pill track with a sliding knob.
AbstractButton {
    id: root

    checkable: true
    hoverEnabled: true
    focusPolicy: Qt.NoFocus
    implicitWidth: 46
    implicitHeight: 26
    opacity: enabled || Util.openDialogs > 0 ? 1 : 0.4

    background: Rectangle {
        radius: height / 2
        color: root.checked ? Theme.accent : root.hovered ? Theme.switchTrackHover : Theme.switchTrack

        Behavior on color { ColorAnimation { duration: Theme.anim } }
    }

    contentItem: Item {
        Rectangle {
            readonly property real size: root.pressed ? 22 : 18

            width: size
            height: 18
            radius: height / 2
            y: (parent.height - height) / 2
            x: root.checked ? parent.width - width - 4 : 4
            color: root.checked ? Theme.background : Theme.text

            Behavior on x { NumberAnimation { duration: Theme.anim; easing.type: Theme.easing } }
            Behavior on width { NumberAnimation { duration: Theme.animFast } }
            Behavior on color { ColorAnimation { duration: Theme.anim } }
        }
    }
}
