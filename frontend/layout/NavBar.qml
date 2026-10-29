import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The floating bar at the bottom of the window: Home, Queue, YouTube Music,
// Library and Settings, each an icon over its name. What is open is marked with a pill
// behind its icon. Wide windows get a compact, centered bar; phone-size ones a
// full-width bar.
Rectangle {
    id: root

    property bool homeOpen: false
    property bool queueOpen: false
    property bool youtubeOpen: false
    property bool libraryOpen: false
    property bool settingsOpen: false

    signal homeClicked()
    signal queueClicked()
    signal youtubeClicked()
    signal libraryClicked()
    signal settingsClicked()

    implicitHeight: 64
    radius: Theme.radiusCard
    color: Theme.miniPlayer
    border.color: Theme.border

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.spacing
        anchors.rightMargin: Theme.spacing
        spacing: 0

        NavButton {
            iconName: "home"
            label: qsTr("Home")
            active: root.homeOpen
            tip: qsTr("Home")
            onClicked: root.homeClicked()
        }
        NavButton {
            iconName: "listMusic"
            label: qsTr("Queue")
            active: root.queueOpen
            tip: qsTr("Queue (Ctrl+1)")
            onClicked: root.queueClicked()
        }
        NavButton {
            iconName: "cloud"
            label: qsTr("YouTube")
            active: root.youtubeOpen
            tip: qsTr("YouTube Music (Ctrl+2)")
            onClicked: root.youtubeClicked()
        }
        NavButton {
            iconName: "compactDisc"
            label: qsTr("Library")
            active: root.libraryOpen
            tip: qsTr("Library (Ctrl+3)")
            onClicked: root.libraryClicked()
        }
        NavButton {
            iconName: "settings"
            label: qsTr("Settings")
            active: root.settingsOpen
            tip: qsTr("Settings (Ctrl+,)")
            onClicked: root.settingsClicked()
        }
    }

    // An icon in a pill (filled while active) over its name.
    component NavButton: AbstractButton {
        id: button

        property string iconName
        property string label
        property string tip
        property bool active: false

        Layout.fillWidth: true
        Layout.fillHeight: true
        hoverEnabled: true
        focusPolicy: Qt.NoFocus
        ToolTip.visible: hovered && tip !== ""
        ToolTip.text: tip
        ToolTip.delay: 600

        contentItem: Item {
            Rectangle {
                id: pill
                anchors.horizontalCenter: parent.horizontalCenter
                y: 8
                width: 56
                height: 28
                radius: height / 2
                color: button.active ? Theme.accentSoft : button.hovered ? Theme.hover : "transparent"
                scale: button.pressed ? 0.92 : 1

                Behavior on color { ColorAnimation { duration: Theme.anim } }
                Behavior on scale { NumberAnimation { duration: Theme.animFast; easing.type: Theme.easing } }

                Icon {
                    anchors.centerIn: parent
                    name: button.iconName
                    size: 14
                    color: button.active ? Theme.accent : Theme.text
                }
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: pill.bottom
                anchors.topMargin: 3
                text: button.label
                color: button.active ? Theme.accent : Theme.textMuted
                font.pixelSize: Theme.fontSizeSmall
                font.weight: button.active ? Font.DemiBold : Font.Normal
            }
        }
    }
}
