import QtQuick
import QtQuick.Controls

// A panel's tabs on one line: they share its width evenly and scroll sideways
// where they don't fit, with an arrow on each side that has more to show.
Item {
    id: root

    property var tabs: [] // [{ icon, label }]
    property int current: -1
    // Hovered or moving: a drawer holding it must not take a drag as a swipe.
    readonly property bool busy: hover.hovered || list.dragging || list.moving

    signal tabPicked(int index)

    implicitHeight: 34

    ListView {
        id: list
        anchors.fill: parent
        orientation: ListView.Horizontal
        spacing: 6
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        flickDeceleration: Theme.flickDeceleration
        maximumFlickVelocity: Theme.flickVelocity
        synchronousDrag: true
        currentIndex: root.current
        onCurrentIndexChanged: if (currentIndex >= 0) positionViewAtIndex(currentIndex, ListView.Contain)
        model: root.tabs

        HoverHandler {
            id: hover
        }

        delegate: PillButton {
            required property var modelData
            required property int index

            width: Math.max(implicitWidth, (list.width - 3 * list.spacing) / 4)
            iconName: modelData.icon
            iconSize: 11
            text: modelData.label
            implicitHeight: 34
            active: root.current === index
            onClicked: root.tabPicked(index)
        }
    }

    ScrollHint {
        anchors.left: parent.left
        iconName: "chevronLeftSmall"
        shown: !list.atXBeginning
        onClicked: list.positionViewAtBeginning()
    }

    ScrollHint {
        anchors.right: parent.right
        iconName: "chevronRight"
        shown: !list.atXEnd
        onClicked: list.positionViewAtEnd()
    }

    component ScrollHint: AbstractButton {
        id: hint

        property string iconName
        property bool shown: false

        anchors.verticalCenter: parent.verticalCenter
        width: 26
        height: 26
        focusPolicy: Qt.NoFocus
        enabled: shown
        opacity: shown ? 1 : 0

        Behavior on opacity { NumberAnimation { duration: Theme.animFast } }

        background: Rectangle {
            radius: width / 2
            color: Theme.surfaceAlt
            border.color: Theme.border
        }

        contentItem: Item {
            Icon {
                anchors.centerIn: parent
                name: hint.iconName
                size: 10
                color: Theme.text
            }
        }
    }
}
