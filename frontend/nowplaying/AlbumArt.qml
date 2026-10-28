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

    readonly property string sourceUrl: Util.coverSource(path, thumbnail)
    readonly property real radius: width * 0.04 // matches the provider's corner radius
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

    scale: playing ? 1.0 : 0.94
    Behavior on scale { NumberAnimation { duration: Theme.animSlow; easing.type: Theme.easingBack } }

    onSourceUrlChanged: load()
    Component.onCompleted: load()

    // Soft shadow: a few stacked, slightly larger, translucent rounded rectangles.
    Repeater {
        model: 5
        delegate: Rectangle {
            required property int index
            readonly property real spread: (index + 1) * 4

            x: -spread / 2
            y: -spread / 2 + 10
            width: root.width + spread
            height: root.height + spread
            radius: root.radius + spread / 2
            color: "black"
            opacity: 0.09
        }
    }

    // Placeholder (also the backdrop while a cover fades in)
    Rectangle {
        anchors.fill: parent
        radius: root.radius
        color: Theme.surfaceAlt
        opacity: root.shown === -1 ? 1 : 0

        Behavior on opacity { NumberAnimation { duration: Theme.animSlow } }

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
        sourceSize: Qt.size(600, 600)
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        cache: false
        smooth: true
        mipmap: true
        opacity: root.shown === 0 ? 1 : 0
        onStatusChanged: root.imageSettled(imageA, 0)

        Behavior on opacity { NumberAnimation { duration: Theme.animSlow } }
    }

    Image {
        id: imageB

        property string requested

        anchors.fill: parent
        sourceSize: Qt.size(600, 600)
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        cache: false
        smooth: true
        mipmap: true
        opacity: root.shown === 1 ? 1 : 0
        onStatusChanged: root.imageSettled(imageB, 1)

        Behavior on opacity { NumberAnimation { duration: Theme.animSlow } }
    }
}
