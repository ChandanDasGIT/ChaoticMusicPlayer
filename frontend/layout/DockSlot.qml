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
    default property alias content: swipeFlick.flickableData

    // Swipe the panel toward its outer edge to put it away (like a phone drawer):
    // it follows the pointer and springs back unless swiped far or fast enough.
    // swipeAnywhere: also over the panel's lists (they keep vertical scrolling and
    // clicks: only a sideways drag swipes). Otherwise only where nothing below
    // takes the press, e.g. the header, so rows can keep their own swipes.
    property bool swipeEnabled: false
    property bool swipeAnywhere: true
    signal swipedAway()

    readonly property int outward: leftSide ? -1 : 1
    // How far the panel is pulled toward its edge: by a drag over the lists
    // (swipeFlick's overshoot: dragged past its edge, a Flickable reports half
    // the distance) or by a drag / touchpad swipe elsewhere (swipeOffset).
    property real swipeOffset: 0
    readonly property real flickPull: outward < 0 ? Math.min(0, -2 * swipeFlick.horizontalOvershoot)
                                                  : Math.max(0, -2 * swipeFlick.horizontalOvershoot)
    readonly property real pulled: swipeOffset + flickPull
    readonly property bool swiping: swipeDrag.active || wheelSwipe.swiping

    function swipeTo(dx) {
        swipeOffset = outward < 0 ? Math.min(0, dx) : Math.max(0, dx)
    }
    function settleSwipe(fast) {
        if (fast || Math.abs(pulled) > width * 0.3)
            swipedAway()
        swipeOffset = 0
    }

    Behavior on swipeOffset {
        enabled: !slot.swiping
        NumberAnimation { duration: Theme.anim; easing.type: Theme.easing }
    }

    property real openness: docked ? 1 : 0
    property real fullWidth: share // the panel's width while opening / closing

    Behavior on openness {
        NumberAnimation { duration: Theme.animPanel; easing.type: Theme.easingPanel }
    }

    visible: docked || openness > 0 // at once when docking, so its content can take the focus
    clip: openness < 1 || pulled !== 0
    opacity: openness

    SplitView.minimumWidth: openness < 1 ? 0 : minimumWidth
    SplitView.preferredWidth: share * openness

    // Back to the shared width. Dragging the divider replaces the width binding;
    // this sets it again.
    function equalize() {
        if (!docked)
            return
        fullWidth = Qt.binding(() => slot.share)
        SplitView.preferredWidth = Qt.binding(() => slot.share * slot.openness)
    }

    onDockedChanged: {
        if (docked) {
            equalize()
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
        x: (slot.leftSide ? slot.width - width : 0) + slot.pulled
        opacity: 1 - 0.6 * Math.min(1, Math.abs(slot.pulled) / Math.max(1, slot.width))

        // Sideways-only: lists inside keep their presses and vertical scrolling;
        // once a drag goes sideways it takes over. Its content stays put; the
        // overshoot pulls the whole panel along instead (and springs back by
        // itself). Like the pages of a SwipeView.
        Flickable {
            id: swipeFlick
            anchors.fill: parent
            contentWidth: width
            contentHeight: height
            interactive: slot.swipeEnabled && slot.swipeAnywhere
            flickableDirection: Flickable.HorizontalFlick
            boundsBehavior: Flickable.DragOverBounds
            boundsMovement: Flickable.StopAtBounds
            onDraggingChanged: {
                if (!dragging && slot.swipeEnabled)
                    slot.settleSwipe(-horizontalVelocity * slot.outward > 800)
            }

            // Two-finger touchpad swipe. Here (on the content) it comes before
            // the Flickable itself; lists keep vertical scrolling, as they take
            // those wheel events before they get here.
            WheelHandler {
                id: wheelSwipe

                property bool swiping: false
                property real total: 0

                enabled: slot.swipeEnabled
                acceptedDevices: PointerDevice.TouchPad
                target: null
                onWheel: (event) => {
                    const dx = event.pixelDelta.x !== 0 ? event.pixelDelta.x : event.angleDelta.x / 8
                    const dy = event.pixelDelta.y !== 0 ? event.pixelDelta.y : event.angleDelta.y / 8
                    if (Math.abs(dx) <= Math.abs(dy))
                        return
                    if (!swiping)
                        total = 0
                    swiping = true
                    total += dx
                    slot.swipeTo(total)
                    wheelSettle.restart()
                }
            }
        }

        Timer {
            id: wheelSettle
            interval: 160
            onTriggered: {
                wheelSwipe.swiping = false
                slot.settleSwipe(false)
            }
        }

        // Mouse / touch swipe where swipeFlick doesn't take it: below the
        // content, so only presses nothing else takes arrive (e.g. the header).
        Item {
            anchors.fill: parent
            z: -1

            DragHandler {
                id: swipeDrag

                enabled: slot.swipeEnabled && !slot.swipeAnywhere
                grabPermissions: PointerHandler.TakeOverForbidden // rows' own swipes and presses win
                target: null
                xAxis.enabled: true
                yAxis.enabled: false
                onTranslationChanged: if (active) slot.swipeTo(translation.x)
                onActiveChanged: if (!active) slot.settleSwipe(centroid.velocity.x * slot.outward > 800)
            }
        }
    }
}
