pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Home tab: YouTube Music's home shelves (playlists / albums). Clicking a card
// opens its songs right here, with a back button.
Item {
    id: root

    // The pointer is over (or dragging) a sideways shelf: a drawer holding this
    // view must not take the drag as a swipe to close.
    readonly property bool swipeBlocked: shelvesBusy > 0
    property int shelvesBusy: 0 // shelves hovered or moving

    Component.onCompleted: {
        if (YouTubeController.homeShelves.length === 0)
            YouTubeController.loadHome()
    }

    // ---- Shelves ------------------------------------------------------------
    Flickable {
        anchors.fill: parent
        contentHeight: shelvesColumn.implicitHeight + Theme.padding
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        opacity: YouTubeController.collectionOpen ? 0 : 1
        visible: opacity > 0
        ScrollBar.vertical: ScrollBar {}

        Behavior on opacity { NumberAnimation { duration: Theme.anim } }

        ColumnLayout {
            id: shelvesColumn
            width: parent.width
            spacing: Theme.spacing

            Repeater {
                model: YouTubeController.homeShelves

                delegate: ColumnLayout {
                    id: shelf

                    required property var modelData

                    Layout.fillWidth: true
                    spacing: Theme.spacing

                    Label {
                        Layout.leftMargin: Theme.padding
                        Layout.topMargin: Theme.spacing
                        text: shelf.modelData.title
                        color: Theme.text
                        font.pixelSize: Theme.fontSizeLarge
                        font.weight: Font.DemiBold
                    }

                    ListView {
                        // Counted in root.shelvesBusy while true.
                        readonly property bool busy: shelfHover.hovered || dragging || moving

                        Layout.fillWidth: true
                        Layout.preferredHeight: 196
                        orientation: ListView.Horizontal
                        spacing: Theme.padding
                        leftMargin: Theme.padding
                        rightMargin: Theme.padding
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds
                        model: shelf.modelData.items

                        onBusyChanged: root.shelvesBusy += busy ? 1 : -1
                        Component.onDestruction: if (busy) root.shelvesBusy -= 1

                        HoverHandler {
                            id: shelfHover
                        }

                        delegate: Item {
                            id: card

                            required property var modelData

                            width: 140
                            height: 196
                            scale: cardHover.hovered ? 1.03 : 1

                            Behavior on scale { NumberAnimation { duration: Theme.anim; easing.type: Theme.easing } }

                            HoverHandler {
                                id: cardHover
                                cursorShape: Qt.PointingHandCursor
                            }

                            TapHandler {
                                onTapped: YouTubeController.openCollection(card.modelData.browseId, card.modelData.title)
                            }

                            // Hold and drag the whole playlist onto the Queue or the player
                            // (its songs are fetched on drop).
                            DragSource {
                                payload: function() {
                                    return { browseId: card.modelData.browseId, title: card.modelData.title,
                                             subtitle: card.modelData.subtitle || "", cover: card.modelData.thumbnail || "" }
                                }
                            }

                            YtThumbnail {
                                id: art
                                width: 140
                                height: 140
                                source: card.modelData.thumbnail
                                hue: card.modelData.title.length % 10 / 10
                                iconName: "listMusic"
                            }

                            Label {
                                anchors.top: art.bottom
                                anchors.topMargin: 6
                                width: parent.width
                                text: card.modelData.title
                                color: Theme.text
                                font.pixelSize: Theme.fontSize
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }

                            Label {
                                anchors.top: art.bottom
                                anchors.topMargin: 24
                                width: parent.width
                                text: card.modelData.subtitle
                                color: Theme.textMuted
                                font.pixelSize: Theme.fontSizeSmall
                                elide: Text.ElideRight
                                maximumLineCount: 2
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }
        }

    }

    // Over the view, not inside the shelves' scroll area: with no shelves that
    // area is only a few pixels tall and would clip the message.
    YtStatus {
        anchors.centerIn: parent
        width: parent.width - 2 * Theme.padding
        visible: !YouTubeController.collectionOpen && (loading || errorText !== "" || emptyText !== "")
        loading: YouTubeController.homeLoading
        loadingText: qsTr("Loading YouTube Music…")
        errorText: YouTubeController.homeError
        emptyText: YouTubeController.homeShelves.length === 0 && !YouTubeController.homeLoading
                   && YouTubeController.homeError === "" ? qsTr("Nothing here right now.") : ""
        onRetry: YouTubeController.loadHome()
    }

    // ---- An opened playlist / album -------------------------------------------
    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spacing
        opacity: YouTubeController.collectionOpen ? 1 : 0
        visible: opacity > 0

        Behavior on opacity { NumberAnimation { duration: Theme.anim } }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.spacing
            Layout.rightMargin: Theme.padding
            Layout.topMargin: Theme.spacing
            spacing: Theme.spacing

            PillButton {
                iconName: "back"
                iconSize: 12
                implicitWidth: 38
                implicitHeight: 34
                onClicked: YouTubeController.closeCollection()
            }

            Label {
                Layout.fillWidth: true
                text: YouTubeController.collectionTitle
                color: Theme.text
                font.pixelSize: Theme.fontSizeLarge
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            PillButton {
                visible: YouTubeController.collectionSongs.length > 0
                filled: true
                iconName: "play"
                iconSize: 11
                text: qsTr("Play all")
                implicitHeight: 34
                onClicked: YouTubeController.playList(YouTubeController.collectionSongs, 0, YouTubeController.collectionTitle)
            }
        }

        ListView {
            populate: ListPopulate {}
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: YouTubeController.collectionSongs
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            // A song here plays the album / playlist from that song.
            delegate: YtTrackRow {
                required property var modelData
                required property int index
                song: modelData
                listSongs: YouTubeController.collectionSongs
                listIndex: index
                listTitle: YouTubeController.collectionTitle
                x: 4
                width: ListView.view.width - 8
            }

            YtStatus {
                anchors.centerIn: parent
                width: parent.width - 2 * Theme.padding
                loading: YouTubeController.collectionLoading
                loadingText: qsTr("Loading songs…")
                errorText: YouTubeController.collectionError
                onRetry: YouTubeController.closeCollection()
            }
        }
    }
}
