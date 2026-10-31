import QtQuick
import QtQuick.Controls

// Round icon button used by the transport and volume controls.
// Hover fades in a background, pressing "squishes" the button.
AbstractButton {
    id: root

    property string iconName
    property real iconSize: 16
    property bool active: false // toggled on (shuffle/repeat): accent-colored icon
    property bool filled: false // accent-filled circle (play/pause)
    property color iconColor: filled ? Theme.background : active ? Theme.accent : Theme.text

    implicitWidth: Math.max(iconSize * 2.2, 32)
    implicitHeight: implicitWidth
    hoverEnabled: true
    focusPolicy: Qt.NoFocus
    opacity: enabled || Util.openDialogs > 0 ? 1 : 0.35
    scale: pressed ? 0.86 : (hovered && filled ? 1.06 : 1)

    Behavior on opacity { NumberAnimation { duration: Theme.anim } }
    Behavior on scale { NumberAnimation { duration: Theme.anim; easing.type: Theme.easingBack } }

    contentItem: Icon {
        name: root.iconName
        size: root.iconSize
        color: root.iconColor

        Behavior on color { ColorAnimation { duration: Theme.anim } }
    }

    background: Item {
        // Soft glow behind the filled button
        Rectangle {
            anchors.centerIn: parent
            width: parent.width * (root.hovered ? 1.35 : 1)
            height: width
            radius: width / 2
            visible: root.filled
            color: Theme.accent
            opacity: root.hovered ? 0.22 : 0

            Behavior on width { NumberAnimation { duration: Theme.animSlow; easing.type: Theme.easing } }
            Behavior on opacity { NumberAnimation { duration: Theme.animSlow } }
        }

        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: {
                if (root.filled)
                    return root.pressed ? Qt.darker(Theme.accent, 1.15) : Theme.accent
                if (root.active)
                    return root.hovered ? Theme.selection : Theme.accentSoft
                return root.pressed ? Theme.selection : root.hovered ? Theme.hover : "transparent"
            }

            Behavior on color { ColorAnimation { duration: Theme.animFast } }
        }
    }
}
