pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Search tab: search YouTube Music for songs (no account needed). The results
// follow once you pause typing; "×" clears the field. With the field empty,
// your recent searches show (each can be removed, or all at once). A search
// is remembered when you press Enter or use one of its results.
Item {
    id: root

    readonly property string query: field.text.trim()
    readonly property bool showHistory: query === "" && SettingsController.ytSearchHistory.length > 0

    function focusField() {
        field.forceActiveFocus()
        field.selectAll()
    }
    // Search now (Enter, or a recent search picked) and remember it.
    function commit(text) {
        liveTimer.stop()
        field.text = text
        YouTubeController.search(text)
        SettingsController.rememberYtSearch(text)
    }
    function clear() {
        liveTimer.stop()
        field.clear()
        YouTubeController.search("")
        field.forceActiveFocus()
    }

    // Spaced out so typing doesn't send a request per key (YouTube throttles busy clients).
    Timer {
        id: liveTimer
        interval: 600
        onTriggered: if (root.query.length >= 2) YouTubeController.search(field.text)
    }

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
            rightPadding: clearButton.visible ? 44 : 16
            placeholderText: qsTr("Search songs on YouTube Music")
            placeholderTextColor: Theme.textMuted
            color: Theme.text
            font.pixelSize: Theme.fontSize
            selectByMouse: true
            onAccepted: if (root.query !== "") root.commit(root.query)
            onTextEdited: {
                if (root.query === "") {
                    liveTimer.stop()
                    YouTubeController.search("")
                } else {
                    liveTimer.restart()
                }
            }

            background: Rectangle {
                radius: height / 2
                color: field.activeFocus ? Theme.fillStrong : Theme.fill
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

            // Clears the field (and the results) at once
            ControlButton {
                id: clearButton
                anchors.right: parent.right
                anchors.rightMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                visible: field.text !== ""
                iconName: "close"
                iconSize: 11
                iconColor: Theme.textMuted
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Clear")
                onClicked: root.clear()
            }
        }

        // ---- Recent searches (the field is empty)
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.showHistory
            spacing: 4

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.padding + 4
                Layout.rightMargin: Theme.padding

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Recent searches")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeSmall
                    font.weight: Font.DemiBold
                }

                PillButton {
                    text: qsTr("Clear all")
                    iconName: "trash"
                    iconSize: 10
                    implicitHeight: 28
                    transparentIdle: true
                    onClicked: SettingsController.clearYtSearchHistory()
                }
            }

            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.leftMargin: Theme.spacing
                Layout.rightMargin: Theme.spacing
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                spacing: 2
                model: SettingsController.ytSearchHistory
                ScrollBar.vertical: ScrollBar {}

                delegate: Rectangle {
                    id: recent

                    required property string modelData

                    width: ListView.view.width
                    height: 40
                    radius: Theme.radius
                    color: recentHover.hovered ? Theme.hover : "transparent"

                    Behavior on color { ColorAnimation { duration: Theme.animFast } }

                    HoverHandler {
                        id: recentHover
                        cursorShape: Qt.PointingHandCursor
                    }
                    TapHandler {
                        onTapped: root.commit(recent.modelData)
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 14
                        anchors.rightMargin: 4
                        spacing: 12

                        Icon {
                            name: "search"
                            size: 11
                            color: Theme.textMuted
                        }

                        Label {
                            Layout.fillWidth: true
                            text: recent.modelData
                            color: Theme.text
                            font.pixelSize: Theme.fontSize
                            elide: Text.ElideRight
                        }

                        PillButton {
                            iconName: "close"
                            iconSize: 10
                            implicitWidth: 30
                            implicitHeight: 30
                            transparentIdle: true
                            opacity: recentHover.hovered ? 1 : 0.5
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Remove from recent searches")
                            onClicked: SettingsController.forgetYtSearch(recent.modelData)
                        }
                    }
                }
            }
        }

        // ---- Results
        ListView {
            id: list
            populate: ListPopulate {}
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !root.showHistory
            clip: true
            model: YouTubeController.searchResults
            boundsBehavior: Flickable.StopAtBounds
            opacity: YouTubeController.searching ? 0.4 : 1
            ScrollBar.vertical: ScrollBar {}

            Behavior on opacity { NumberAnimation { duration: Theme.anim } }

            // Using a result (play, its menu, dragging it...) remembers the search.
            PointHandler {
                onActiveChanged: if (active && root.query !== "" && list.count > 0) SettingsController.rememberYtSearch(root.query)
            }

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
                emptyText: !YouTubeController.searching && list.count === 0 && root.query === ""
                           ? qsTr("Search YouTube Music. No account needed: play results, add them to the Queue, or save them.")
                           : ""
                onRetry: YouTubeController.search(field.text)
            }
        }
    }
}
