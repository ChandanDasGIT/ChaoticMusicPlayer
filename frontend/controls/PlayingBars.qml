pragma ComponentBehavior: Bound

import QtQuick

// Three little bars that bounce while music plays ("now playing" cue) and
// settle low when paused.
Row {
    id: root

    property bool playing: false
    property color color: Theme.accent
    property real barWidth: 3

    spacing: 2
    height: 14

    Repeater {
        model: 3

        delegate: Rectangle {
            id: bar

            required property int index
            // Different speeds so they don't move in lockstep.
            readonly property int period: [420, 560, 480][index]
            property real level: 0.35

            y: root.height - height // grow upwards from the bottom
            width: root.barWidth
            height: Math.max(3, root.height * level)
            radius: width / 2
            color: root.color

            SequentialAnimation on level {
                running: root.playing
                loops: Animation.Infinite
                NumberAnimation { to: 1.0; duration: bar.period; easing.type: Easing.InOutSine }
                NumberAnimation { to: 0.25; duration: bar.period; easing.type: Easing.InOutSine }
                onRunningChanged: if (!running) bar.level = 0.35
            }

            Behavior on level {
                enabled: !root.playing
                NumberAnimation { duration: Theme.anim }
            }
        }
    }
}
