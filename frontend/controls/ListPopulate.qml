import QtQuick

// "populate" transition for lists: when a list fills (opened, new results,
// re-sorted), its rows fade in and rise slightly, one after another (only the
// first dozen are staggered, so long lists don't drag). sideways: for rows of
// cards, which slide in from the right instead.
Transition {
    id: root

    property bool sideways: false

    SequentialAnimation {
        PropertyAction { property: "opacity"; value: 0 }
        PauseAnimation { duration: Math.min(root.ViewTransition.index, 12) * 22 }
        ParallelAnimation {
            NumberAnimation { property: "opacity"; to: 1; duration: Theme.anim; easing.type: Theme.easing }
            NumberAnimation {
                property: root.sideways ? "x" : "y"
                from: (root.sideways ? root.ViewTransition.destination.x + 28 : root.ViewTransition.destination.y + 10)
                to: root.sideways ? root.ViewTransition.destination.x : root.ViewTransition.destination.y
                duration: root.sideways ? Theme.animSlow : Theme.anim
                easing.type: Theme.easing
            }
        }
    }
}
