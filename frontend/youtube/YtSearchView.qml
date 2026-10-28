pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Search tab: search YouTube Music for songs (no account needed). While you
// type, suggested searches appear under the field (arrows + Enter, or a
// click, pick one) and the results below follow once you pause typing.
Item {
    id: root

    property int highlighted: -1     // the suggestion picked with the arrow keys
    property bool suggestionsDismissed: false // until the text is edited again

    function focusField() {
        field.forceActiveFocus()
        field.selectAll()
    }
    // Search now (Enter or a picked suggestion); the dropdown closes until the next edit.
    function commit(query) {
        suggestTimer.stop()
        liveTimer.stop()
        suggestionsDismissed = true
        highlighted = -1
        YouTubeController.search(query)
    }

    // Spaced out so typing doesn't send a request per key (YouTube throttles busy clients).
    Timer {
        id: suggestTimer
        interval: 180
        onTriggered: YouTubeController.suggest(field.text)
    }
    Timer {
        id: liveTimer
        interval: 600
        onTriggered: if (field.text.trim().length >= 2) YouTubeController.search(field.text)
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
            rightPadding: 16
            placeholderText: qsTr("Search songs on YouTube Music")
            placeholderTextColor: Theme.textMuted
            color: Theme.text
            font.pixelSize: Theme.fontSize
            selectByMouse: true
            onAccepted: {
                if (root.highlighted >= 0)
                    text = YouTubeController.suggestions[root.highlighted]
                root.commit(text)
            }
            onTextEdited: {
                root.suggestionsDismissed = false
                root.highlighted = -1
                suggestTimer.restart()
                liveTimer.restart()
            }

            Keys.onDownPressed: (event) => {
                event.accepted = suggestions.shown
                if (suggestions.shown)
                    root.highlighted = Math.min(root.highlighted + 1, Math.min(YouTubeController.suggestions.length, 5) - 1)
            }
            Keys.onUpPressed: (event) => {
                event.accepted = suggestions.shown
                if (suggestions.shown)
                    root.highlighted = Math.max(root.highlighted - 1, -1)
            }
            // Esc closes the dropdown first (otherwise it reaches the panel, which closes).
            Keys.onEscapePressed: (event) => {
                event.accepted = suggestions.shown
                root.suggestionsDismissed = true
                root.highlighted = -1
            }

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


        // Suggested searches, inline so the results stay visible below them.
        Rectangle {
            id: suggestions

            readonly property bool shown: field.activeFocus && !root.suggestionsDismissed && field.text.trim() !== ""
                                          && YouTubeController.suggestions.length > 0

            Layout.fillWidth: true
            Layout.leftMargin: Theme.padding
            Layout.rightMargin: Theme.padding
            Layout.preferredHeight: shown ? suggestionColumn.implicitHeight + 12 : 0
            visible: Layout.preferredHeight > 0
            opacity: shown ? 1 : 0
            clip: true
            radius: Theme.radiusLarge
            color: Theme.surfaceAlt
            border.color: Theme.border

            Behavior on Layout.preferredHeight { NumberAnimation { duration: Theme.anim; easing.type: Theme.easing } }
            Behavior on opacity { NumberAnimation { duration: Theme.animFast } }

            Column {
                id: suggestionColumn
                x: 6
                y: 6
                width: parent.width - 12

                Repeater {
                    model: YouTubeController.suggestions.slice(0, 5)

                    delegate: Rectangle {
                        id: suggestion

                        required property string modelData
                        required property int index

                        width: suggestionColumn.width
                        height: 34
                        radius: Theme.radius
                        color: root.highlighted === index || suggestionHover.hovered ? Theme.hover : "transparent"

                        HoverHandler {
                            id: suggestionHover
                            cursorShape: Qt.PointingHandCursor
                        }
                        TapHandler {
                            onTapped: {
                                field.text = suggestion.modelData
                                root.commit(suggestion.modelData)
                            }
                        }

                        Icon {
                            id: suggestionIcon
                            anchors.left: parent.left
                            anchors.leftMargin: 12
                            anchors.verticalCenter: parent.verticalCenter
                            name: "search"
                            size: 11
                            color: Theme.textMuted
                        }

                        Label {
                            anchors.left: suggestionIcon.right
                            anchors.leftMargin: 12
                            anchors.right: parent.right
                            anchors.rightMargin: 12
                            anchors.verticalCenter: parent.verticalCenter
                            text: suggestion.modelData
                            color: Theme.text
                            font.pixelSize: Theme.fontSize
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }

        ListView {
            id: list
            populate: ListPopulate {}
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
