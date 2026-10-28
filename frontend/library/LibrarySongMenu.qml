import QtQuick
import QtQuick.Controls

// "⋮" menu of a library song: play, queue, playlists, details.
ThemedMenu {
    id: root

    property bool inDetail: false
    property int index: -1
    property string path

    signal detailsRequested(string path)

    Action {
        text: qsTr("Play")
        icon.name: "play"
        onTriggered: LocalLibraryController.playInQueue(root.inDetail, root.index)
    }

    Action {
        text: qsTr("Add to queue")
        icon.name: "plus"
        onTriggered: LocalLibraryController.addToQueue(root.inDetail, root.index)
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
}
