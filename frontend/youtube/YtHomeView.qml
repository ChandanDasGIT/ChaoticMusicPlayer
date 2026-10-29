pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Home tab: YouTube Music's home shelves (playlists / albums). Clicking a card
// opens its songs over the tabs (YtCollectionView).
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

    Flickable {
        anchors.fill: parent
        contentHeight: shelvesColumn.implicitHeight + Theme.padding
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        ColumnLayout {
            id: shelvesColumn
            width: parent.width
            spacing: Theme.spacing

            Repeater {
                model: YouTubeController.homeShelves

                delegate: YtShelf {
                    required property var modelData
                    Layout.fillWidth: true
                    title: modelData.title
                    items: modelData.items
                    onBusyChanged: root.shelvesBusy += busy ? 1 : -1
                    Component.onDestruction: if (busy) root.shelvesBusy -= 1
                }
            }
        }
    }

    // Over the view, not inside the shelves' scroll area: with no shelves that
    // area is only a few pixels tall and would clip the message.
    YtStatus {
        anchors.centerIn: parent
        width: parent.width - 2 * Theme.padding
        loading: YouTubeController.homeLoading
        loadingText: qsTr("Loading YouTube Music…")
        errorText: YouTubeController.homeError
        emptyText: YouTubeController.homeShelves.length === 0 && !YouTubeController.homeLoading
                   && YouTubeController.homeError === "" ? qsTr("Nothing here right now.") : ""
        onRetry: YouTubeController.loadHome()
    }
}
