pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Home: a greeting for the time of day, "Your Mix" (your most played songs,
// shuffled), and what you've been listening to: recently played, most played
// this month, albums to jump back into, albums recently added to the library,
// a YouTube Mix recommended from the YouTube song you play most, and the whole
// history ("See all", by day). Most of it comes from the private listening
// history.
Rectangle {
    id: root

    property bool showPinButton: true
    property bool pinned: false
    property bool showCloseButton: false
    property bool showingHistory: false // the whole history instead of the overview

    signal pinToggled()
    signal closeRequested()

    readonly property bool hasHistory: HistoryController.count > 0

    // The greeting follows the clock.
    property int hour: new Date().getHours()
    readonly property string greeting: hour >= 5 && hour < 12 ? qsTr("Good morning")
                                     : hour >= 12 && hour < 17 ? qsTr("Good afternoon")
                                     : qsTr("Good evening")
    Timer {
        interval: 60 * 1000
        running: root.visible
        repeat: true
        triggeredOnStart: true
        onTriggered: root.hour = new Date().getHours()
    }

    color: Theme.panelShade // softens the cover tint behind it
    radius: Theme.radiusCard
    border.color: Theme.border

    // Pin / close, top right
    Row {
        z: 2
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: Theme.spacing
        spacing: 2

        ControlButton {
            visible: root.showPinButton
            iconName: "thumbtack"
            iconSize: 14
            active: root.pinned
            iconColor: root.pinned ? Theme.accent : Theme.textMuted
            rotation: root.pinned ? 0 : 45
            ToolTip.visible: hovered
            ToolTip.text: root.pinned ? qsTr("Unpin panel") : qsTr("Pin panel")
            onClicked: root.pinToggled()

            Behavior on rotation { NumberAnimation { duration: Theme.anim; easing.type: Theme.easing } }
        }

        ControlButton {
            visible: root.showCloseButton
            iconName: "close"
            iconSize: 13
            iconColor: Theme.textMuted
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Close")
            onClicked: root.closeRequested()
        }
    }

    // ---- The overview
    Flickable {
        id: overview
        anchors.fill: parent
        anchors.margins: 1
        anchors.bottomMargin: 6
        clip: true
        contentHeight: sections.implicitHeight + 2 * Theme.padding
        boundsBehavior: Flickable.StopAtBounds
        opacity: root.showingHistory ? 0 : 1
        visible: opacity > 0
        ScrollBar.vertical: ScrollBar {}

        Behavior on opacity { NumberAnimation { duration: Theme.anim } }

        ColumnLayout {
            id: sections
            x: Theme.padding + 4
            y: Theme.padding
            width: overview.width - 2 * (Theme.padding + 4)
            spacing: Theme.padding

            // Greeting
            Label {
                Layout.fillWidth: true
                Layout.topMargin: Theme.padding * 2
                Layout.rightMargin: 60 // clear of the pin / close buttons
                text: root.greeting
                color: Theme.text
                font.pixelSize: 38
                font.weight: Font.Bold
                font.letterSpacing: -0.5
                wrapMode: Text.WordWrap
            }

            // "Your Mix": a big round shuffle button
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: Theme.spacing
                Layout.bottomMargin: Theme.spacing
                spacing: Theme.padding

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Your Mix")
                        color: Theme.text
                        font.pixelSize: Theme.fontSizeTitle
                        font.weight: Font.DemiBold
                    }

                    Label {
                        Layout.fillWidth: true
                        text: root.hasHistory ? qsTr("The songs you play most, shuffled")
                            : LocalLibraryController.songCount > 0 ? qsTr("Songs from your library, shuffled")
                            : qsTr("Play some music, and what you love shows up here")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSize
                        wrapMode: Text.WordWrap
                    }
                }

                AbstractButton {
                    id: mixButton
                    implicitWidth: 72
                    implicitHeight: 72
                    enabled: root.hasHistory || LocalLibraryController.songCount > 0
                    opacity: enabled ? 1 : 0.4
                    hoverEnabled: true
                    focusPolicy: Qt.NoFocus
                    scale: pressed ? 0.92 : hovered ? 1.05 : 1
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Play Your Mix")
                    onClicked: HistoryController.playMix()

                    Behavior on scale { NumberAnimation { duration: Theme.anim; easing.type: Theme.easingBack } }

                    background: Rectangle {
                        radius: width / 2
                        color: Theme.accent
                    }
                    contentItem: Item {
                        Icon {
                            anchors.centerIn: parent
                            name: "shuffle"
                            size: 22
                            color: Theme.background
                        }
                    }
                }
            }

            // Recently played
            SectionTitle {
                visible: root.hasHistory
                text: qsTr("Recently played")
            }
            ListView {
                Layout.fillWidth: true
                Layout.preferredHeight: 172
                visible: root.hasHistory
                orientation: ListView.Horizontal
                spacing: Theme.padding
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: HistoryController.recent

                delegate: CoverCard {
                    required property var modelData
                    required property int index

                    title: modelData.title
                    subtitle: modelData.artist
                    cover: Util.coverSource(modelData.path, modelData.thumbnail)
                    dragItems: [modelData.item]
                    onClicked: HistoryController.playList("recent", index)
                }
            }

            // Most played this month
            SectionTitle {
                visible: HistoryController.mostPlayed.length > 0
                text: qsTr("Most played this month")
            }
            Column {
                Layout.fillWidth: true
                visible: HistoryController.mostPlayed.length > 0
                spacing: 2

                Repeater {
                    model: HistoryController.mostPlayed.slice(0, 5)

                    delegate: HomeSongRow {
                        required property var modelData
                        required property int index

                        width: parent.width
                        song: modelData
                        note: modelData.plays === 1 ? qsTr("1 play") : qsTr("%1 plays").arg(modelData.plays)
                        onClicked: HistoryController.playList("mostPlayed", index)
                    }
                }
            }

            // Albums to jump back into
            SectionTitle {
                visible: HistoryController.recentAlbums.length > 0
                text: qsTr("Jump back in")
            }
            ListView {
                Layout.fillWidth: true
                Layout.preferredHeight: 172
                visible: HistoryController.recentAlbums.length > 0
                orientation: ListView.Horizontal
                spacing: Theme.padding
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: HistoryController.recentAlbums

                delegate: CoverCard {
                    required property var modelData

                    title: modelData.album
                    subtitle: modelData.artist
                    cover: Util.coverSource(modelData.path, "")
                    iconName: "compactDisc"
                    onClicked: LocalLibraryController.playAlbumOf(modelData.path)
                }
            }

            // Albums newest in the library
            SectionTitle {
                visible: LocalLibraryController.recentlyAdded.length > 0
                text: qsTr("Recently added")
            }
            ListView {
                Layout.fillWidth: true
                Layout.preferredHeight: 172
                visible: LocalLibraryController.recentlyAdded.length > 0
                orientation: ListView.Horizontal
                spacing: Theme.padding
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: LocalLibraryController.recentlyAdded

                delegate: CoverCard {
                    required property var modelData

                    title: modelData.album
                    subtitle: modelData.artist
                    cover: Util.coverSource(modelData.coverPath, "")
                    iconName: "compactDisc"
                    dragItems: LocalLibraryController.albumItems(modelData.key)
                    onClicked: LocalLibraryController.playAlbum(modelData.key)
                }
            }

            // Recommended: the Mix of the YouTube song played most
            SectionTitle {
                visible: recommended.visible
                text: qsTr("Recommended for you")
            }
            Rectangle {
                id: recommended
                Layout.fillWidth: true
                implicitHeight: 84
                visible: HistoryController.favoriteYouTube.videoId !== undefined
                radius: Theme.radiusLarge
                color: recommendedHover.hovered ? Theme.hover : Theme.fillSoft

                Behavior on color { ColorAnimation { duration: Theme.animFast } }

                HoverHandler {
                    id: recommendedHover
                    cursorShape: Qt.PointingHandCursor
                }
                TapHandler {
                    onTapped: YouTubeController.play(HistoryController.favoriteYouTube)
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: Theme.padding

                    YtThumbnail {
                        Layout.preferredWidth: 64
                        Layout.preferredHeight: 64
                        source: HistoryController.favoriteYouTube.thumbnail || ""
                        hue: (HistoryController.favoriteYouTube.title || "").length % 10 / 10
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Mix – %1").arg(HistoryController.favoriteYouTube.title || "")
                            color: Theme.text
                            font.pixelSize: Theme.fontSize
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.fillWidth: true
                            text: qsTr("What YouTube Music plays after the song you play most")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSizeSmall
                            wrapMode: Text.WordWrap
                            maximumLineCount: 2
                        }
                    }

                    Icon {
                        name: "play"
                        size: 14
                        color: Theme.accent
                    }
                }
            }

            // History: the latest few, and the whole of it
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: Theme.spacing
                visible: root.hasHistory

                Label {
                    Layout.fillWidth: true
                    text: qsTr("History")
                    color: Theme.text
                    font.pixelSize: Theme.fontSizeLarge
                    font.weight: Font.DemiBold
                }

                PillButton {
                    text: qsTr("See all")
                    iconName: "chevronRight"
                    iconSize: 10
                    implicitHeight: 30
                    transparentIdle: true
                    onClicked: root.showingHistory = true
                }
            }
            Column {
                Layout.fillWidth: true
                visible: root.hasHistory
                spacing: 2

                Repeater {
                    model: HistoryController.entries.slice(0, 4)

                    delegate: HomeSongRow {
                        required property var modelData
                        required property int index

                        width: parent.width
                        song: modelData
                        note: modelData.day === qsTr("Today") ? modelData.clock : modelData.day
                        removable: true
                        onClicked: HistoryController.playList("entries", index)
                        onRemoveRequested: HistoryController.removeEntry(index)
                    }
                }
            }

            // Nothing listened to yet
            Label {
                Layout.fillWidth: true
                Layout.topMargin: Theme.padding
                visible: !root.hasHistory
                text: SettingsController.historyEnabled
                      ? qsTr("Songs you listen to show up here: recently played, your most played, albums to jump back into and recommendations.")
                      : qsTr("The listening history is off (Settings → General), so Home has nothing to show yet.")
                color: Theme.textMuted
                font.pixelSize: Theme.fontSize
                wrapMode: Text.WordWrap
            }
        }
    }

    // ---- The whole history, by day
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 1
        anchors.bottomMargin: 6
        spacing: Theme.spacing
        opacity: root.showingHistory ? 1 : 0
        visible: opacity > 0
        transform: Translate { x: root.showingHistory ? 0 : 48; Behavior on x { NumberAnimation { duration: Theme.animSlow; easing.type: Theme.easingPanel } } }

        Behavior on opacity { NumberAnimation { duration: Theme.anim } }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.spacing
            Layout.topMargin: Theme.spacing
            Layout.rightMargin: 90 // clear of the pin / close buttons
            spacing: Theme.padding

            PillButton {
                iconName: "back"
                iconSize: 12
                implicitWidth: 36
                implicitHeight: 36
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Back to Home")
                onClicked: root.showingHistory = false
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("History")
                color: Theme.text
                font.pixelSize: Theme.fontSizeTitle
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
        }

        ListView {
            id: historyList
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: Theme.spacing
            Layout.rightMargin: Theme.spacing
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            spacing: 2
            model: HistoryController.entries
            ScrollBar.vertical: ScrollBar {}

            // A day's heading above its first play.
            delegate: Column {
                id: entry

                required property var modelData
                required property int index
                readonly property bool firstOfDay: index === 0 || HistoryController.entries[index - 1].day !== modelData.day

                width: historyList.width

                Label {
                    visible: entry.firstOfDay
                    leftPadding: 8
                    topPadding: entry.index === 0 ? 4 : Theme.padding
                    bottomPadding: 4
                    text: entry.modelData.day
                    color: Theme.accent
                    font.pixelSize: Theme.fontSizeSmall
                    font.weight: Font.DemiBold
                }

                HomeSongRow {
                    width: parent.width
                    song: entry.modelData
                    note: entry.modelData.clock
                    removable: true
                    onClicked: HistoryController.playList("entries", entry.index)
                    onRemoveRequested: HistoryController.removeEntry(entry.index)
                }
            }

            Label {
                anchors.centerIn: parent
                visible: historyList.count === 0
                text: qsTr("No songs listened to yet.")
                color: Theme.textMuted
                font.pixelSize: Theme.fontSize
            }
        }
    }

    component SectionTitle: Label {
        Layout.fillWidth: true
        Layout.topMargin: Theme.spacing
        color: Theme.text
        font.pixelSize: Theme.fontSizeLarge
        font.weight: Font.DemiBold
    }

    // A cover with a title and subtitle under it (songs, albums). Hold and
    // drag to drop it on the Queue or the player (when it has `dragItems`).
    component CoverCard: Item {
        id: card

        property string title
        property string subtitle
        property string cover
        property string iconName: "music"
        property var dragItems: null
        signal clicked()

        width: 128
        height: 172
        scale: cardHover.hovered ? 1.03 : 1

        Behavior on scale { NumberAnimation { duration: Theme.anim; easing.type: Theme.easing } }

        HoverHandler {
            id: cardHover
            cursorShape: Qt.PointingHandCursor
        }
        TapHandler {
            onTapped: card.clicked()
        }
        DragSource {
            enabled: card.dragItems !== null
            payload: function() {
                return { items: card.dragItems, title: card.title, subtitle: card.subtitle, cover: card.cover }
            }
        }

        Rectangle {
            id: art
            width: 128
            height: 128
            radius: Theme.radiusLarge
            color: Theme.surfaceAlt
            clip: true

            Icon {
                anchors.centerIn: parent
                name: card.iconName
                size: 26
                color: Theme.textDim
            }

            Image {
                anchors.fill: parent
                source: card.cover
                sourceSize: Qt.size(256, 256)
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                opacity: status === Image.Ready ? 1 : 0

                Behavior on opacity { NumberAnimation { duration: Theme.anim } }
            }
        }

        Label {
            anchors.top: art.bottom
            anchors.topMargin: 6
            width: parent.width
            text: card.title
            color: Theme.text
            font.pixelSize: Theme.fontSize
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }

        Label {
            anchors.top: art.bottom
            anchors.topMargin: 24
            width: parent.width
            text: card.subtitle
            color: Theme.textMuted
            font.pixelSize: Theme.fontSizeSmall
            elide: Text.ElideRight
        }
    }
}
