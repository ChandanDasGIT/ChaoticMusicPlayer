import QtQuick
import QtQuick.Controls

// Rounded slider (used for volume). The handle grows while hovered/dragged.
Slider {
    id: control

    hoverEnabled: true
    focusPolicy: Qt.NoFocus

    background: Rectangle {
        x: control.leftPadding
        y: control.topPadding + control.availableHeight / 2 - height / 2
        width: control.availableWidth
        height: 6
        radius: 3
        color: Theme.border

        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: parent.radius
            color: control.hovered || control.pressed ? Theme.accent : Theme.text

            Behavior on color { ColorAnimation { duration: Theme.anim } }
        }
    }

    handle: Rectangle {
        readonly property real size: control.pressed ? 16 : control.hovered ? 14 : 10

        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + control.availableHeight / 2 - height / 2
        implicitWidth: size
        implicitHeight: size
        radius: width / 2
        color: Theme.text
        visible: control.enabled

        Behavior on implicitWidth { NumberAnimation { duration: Theme.animFast; easing.type: Theme.easing } }
        Behavior on implicitHeight { NumberAnimation { duration: Theme.animFast; easing.type: Theme.easing } }
    }
}
