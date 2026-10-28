pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Read-only details of a music file: tags, audio properties, file info, what
// YouTube Music knows about it, and (collapsed) every raw tag. Values can be
// selected and copied.
Popup {
    id: root

    // [{ title, collapsible, fields: [{ label, value }] }]
    property var sections: []
    property string path
    property var ytInfo: ({})

    function show(filePath) {
        path = filePath
        sections = LibraryController.trackDetails(filePath)
        YtMetadataController.prioritize(filePath)
        ytInfo = YtMetadataController.infoFor(filePath)
        open()
    }

    Connections {
        target: YtMetadataController
        function onInfoChanged(changedPath) {
            if (changedPath === root.path)
                root.ytInfo = YtMetadataController.infoFor(root.path)
        }
    }

    anchors.centerIn: Overlay.overlay
    width: Math.min(Overlay.overlay ? Overlay.overlay.width - 2 * Theme.padding : 560, 560)
    height: Math.min(Overlay.overlay ? Overlay.overlay.height - 2 * Theme.padding : 640, 640)
    modal: true
    focus: true
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.anim }
        NumberAnimation { property: "scale"; from: 0.94; to: 1; duration: Theme.anim; easing.type: Theme.easing }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; to: 0; duration: Theme.animFast }
        NumberAnimation { property: "scale"; to: 0.96; duration: Theme.animFast }
    }

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radiusLarge
        border.color: Theme.border
    }

    Overlay.modal: Rectangle {
        color: "#99000000"
        Behavior on opacity { NumberAnimation { duration: Theme.anim } }
    }

    contentItem: ColumnLayout {
        spacing: 0

        // Title bar
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Theme.padding
            Layout.rightMargin: Theme.spacing

            Icon {
                name: "info"
                size: 16
                color: Theme.accent
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Track details")
                color: Theme.text
                font.pixelSize: Theme.fontSizeLarge
            }

            ControlButton {
                iconName: "close"
                iconSize: 13
                onClicked: root.close()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: Theme.border
        }

        ScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true

            ColumnLayout {
                width: scroll.availableWidth
                spacing: Theme.padding

                Item { implicitHeight: 1 }

                // Tags, Audio, File
                Repeater {
                    model: root.sections.slice(0, 3)
                    delegate: DetailsSection {
                        Layout.fillWidth: true
                        Layout.leftMargin: Theme.padding
                        Layout.rightMargin: Theme.padding
                    }
                }

                YouTubeSection {
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.padding
                    Layout.rightMargin: Theme.padding
                }

                // All tags
                Repeater {
                    model: root.sections.slice(3)
                    delegate: DetailsSection {
                        Layout.fillWidth: true
                        Layout.leftMargin: Theme.padding
                        Layout.rightMargin: Theme.padding
                    }
                }

                Item { implicitHeight: Theme.spacing }
            }
        }
    }

    component DetailsSection: ColumnLayout {
        id: section

        required property var modelData
        property bool expanded: !modelData.collapsible

        visible: modelData.fields.length > 0
        spacing: 6

        // Section heading (click to expand/collapse "All tags")
        Item {
            Layout.fillWidth: true
            implicitHeight: 24

            Row {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 6

                Icon {
                    visible: section.modelData.collapsible
                    name: section.expanded ? "chevronDown" : "chevronRight"
                    size: 10
                    color: Theme.accent
                    height: parent.height
                }

                Label {
                    text: section.modelData.title.toUpperCase()
                    color: Theme.accent
                    font.pixelSize: Theme.fontSizeSmall
                    font.bold: true
                    font.letterSpacing: 1
                }

                Label {
                    visible: section.modelData.collapsible
                    text: "(" + section.modelData.fields.length + ")"
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeSmall
                }
            }

            MouseArea {
                anchors.fill: parent
                enabled: section.modelData.collapsible
                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                onClicked: section.expanded = !section.expanded
            }
        }

        Repeater {
            model: section.expanded ? section.modelData.fields : []

            delegate: RowLayout {
                id: field

                required property var modelData

                Layout.fillWidth: true
                spacing: Theme.padding

                Label {
                    Layout.preferredWidth: 130
                    Layout.alignment: Qt.AlignTop
                    text: field.modelData.label
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSize
                    wrapMode: Text.WrapAnywhere
                }

                TextEdit {
                    Layout.fillWidth: true
                    text: field.modelData.value
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.WrapAnywhere
                    color: Theme.text
                    selectionColor: Theme.selection
                    selectedTextColor: Theme.text
                    font.pixelSize: Theme.fontSize
                }
            }
        }
    }

    // "From YouTube Music": the matched song, or why there is none (yet).
    component YouTubeSection: ColumnLayout {
        id: yt

        readonly property string lookupState: root.ytInfo.state !== undefined ? root.ytInfo.state : ""

        spacing: 6

        Item {
            Layout.fillWidth: true
            implicitHeight: 24

            Row {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 6

                Icon {
                    name: "cloud"
                    size: 10
                    color: Theme.accent
                    height: parent.height
                }

                Label {
                    text: qsTr("From YouTube Music").toUpperCase()
                    color: Theme.accent
                    font.pixelSize: Theme.fontSizeSmall
                    font.bold: true
                    font.letterSpacing: 1
                }

                Label {
                    visible: yt.lookupState === "matched" && root.ytInfo.close === true
                    text: qsTr("(close match)")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeSmall
                }
            }
        }

        // Not matched (yet)
        Label {
            id: statusLabel
            Layout.fillWidth: true
            visible: yt.lookupState !== "matched"
            wrapMode: Text.WordWrap
            text: yt.lookupState === "pending" ? qsTr("Looking it up on YouTube Music…")
                : yt.lookupState === "nomatch" ? qsTr("No match on YouTube Music. The title and the album or an artist must match.")
                : yt.lookupState === "error" ? qsTr("Couldn't reach YouTube Music. It will try again.")
                : yt.lookupState === "off" ? qsTr("Turned off in Settings.")
                : ""
            color: Theme.textMuted
            font.pixelSize: Theme.fontSize

            SequentialAnimation on opacity {
                running: yt.lookupState === "pending"
                loops: Animation.Infinite
                NumberAnimation { to: 0.4; duration: 700; easing.type: Easing.InOutSine }
                NumberAnimation { to: 1; duration: 700; easing.type: Easing.InOutSine }
                onRunningChanged: if (!running) statusLabel.opacity = 1
            }
        }

        // Matched
        RowLayout {
            Layout.fillWidth: true
            visible: yt.lookupState === "matched"
            spacing: Theme.padding

            Rectangle {
                Layout.alignment: Qt.AlignTop
                implicitWidth: 76
                implicitHeight: 76
                radius: Theme.radius
                color: Theme.surfaceAlt
                clip: true

                Icon {
                    anchors.centerIn: parent
                    name: "music"
                    size: 18
                    color: Theme.textMuted
                }

                Image {
                    anchors.fill: parent
                    source: yt.lookupState === "matched" ? root.ytInfo.thumbnail : ""
                    sourceSize: Qt.size(152, 152)
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    opacity: status === Image.Ready ? 1 : 0

                    Behavior on opacity { NumberAnimation { duration: Theme.anim } }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                YtField { label: qsTr("Title"); value: root.ytInfo.title || "" }
                YtField { label: qsTr("Artists"); value: root.ytInfo.artists || "" }
                YtField { label: qsTr("Album"); value: root.ytInfo.album || "" }
                YtField { label: qsTr("Year"); value: root.ytInfo.year || "" }
                YtField {
                    label: qsTr("Duration")
                    value: root.ytInfo.duration > 0 ? Theme.formatTime(root.ytInfo.duration) : ""
                }
                YtField { label: qsTr("Link"); value: root.ytInfo.url || "" }
            }
        }
    }

    component YtField: RowLayout {
        id: ytField

        property string label
        property string value

        Layout.fillWidth: true
        visible: value !== ""
        spacing: Theme.padding

        Label {
            Layout.preferredWidth: 70
            Layout.alignment: Qt.AlignTop
            text: ytField.label
            color: Theme.textMuted
            font.pixelSize: Theme.fontSize
        }

        TextEdit {
            Layout.fillWidth: true
            text: ytField.value
            readOnly: true
            selectByMouse: true
            wrapMode: TextEdit.WrapAnywhere
            color: Theme.text
            selectionColor: Theme.selection
            selectedTextColor: Theme.text
            font.pixelSize: Theme.fontSize
        }
    }
}
