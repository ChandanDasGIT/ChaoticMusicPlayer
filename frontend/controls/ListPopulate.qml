import QtQuick

// "populate" transition for lists: when a list fills (opened, new results,
// re-sorted), its rows fade in and rise slightly, one after another (only the
// first dozen are staggered, so long lists don't drag).
Transition {
    id: root

    SequentialAnimation {
        PropertyAction { property: "opacity"; value: 0 }
        PauseAnimation { duration: Math.min(root.ViewTransition.index, 12) * 22 }
        ParallelAnimation {
            NumberAnimation { property: "opacity"; to: 1; duration: Theme.anim; easing.type: Theme.easing }
            NumberAnimation {
                property: "y"
                from: root.ViewTransition.destination.y + 10
                to: root.ViewTransition.destination.y
                duration: Theme.anim
                easing.type: Theme.easing
            }
        }
    }
}
