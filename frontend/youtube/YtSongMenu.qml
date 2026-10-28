import QtQuick
import QtQuick.Controls

// "⋮" menu of a YouTube song: play next, queue, playlists, saved.
ThemedMenu {
    id: root

    property var song: ({})

    Action {
        text: qsTr("Play next")
        icon.name: "next"
        enabled: "ytmusic:" + (root.song.videoId || "") !== PlayerController.source
        onTriggered: PlaylistsController.playNext([root.song])
    }

    Action {
        text: qsTr("Add to queue")
        icon.name: "plus"
        onTriggered: YouTubeController.enqueue(root.song)
    }

    AddToPlaylistMenu {
        items: [root.song]
    }

    Action {
        text: qsTr("New playlist with this song…")
        icon.name: "listMusic"
        onTriggered: PlaylistsController.requestNewPlaylist([root.song])
    }

    Action {
        text: qsTr("Save")
        icon.name: "bookmark"
        onTriggered: YouTubeController.save(root.song)
    }
}
