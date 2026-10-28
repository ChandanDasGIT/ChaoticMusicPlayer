import QtQuick
import QtQuick.Controls

// A SplitView column for a dockable panel. Docking opens its width smoothly
// (the panel slides in from its outer side and fades in, the player glides
// aside); undocking closes it the same way. The panel keeps its full width
// while the column opens or closes, so its content doesn't squeeze.
Item {
    id: slot

    property bool docked: false
    property real share: 300      // width when docked (until the user drags the divider)
    property real minimumWidth: 240
    property bool leftSide: true  // slides in from the left edge (else from the right)
    default property alias content: holder.data

    property real openness: docked ? 1 : 0
    property real fullWidth: share // the panel's width while opening / closing

    Behavior on openness {
        NumberAnimation { duration: Theme.animPanel; easing.type: Theme.easingPanel }
    }

    visible: openness > 0
    clip: openness < 1
    opacity: openness

    SplitView.minimumWidth: openness < 1 ? 0 : minimumWidth
    SplitView.preferredWidth: share * openness

    onDockedChanged: {
        // Dragging the divider replaces the width binding; set it again here.
        if (docked) {
            fullWidth = Qt.binding(() => slot.share)
            SplitView.preferredWidth = Qt.binding(() => slot.share * slot.openness)
        } else {
            const closingFrom = width
            fullWidth = closingFrom
            SplitView.preferredWidth = Qt.binding(() => closingFrom * slot.openness)
        }
    }

    Item {
        id: holder
        width: slot.openness < 1 ? Math.max(slot.width, slot.fullWidth) : slot.width
        height: slot.height
        x: slot.leftSide ? slot.width - width : 0
    }
}
