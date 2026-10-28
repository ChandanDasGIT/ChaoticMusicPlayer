pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Search tab: search YouTube Music for songs (no account needed).
Item {
    id: root

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spacing

        TextField {
            id: field
            Layout.fillWidth: true
            Layout.leftMargin: Theme.padding
            Layout.rightMargin: Theme.padding
            Layout.topMargin: Theme.spacing
            implicitHeight: 44
            leftPadding: 42
            rightPadding: 16
            placeholderText: qsTr("Search songs on YouTube Music")
            placeholderTextColor: Theme.textMuted
            color: Theme.text
            font.pixelSize: Theme.fontSize
            selectByMouse: true
            onAccepted: YouTubeController.search(text)

            background: Rectangle {
                radius: height / 2
                color: field.activeFocus ? "#24ffffff" : "#14ffffff"
                border.width: field.activeFocus ? 1 : 0
                border.color: Theme.accent

                Behavior on color { ColorAnimation { duration: Theme.animFast } }
            }

            Icon {
                anchors.left: parent.left
                anchors.leftMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                name: "search"
                size: 13
                color: field.activeFocus ? Theme.accent : Theme.textMuted
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: YouTubeController.searchResults
            boundsBehavior: Flickable.StopAtBounds
            opacity: YouTubeController.searching ? 0.4 : 1
            ScrollBar.vertical: ScrollBar {}

            Behavior on opacity { NumberAnimation { duration: Theme.anim } }

            delegate: YtTrackRow {
                required property var modelData
                song: modelData
                x: 4
                width: ListView.view.width - 8
            }

            YtStatus {
                anchors.centerIn: parent
                width: parent.width - 2 * Theme.padding
                loading: YouTubeController.searching && list.count === 0
                loadingText: qsTr("Searching…")
                errorText: YouTubeController.searchError
                emptyText: !YouTubeController.searching && list.count === 0 && field.text.trim() === ""
                           ? qsTr("Search YouTube Music. No account needed: play results, add them to your playlist, or save them.")
                           : ""
                onRetry: YouTubeController.search(field.text)
            }
        }
    }
}
