pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

// "Add to playlist ▸" submenu: one entry per playlist, plus "New playlist…".
// `items` are the songs to add (see PlaylistsController).
ThemedMenu {
    id: root

    property var items: []

    title: qsTr("Add to playlist")

    Instantiator {
        model: PlaylistsController.playlists

        delegate: Action {
            required property var modelData

            text: modelData.name + "  (" + modelData.count + ")"
            icon.name: "listMusic"
            onTriggered: PlaylistsController.addToPlaylist(modelData.id, root.items)
        }

        onObjectAdded: (index, object) => root.insertAction(index, object)
        onObjectRemoved: (index, object) => root.removeAction(object)
    }

    MenuSeparator {
        visible: PlaylistsController.playlists.length > 0
        contentItem: Rectangle {
            implicitHeight: 1
            color: Theme.border
        }
    }

    Action {
        text: qsTr("New playlist…")
        icon.name: "plus"
        onTriggered: PlaylistsController.requestNewPlaylist(root.items)
    }
}
