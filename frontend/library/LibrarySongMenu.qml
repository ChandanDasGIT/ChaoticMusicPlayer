import QtQuick
import QtQuick.Controls

// "⋮" menu of a library song: play, queue, playlists, details, editing its tags.
ThemedMenu {
    id: root

    property int list: LocalLibraryController.SongsList // the list the song is shown in
    property int index: -1
    property string path

    signal detailsRequested(string path)
    signal editTagsRequested(string path)

    Action {
        text: qsTr("Play")
        icon.name: "play"
        onTriggered: LocalLibraryController.play(root.list, root.index)
    }

    Action {
        text: qsTr("Play next")
        icon.name: "next"
        enabled: root.path !== PlayerController.source
        onTriggered: PlaylistsController.playNext([{ path: root.path }])
    }

    Action {
        text: qsTr("Add to queue")
        icon.name: "plus"
        onTriggered: LocalLibraryController.addToQueue(root.list, root.index)
    }

    AddToPlaylistMenu {
        items: [{ path: root.path }]
    }

    Action {
        text: qsTr("New playlist with this song…")
        icon.name: "listMusic"
        onTriggered: PlaylistsController.requestNewPlaylist([{ path: root.path }])
    }

    Action {
        text: qsTr("Track details")
        icon.name: "info"
        onTriggered: root.detailsRequested(root.path)
    }

    Action {
        text: qsTr("Edit tags…")
        icon.name: "sliders"
        onTriggered: root.editTagsRequested(root.path)
    }
}
