pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The local library panel: Songs / Albums / Artists / Folders. Opening an
// album or an artist shows its songs, with a back button; Folders is browsed
// like a file manager. Clicking a song plays the list it's in (never adding it
// to the Queue). The header has Refresh, Pin and Close. The music folders (and
// the ones left out) are chosen in Settings.
Rectangle {
    id: root

    property bool showPinButton: true
    property bool pinned: false
    property bool showCloseButton: true
    property int tab: 0
    readonly property bool hasFolders: SettingsController.libraryFolders.length > 0
    readonly property bool inDetail: LocalLibraryController.detailKind !== 0 && (tab === 1 || tab === 2)

    signal pinToggled()
    signal closeRequested()
    signal settingsRequested()
    signal detailsRequested(string path)
    signal editTagsRequested(string path)
    signal foldersSettingsRequested() // the music folders are chosen in Settings

    onTabChanged: LocalLibraryController.closeDetail()

    // Back / Esc: out of the album / artist, or up one folder (false: nothing to go back from).
    readonly property bool canGoBack: inDetail || (tab === 3 && hasFolders && LocalLibraryController.folder !== "")
    function back() {
        if (inDetail)
            LocalLibraryController.closeDetail()
        else if (canGoBack)
            LocalLibraryController.folderUp()
        else
            return false
        return true
    }
    // Ctrl+F: the Songs tab's search, ready to type.
    function focusSearch() {
        tab = 0
        searchField.forceActiveFocus()
        searchField.selectAll()
    }

    color: Theme.panelShade // softens the cover tint behind it
    radius: Theme.radiusCard
    border.color: Theme.border


    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 1 // inside the border
        anchors.bottomMargin: 6 // scrolling rows stay clear of the rounded corners
        spacing: 0

        // Header
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.headerHeight
            color: Theme.fillSoft
            topLeftRadius: Theme.radiusCard - 1
            topRightRadius: Theme.radiusCard - 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.padding
                anchors.rightMargin: Theme.spacing
                spacing: Theme.spacing

                Icon {
                    name: "compactDisc"
                    size: 14
                    color: Theme.accent
                }

                Label {
                    text: qsTr("Library")
                    color: Theme.text
                    font.pixelSize: Theme.fontSizeLarge
                }

                Label {
                    Layout.fillWidth: true
                    visible: LocalLibraryController.songCount > 0
                    text: Util.songCount(LocalLibraryController.songCount)
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeSmall
                    elide: Text.ElideRight
                }

                Item {
                    Layout.fillWidth: true
                    visible: LocalLibraryController.songCount === 0
                }

                ControlButton {
                    id: refreshButton
                    iconName: "refresh"
                    iconSize: 13
                    enabled: root.hasFolders && !LocalLibraryController.scanning
                    iconColor: LocalLibraryController.scanning ? Theme.accent : Theme.textMuted
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Look for new and changed songs")
                    onClicked: LocalLibraryController.refresh()

                    RotationAnimation on rotation {
                        running: LocalLibraryController.scanning
                        loops: Animation.Infinite
                        from: 0
                        to: 360
                        duration: 1000
                        onRunningChanged: if (!running) refreshButton.rotation = 0
                    }
                }

                ControlButton {
                    visible: root.showPinButton
                    iconName: "thumbtack"
                    iconSize: 14
                    active: root.pinned
                    iconColor: root.pinned ? Theme.accent : Theme.textMuted
                    rotation: root.pinned ? 0 : 45
                    ToolTip.visible: hovered
                    ToolTip.text: root.pinned ? qsTr("Unpin panel") : qsTr("Pin panel")
                    onClicked: root.pinToggled()

                    Behavior on rotation { NumberAnimation { duration: Theme.anim; easing.type: Theme.easing } }
                }

                ControlButton {
                    visible: root.showCloseButton
                    iconName: "close"
                    iconSize: 13
                    iconColor: Theme.textMuted
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Close")
                    onClicked: root.closeRequested()
                }
            }
        }

        // Tabs
        Flow {
            Layout.fillWidth: true
            Layout.margins: Theme.padding
            Layout.bottomMargin: 4
            spacing: 6

            Repeater {
                model: [
                    { icon: "music", label: qsTr("Songs") },
                    { icon: "compactDisc", label: qsTr("Albums") },
                    { icon: "user", label: qsTr("Artists") },
                    { icon: "folder", label: qsTr("Folders") }
                ]

                delegate: PillButton {
                    required property var modelData
                    required property int index

                    iconName: modelData.icon
                    iconSize: 11
                    text: modelData.label
                    implicitHeight: 34
                    active: root.tab === index
                    onClicked: root.tab = index
                }
            }
        }

        // Scan progress
        Label {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.padding
            Layout.rightMargin: Theme.padding
            visible: LocalLibraryController.scanning
            text: LocalLibraryController.scanTotal > 0
                  ? qsTr("Reading songs… %1 / %2").arg(LocalLibraryController.scanDone).arg(LocalLibraryController.scanTotal)
                  : qsTr("Looking for songs…")
            color: Theme.textMuted
            font.pixelSize: Theme.fontSizeSmall
        }

        // Pages: the new one slides in from its side, the old one out the other way.
        Item {
            id: pages
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            // ---- Songs
            ColumnLayout {
                anchors.fill: parent
                spacing: Theme.spacing
                opacity: root.tab === 0 ? 1 : 0
                visible: opacity > 0
                transform: PageShift { page: 0 }
                Behavior on opacity { NumberAnimation { duration: Theme.anim } }

                TextField {
                    id: searchField
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.padding
                    Layout.rightMargin: Theme.padding
                    Layout.topMargin: 4
                    visible: LocalLibraryController.songCount > 0
                    implicitHeight: 40
                    leftPadding: 40
                    rightPadding: 16
                    placeholderText: qsTr("Search your library")
                    placeholderTextColor: Theme.textMuted
                    color: Theme.text
                    font.pixelSize: Theme.fontSize
                    selectByMouse: true
                    text: LocalLibraryController.search
                    onTextEdited: LocalLibraryController.search = text

                    background: Rectangle {
                        radius: height / 2
                        color: searchField.activeFocus ? Theme.fillStrong : Theme.fill
                        border.width: searchField.activeFocus ? 1 : 0
                        border.color: Theme.accent

                        Behavior on color { ColorAnimation { duration: Theme.animFast } }
                    }

                    Icon {
                        anchors.left: parent.left
                        anchors.leftMargin: 15
                        anchors.verticalCenter: parent.verticalCenter
                        name: "search"
                        size: 12
                        color: Theme.textMuted
                    }
                }

                SongList {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: LocalLibraryController.songs
                    list: LocalLibraryController.SongsList
                }
            }

            // ---- Albums (grid of covers)
            GridView {
                id: albumGrid
                populate: ListPopulate {}

                readonly property int columns: Math.max(2, Math.floor(width / 150))

                anchors.fill: parent
                anchors.leftMargin: Theme.spacing
                anchors.rightMargin: Theme.spacing
                opacity: root.tab === 1 && !root.inDetail ? 1 : 0
                visible: opacity > 0
                transform: PageShift { page: 1 }
                Behavior on opacity { NumberAnimation { duration: Theme.anim } }
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                cellWidth: width / columns
                cellHeight: cellWidth + 46
                model: LocalLibraryController.albums
                ScrollBar.vertical: ScrollBar {}

                delegate: Item {
                    id: tile

                    required property var modelData

                    width: albumGrid.cellWidth
                    height: albumGrid.cellHeight

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 4
                        radius: Theme.radiusLarge
                        color: tileHover.hovered ? Theme.hover : "transparent"

                        Behavior on color { ColorAnimation { duration: Theme.animFast } }

                        HoverHandler { id: tileHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: LocalLibraryController.showAlbum(tile.modelData.key) }

                        // Hold and drag the whole album onto the Queue or the player
                        DragSource {
                            payload: function() {
                                return { items: LocalLibraryController.albumItems(tile.modelData.key),
                                         title: tile.modelData.album,
                                         subtitle: tile.modelData.count === 1 ? qsTr("1 song")
                                                                             : qsTr("%1 songs").arg(tile.modelData.count),
                                         cover: Util.coverSource(tile.modelData.coverPath, "") }
                            }
                        }

                        Rectangle {
                            id: cover
                            x: 6
                            y: 6
                            width: parent.width - 12
                            height: width
                            radius: Theme.radius
                            color: Theme.surfaceAlt
                            clip: true
                            scale: tileHover.hovered ? 1.03 : 1

                            Behavior on scale { NumberAnimation { duration: Theme.anim; easing.type: Theme.easing } }

                            Icon {
                                anchors.centerIn: parent
                                name: "compactDisc"
                                size: 28
                                color: Theme.textDim
                            }

                            Image {
                                anchors.fill: parent
                                source: Util.coverSource(tile.modelData.coverPath, "")
                                sourceSize: Qt.size(280, 280)
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                                opacity: status === Image.Ready ? 1 : 0

                                Behavior on opacity { NumberAnimation { duration: Theme.anim } }
                            }
                        }

                        Label {
                            anchors.top: cover.bottom
                            anchors.topMargin: 6
                            x: 8
                            width: parent.width - 16
                            text: tile.modelData.album
                            color: Theme.text
                            font.pixelSize: Theme.fontSize
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }

                        Label {
                            anchors.top: cover.bottom
                            anchors.topMargin: 24
                            x: 8
                            width: parent.width - 16
                            text: tile.modelData.artist
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSizeSmall
                            elide: Text.ElideRight
                        }
                    }
                }
            }

            // ---- Artists
            ListView {
                populate: ListPopulate {}
                anchors.fill: parent
                anchors.leftMargin: Theme.spacing
                anchors.rightMargin: Theme.spacing
                opacity: root.tab === 2 && !root.inDetail ? 1 : 0
                visible: opacity > 0
                transform: PageShift { page: 2 }
                Behavior on opacity { NumberAnimation { duration: Theme.anim } }
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                spacing: 2
                model: LocalLibraryController.artists
                ScrollBar.vertical: ScrollBar {}

                delegate: Rectangle {
                    id: artistRow

                    required property var modelData

                    width: ListView.view.width
                    height: 52
                    radius: Theme.radius
                    color: artistHover.hovered ? Theme.hover : "transparent"

                    Behavior on color { ColorAnimation { duration: Theme.animFast } }

                    HoverHandler { id: artistHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: LocalLibraryController.showArtist(artistRow.modelData.name) }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 12
                        spacing: Theme.padding

                        Rectangle {
                            Layout.preferredWidth: 36
                            Layout.preferredHeight: 36
                            radius: 18
                            color: Theme.accentSoft

                            Label {
                                anchors.centerIn: parent
                                text: artistRow.modelData.name.charAt(0).toUpperCase()
                                color: Theme.accent
                                font.pixelSize: Theme.fontSizeLarge
                                font.weight: Font.DemiBold
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: artistRow.modelData.name
                            color: Theme.text
                            font.pixelSize: Theme.fontSize
                            elide: Text.ElideRight
                        }

                        Label {
                            text: Util.songCount(artistRow.modelData.count)
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSizeSmall
                        }

                        Icon {
                            name: "chevronRight"
                            size: 10
                            color: Theme.textDim
                        }
                    }
                }
            }

            // ---- An opened album / artist
            ColumnLayout {
                anchors.fill: parent
                spacing: Theme.spacing
                opacity: root.inDetail ? 1 : 0
                visible: opacity > 0
                transform: DetailShift {}
                Behavior on opacity { NumberAnimation { duration: Theme.anim } }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.spacing
                    Layout.rightMargin: Theme.padding
                    spacing: Theme.padding

                    PillButton {
                        iconName: "back"
                        iconSize: 12
                        implicitWidth: 36
                        implicitHeight: 36
                        ToolTip.visible: hovered
                        ToolTip.text: root.tab === 1 ? qsTr("Back to albums") : qsTr("Back to artists")
                        onClicked: LocalLibraryController.closeDetail()
                    }

                    Rectangle {
                        visible: LocalLibraryController.detailKind === 1
                        Layout.preferredWidth: 56
                        Layout.preferredHeight: 56
                        radius: Theme.radius
                        color: Theme.surfaceAlt
                        clip: true

                        Image {
                            anchors.fill: parent
                            source: Util.coverSource(LocalLibraryController.detailCoverPath, "")
                            sourceSize: Qt.size(112, 112)
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            Layout.fillWidth: true
                            text: LocalLibraryController.detailTitle
                            color: Theme.text
                            font.pixelSize: Theme.fontSizeLarge
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }

                        Label {
                            Layout.fillWidth: true
                            text: LocalLibraryController.detailSubtitle
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSizeSmall
                            elide: Text.ElideRight
                        }
                    }
                }

                RowLayout {
                    Layout.leftMargin: Theme.padding
                    spacing: 6

                    PillButton {
                        iconName: "play"
                        iconSize: 11
                        text: qsTr("Play all")
                        implicitHeight: 32
                        filled: true
                        onClicked: LocalLibraryController.play(LocalLibraryController.DetailList, -1)
                    }

                    PillButton {
                        iconName: "plus"
                        iconSize: 11
                        text: qsTr("Add all to queue")
                        implicitHeight: 32
                        onClicked: LocalLibraryController.addToQueue(LocalLibraryController.DetailList, -1)
                    }
                }

                SongList {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: LocalLibraryController.detailSongs
                    list: LocalLibraryController.DetailList
                    showAlbum: LocalLibraryController.detailKind === 2
                }
            }

            // ---- Folders, browsed like a file manager: a path bar, then the
            // folder's subfolders and its songs. At the top, the music folders
            // (added and removed in Settings).
            ColumnLayout {
                anchors.fill: parent
                spacing: 4
                opacity: root.tab === 3 && root.hasFolders ? 1 : 0
                visible: opacity > 0
                transform: PageShift { page: 3 }
                Behavior on opacity { NumberAnimation { duration: Theme.anim } }

                readonly property bool atTop: LocalLibraryController.folder === ""

                // Path bar: up, then where we are (each part opens that folder)
                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.spacing
                    Layout.rightMargin: Theme.padding
                    spacing: 2

                    PillButton {
                        iconName: "back"
                        iconSize: 12
                        implicitWidth: 34
                        implicitHeight: 34
                        enabled: !parent.parent.atTop
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Up one folder")
                        onClicked: LocalLibraryController.folderUp()
                    }

                    Flickable {
                        id: pathBar
                        Layout.fillWidth: true
                        Layout.preferredHeight: 34
                        clip: true
                        contentWidth: crumbs.width
                        flickableDirection: Flickable.HorizontalFlick
                        boundsBehavior: Flickable.StopAtBounds
                        // The deepest part stays in view.
                        onContentWidthChanged: contentX = Math.max(0, contentWidth - width)

                        Row {
                            id: crumbs
                            height: parent.height
                            spacing: 0

                            Repeater {
                                model: [{ name: qsTr("Folders"), path: "" }].concat(LocalLibraryController.folderCrumbs)

                                delegate: Row {
                                    id: crumb

                                    required property var modelData
                                    required property int index
                                    readonly property bool last: index === LocalLibraryController.folderCrumbs.length

                                    height: crumbs.height

                                    Icon {
                                        visible: crumb.index > 0
                                        anchors.verticalCenter: parent.verticalCenter
                                        name: "chevronRight"
                                        size: 9
                                        color: Theme.textDim
                                    }

                                    PillButton {
                                        anchors.verticalCenter: parent.verticalCenter
                                        iconName: crumb.index === 0 ? "home" : ""
                                        iconSize: 11
                                        text: crumb.modelData.name
                                        implicitHeight: 30
                                        transparentIdle: true
                                        active: crumb.last
                                        onClicked: if (!crumb.last) LocalLibraryController.openFolder(crumb.modelData.path)
                                    }
                                }
                            }
                        }
                    }

                    PillButton {
                        visible: !parent.parent.atTop
                        iconName: "play"
                        iconSize: 11
                        implicitWidth: 34
                        implicitHeight: 34
                        filled: true
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Play all (subfolders too)")
                        onClicked: LocalLibraryController.playFolder()
                    }

                    PillButton {
                        visible: !parent.parent.atTop
                        iconName: "plus"
                        iconSize: 11
                        implicitWidth: 34
                        implicitHeight: 34
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Add all to queue")
                        onClicked: LocalLibraryController.addFolderToQueue()
                    }
                }

                // Subfolders first, then the songs, in one scrolling list
                ListView {
                    id: folderView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.leftMargin: Theme.spacing
                    Layout.rightMargin: Theme.spacing
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    spacing: 2
                    ScrollBar.vertical: ScrollBar {}
                    model: LocalLibraryController.folderSongs

                    header: Column {
                        width: folderView.width
                        spacing: 2
                        bottomPadding: LocalLibraryController.subfolders.length > 0 && folderView.count > 0 ? 6 : 0

                        Repeater {
                            model: LocalLibraryController.subfolders

                            delegate: LibraryFolderRow {
                                required property var modelData

                                width: folderView.width
                                name: modelData.name
                                path: modelData.path
                                count: modelData.count
                                location: modelData.location || ""
                                onOpened: {
                                    LocalLibraryController.openFolder(path)
                                    folderView.positionViewAtBeginning()
                                }
                            }
                        }
                    }

                    delegate: LibrarySongRow {
                        width: ListView.view.width
                        list: LocalLibraryController.FolderList
                        onDetailsRequested: (path) => root.detailsRequested(path)
                        onEditTagsRequested: (path) => root.editTagsRequested(path)
                    }

                    Label {
                        anchors.centerIn: parent
                        width: parent.width - 4 * Theme.padding
                        visible: folderView.count === 0 && LocalLibraryController.subfolders.length === 0
                        text: qsTr("No songs in this folder.")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSize
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }
                }
            }

            // ---- No music folders yet (they're chosen in Settings)
            Column {
                anchors.centerIn: parent
                width: parent.width - 4 * Theme.padding
                spacing: Theme.spacing
                visible: !root.hasFolders

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 76
                    height: 76
                    radius: 38
                    color: Theme.accentSoft

                    Icon {
                        anchors.centerIn: parent
                        name: "compactDisc"
                        size: 28
                        color: Theme.accent
                    }
                }

                Item { width: 1; height: 4 }

                Label {
                    width: parent.width
                    text: qsTr("Build your library")
                    color: Theme.text
                    font.pixelSize: Theme.fontSizeLarge
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                }

                Label {
                    width: parent.width
                    text: qsTr("Choose the folders where your music is. Every song in them shows up here, sorted into albums, artists and folders.")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeSmall
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }

                Item { width: 1; height: 4 }

                PillButton {
                    anchors.horizontalCenter: parent.horizontalCenter
                    iconName: "folderPlus"
                    iconSize: 12
                    text: qsTr("Choose music folders")
                    implicitHeight: 36
                    filled: true
                    onClicked: root.foldersSettingsRequested()
                }
            }

            // Folders added, nothing found (yet)
            Label {
                anchors.centerIn: parent
                width: parent.width - 4 * Theme.padding
                visible: root.hasFolders && root.tab !== 3 && LocalLibraryController.songCount === 0
                         && !LocalLibraryController.scanning
                text: qsTr("No songs found in your library folders.")
                color: Theme.textMuted
                font.pixelSize: Theme.fontSize
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }
        }

    }

    // A list of library songs.
    component SongList: ListView {
        id: songList
        populate: ListPopulate {}

        property int list: LocalLibraryController.SongsList
        property bool showAlbum: true

        Layout.leftMargin: Theme.spacing
        Layout.rightMargin: Theme.spacing
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        spacing: 2
        ScrollBar.vertical: ScrollBar {}

        delegate: LibrarySongRow {
            width: ListView.view.width
            list: songList.list
            showAlbum: songList.showAlbum
            onDetailsRequested: (path) => root.detailsRequested(path)
            onEditTagsRequested: (path) => root.editTagsRequested(path)
        }
    }

    // The opened album / artist slides in from the right.
    component DetailShift: Translate {
        x: root.inDetail ? 0 : 48
        Behavior on x { NumberAnimation { duration: Theme.animSlow; easing.type: Theme.easingPanel } }
    }

    // Horizontal offset of a tab page: 0 when shown, else off to its side.
    component PageShift: Translate {
        required property int page
        x: root.tab === page ? 0 : (page < root.tab ? -48 : 48)
        Behavior on x { NumberAnimation { duration: Theme.animSlow; easing.type: Theme.easingPanel } }
    }
}
