import QtQuick

// Press and hold, then drag: hands payload() to SongDrag, so the songs can be
// dropped on the Queue or the player. Fills its parent (a song row, an album
// tile...) underneath its content, so its buttons and taps keep working.
Item {
    id: root

    // Asked for when the drag starts: { items, browseId, title, subtitle, cover }
    property var payload: function() { return {} }
    property bool dragging: false

    anchors.fill: parent
    z: -1

    TapHandler {
        id: hold
        longPressThreshold: 0.4
        gesturePolicy: TapHandler.DragThreshold // moving first scrolls the list instead
        onLongPressed: {
            root.dragging = true
            SongDrag.start(root, root.payload(), hold.point.scenePosition)
        }
    }

    // Follows the held pointer (a passive grab: it sees every move) and drops on release.
    PointHandler {
        id: tracker
        onTranslationChanged: if (root.dragging) SongDrag.move(tracker.point.scenePosition)
        onActiveChanged: {
            if (!active && root.dragging) {
                root.dragging = false
                SongDrag.finish()
            }
        }
    }
}
