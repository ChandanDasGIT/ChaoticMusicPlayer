import QtQuick
import QtQuick.Controls

// The "Add all" drop-down of an album, an artist or a folder: into the Queue,
// into a playlist, or into a new one.
ThemedMenu {
    id: root

    property var songs: []        // [{ path }]
    property var addToQueue: null // the list's own way (it keeps the list's order)

    function openAt(button, songs, addToQueue) {
        root.songs = songs
        root.addToQueue = addToQueue
        popup(button, 0, button.height)
    }

    Action {
        text: qsTr("Add all to queue")
        icon.name: "plus"
        onTriggered: root.addToQueue()
    }

    AddToPlaylistMenu {
        title: qsTr("Add all to playlist")
        items: root.songs
    }

    Action {
        text: qsTr("Create new playlist…")
        icon.name: "listMusic"
        onTriggered: PlaylistsController.requestNewPlaylist(root.songs)
    }
}
