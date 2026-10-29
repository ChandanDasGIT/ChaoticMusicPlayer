pragma Singleton

import QtQuick

// Songs being dragged from the Library or YouTube onto the Queue or the player.
// A source starts it on press-and-hold, moves it with the pointer and ends it
// on release; the drop targets watch `scenePos` (to light up) and `dropped`
// (to take the songs, with deliver()). The window draws the floating card.
QtObject {
    id: root

    property bool active: false
    property var items: []         // [{ path }] or YouTube song maps
    property string browseId: ""   // or a YouTube playlist: its songs are fetched on drop
    property string title: ""
    property string subtitle: ""
    property string cover: ""      // image source for the card
    property point scenePos

    // Lists / panels under the source stop scrolling and swiping meanwhile,
    // so they don't take the drag over.
    property var frozen: []

    signal dropped()

    function start(source, payload, pos) {
        cancel()
        items = payload.items || []
        browseId = payload.browseId || ""
        title = payload.title || ""
        subtitle = payload.subtitle || ""
        cover = payload.cover || ""
        scenePos = pos
        active = true // first: panels whose swiping depends on it stop by themselves
        let list = []
        for (let item = source.parent; item; item = item.parent) {
            if (item.flickableDirection !== undefined && item.interactive) {
                item.interactive = false
                list.push(item)
            }
        }
        frozen = list
    }

    function move(pos) {
        if (active)
            scenePos = pos
    }

    // Released: the target under the pointer takes the songs.
    function finish() {
        if (!active)
            return
        dropped()
        cancel()
    }

    function cancel() {
        for (const item of frozen)
            item.interactive = true
        frozen = []
        active = false
    }

    // Called by the target that got the drop: into playlist `listId` ("" = the
    // Queue) at `row` (-1 = the end).
    function deliver(listId, row) {
        if (browseId !== "") {
            pending[browseId] = { listId: listId, row: row }
            YouTubeController.fetchCollection(browseId, title)
        } else {
            PlaylistsController.insertItems(listId, items, row)
        }
    }

    // Where each YouTube playlist being fetched goes, by browseId.
    property var pending: ({})

    property Connections youtube: Connections {
        target: YouTubeController
        function onCollectionFetched(browseId, songs) {
            const to = root.pending[browseId]
            if (!to)
                return
            delete root.pending[browseId]
            PlaylistsController.insertItems(to.listId, songs, to.row)
        }
    }
}
