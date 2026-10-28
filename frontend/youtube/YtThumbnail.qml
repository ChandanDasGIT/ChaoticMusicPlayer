import QtQuick

// Artwork for YouTube items: the real thumbnail when there is one (rounded by
// the album-art provider), over a colored placeholder that shows while it
// loads or when there is none.
Rectangle {
    id: root

    property string source    // thumbnail URL
    property real hue: 0.6
    property string iconName: "music"

    radius: Math.min(width, height) * 0.12
    gradient: Gradient {
        GradientStop { position: 0.0; color: Qt.hsla(root.hue, 0.45, 0.48, 1) }
        GradientStop { position: 1.0; color: Qt.hsla((root.hue + 0.08) % 1, 0.5, 0.28, 1) }
    }

    Icon {
        anchors.centerIn: parent
        name: root.iconName
        size: Math.min(root.width, root.height) * 0.32
        color: "#b3ffffff"
        visible: image.status !== Image.Ready
    }

    Image {
        id: image
        anchors.fill: parent
        source: root.source !== "" ? Util.coverSource("", root.source) : ""
        sourceSize: Qt.size(Math.max(64, root.width * 2), Math.max(64, root.height * 2))
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        opacity: status === Image.Ready ? 1 : 0

        Behavior on opacity { NumberAnimation { duration: Theme.anim } }
    }
}
