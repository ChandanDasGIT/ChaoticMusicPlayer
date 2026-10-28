import QtQuick
import QtQuick.Controls

// Stadium-shaped button with an icon and/or text.
//   filled: accent background (main action)
//   active: toggled on (accent-tinted)
// Pressing squishes it and squares the corners a little.
AbstractButton {
    id: root

    property string iconName
    property real iconSize: 16
    property bool filled: false
    property bool active: false
    property bool transparentIdle: false // no background unless hovered/active (inside a group)
    property color iconColor: filled ? Theme.background : active ? Theme.accent : Theme.text

    implicitHeight: 44
    implicitWidth: Math.max(implicitHeight, row.implicitWidth + 32)
    hoverEnabled: true
    focusPolicy: Qt.NoFocus
    opacity: enabled ? 1 : 0.35
    scale: pressed ? 0.93 : 1

    Behavior on opacity { NumberAnimation { duration: Theme.anim } }
    Behavior on scale { NumberAnimation { duration: Theme.anim; easing.type: Theme.easingBack } }

    contentItem: Item {
        Row {
            id: row
            anchors.centerIn: parent
            spacing: 8

            Icon {
                anchors.verticalCenter: parent.verticalCenter
                visible: root.iconName !== ""
                name: root.iconName
                size: root.iconSize
                color: root.iconColor

                Behavior on color { ColorAnimation { duration: Theme.anim } }
            }

            Label {
                anchors.verticalCenter: parent.verticalCenter
                visible: root.text !== ""
                text: root.text
                color: root.iconColor
                font.pixelSize: Theme.fontSize
                font.weight: Font.DemiBold
            }
        }
    }

    background: Rectangle {
        radius: root.pressed ? root.height * 0.3 : root.height / 2
        color: {
            if (root.filled)
                return root.pressed ? Qt.darker(Theme.accent, 1.15)
                     : root.hovered ? Qt.lighter(Theme.accent, 1.08) : Theme.accent
            if (root.active)
                return root.hovered ? "#4d9b8cff" : Theme.accentSoft
            if (root.pressed)
                return Theme.selection
            if (root.hovered)
                return "#1fffffff"
            return root.transparentIdle ? "transparent" : "#14ffffff"
        }

        Behavior on radius { NumberAnimation { duration: Theme.anim; easing.type: Theme.easing } }
        Behavior on color { ColorAnimation { duration: Theme.animFast } }
    }
}
