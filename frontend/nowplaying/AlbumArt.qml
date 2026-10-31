pragma ComponentBehavior: Bound

import QtQuick

// Cover art of the playing track: rounded corners (baked in by the image
// provider), soft shadow, crossfade on track change, slightly smaller while
// paused. Music-note placeholder when the file has no cover.
// Uses no shader effects, so it renders on every Qt Quick backend.
Item {
    id: root

    property string path      // audio file (or YouTube track) whose cover to show
    property string thumbnail // YouTube tracks: thumbnail URL
    property bool playing: false

    // 0 = this item's own square .. 1 = reaching past it by bleedTop / bleedSide (the
    // screen's edges): square corners, and no animation of its own.
    property real spread: 0
    property real bleedTop: 0
    property real bleedSide: 0
    readonly property bool full: spread >= 1

    readonly property string sourceUrl: {
        const url = Util.coverSource(path, thumbnail)
        if (url === "" || !full)
            return url
        const shape = Math.round(1000 * (width + 2 * bleedSide) / Math.max(1, height + bleedTop))
        return url + (url.includes("?") ? "&" : "?") + "square=1&river=" + shape
    }
    readonly property real radius: full ? 0 : width * 0.04 // matches the provider's corner radius
    // Which image is showing: 0 / 1, or -1 for the placeholder.
    property int shown: -1

    // Two images take turns so the old cover can fade out while the new one fades in.
    function load() {
        if (sourceUrl === "") {
            shown = -1
            return
        }
        const target = shown === 0 ? imageB : imageA
        target.requested = sourceUrl
        target.source = sourceUrl
    }

    function imageSettled(image, index) {
        // Compare with what we asked for: Qt normalizes image.source, so it can differ.
        if (image.requested !== sourceUrl || image.status === Image.Loading)
            return
        // The provider returns a 1x1 transparent image when there is no cover.
        shown = image.status === Image.Ready && image.implicitWidth > 1 ? index : -1
    }

    scale: playing || full ? 1.0 : 0.94
    Behavior on scale { enabled: !root.full; NumberAnimation { duration: Theme.animSlow; easing.type: Theme.easingBack } }

    onSourceUrlChanged: load()
    Component.onCompleted: load()

    // Soft shadow: a few stacked, slightly larger, translucent rounded rectangles.
    Repeater {
        model: 5
        delegate: Rectangle {
            required property int index
            readonly property real spread: (index + 1) * 4

            x: -root.bleedSide - spread / 2
            y: -root.bleedTop - spread / 2 + 10
            width: root.width + 2 * root.bleedSide + spread
            height: root.height + root.bleedTop + spread
            radius: root.radius + spread / 2
            color: "black"
            opacity: 0.09 * (1 - root.spread)
        }
    }

    // Placeholder (also the backdrop while a cover fades in)
    Rectangle {
        anchors.fill: parent
        anchors.topMargin: -root.bleedTop
        anchors.leftMargin: -root.bleedSide
        anchors.rightMargin: -root.bleedSide
        radius: root.radius
        color: Theme.surfaceAlt
        opacity: root.shown === -1 ? 1 : 0

        Behavior on opacity { enabled: !root.full; NumberAnimation { duration: Theme.animSlow } }

        Icon {
            anchors.centerIn: parent
            name: "music"
            size: parent.width * 0.3
            color: Theme.textMuted
        }
    }

    Image {
        id: imageA

        property string requested

        anchors.fill: parent
        anchors.topMargin: -root.bleedTop
        anchors.leftMargin: -root.bleedSide
        anchors.rightMargin: -root.bleedSide
        sourceSize: root.spread > 0 ? Qt.size(1200, 1200) : Qt.size(600, 600)
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: false
        smooth: true
        mipmap: true
        opacity: root.shown === 0 ? 1 : 0
        // The new cover settles in from slightly larger as it fades in.
        scale: root.shown === 0 || root.full ? 1 : 1.04
        onStatusChanged: root.imageSettled(imageA, 0)

        Behavior on opacity { enabled: !root.full; NumberAnimation { duration: Theme.animSlow } }
        Behavior on scale { enabled: !root.full; NumberAnimation { duration: Theme.animSlow * 1.4; easing.type: Theme.easing } }
    }

    Image {
        id: imageB

        property string requested

        anchors.fill: parent
        anchors.topMargin: -root.bleedTop
        anchors.leftMargin: -root.bleedSide
        anchors.rightMargin: -root.bleedSide
        sourceSize: root.spread > 0 ? Qt.size(1200, 1200) : Qt.size(600, 600)
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: false
        smooth: true
        mipmap: true
        opacity: root.shown === 1 ? 1 : 0
        // The new cover settles in from slightly larger as it fades in.
        scale: root.shown === 1 || root.full ? 1 : 1.04
        onStatusChanged: root.imageSettled(imageB, 1)

        Behavior on opacity { enabled: !root.full; NumberAnimation { duration: Theme.animSlow } }
        Behavior on scale { enabled: !root.full; NumberAnimation { duration: Theme.animSlow * 1.4; easing.type: Theme.easing } }
    }
}
