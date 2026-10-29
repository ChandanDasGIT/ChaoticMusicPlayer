import QtQuick

// The app's keyboard shortcuts. Playback ones act directly; the rest ask Main
// (signals). Keys that typing uses (Space, arrows, M) are off while a text
// field has focus. The list in Settings mirrors this file: keep them in sync.
// Application-wide: window shortcuts are switched off under a modal popup, and
// the slide-in panels are modal popups.
Item {
    id: root

    // The window's focused item is a text field: leave plain keys to it.
    readonly property bool typing: {
        const item = Window.activeFocusItem
        return item !== null && "cursorPosition" in item
    }

    property bool escapeEnabled: false
    // A dialog is open: the window shortcuts (panels, lyrics, search, settings)
    // would act behind it, so only playback and Quit stay on.
    property bool dialogOpen: false

    signal escapePressed()
    signal toggleLyrics()
    signal focusSearch()
    signal toggleQueue()
    signal toggleYoutube()
    signal toggleLibrary()
    signal openSettings()
    signal quit()

    function seekBy(ms) {
        if (!PlayerController.hasTrack)
            return
        const end = PlayerController.duration > 0 ? PlayerController.duration - 500 : PlayerController.position + ms
        PlayerController.seek(Math.max(0, Math.min(end, PlayerController.position + ms)))
    }
    function changeVolume(steps) {
        PlayerController.muted = false
        PlayerController.volume = Math.max(0, Math.min(1, PlayerController.volume + steps * SettingsController.volumeStep / 100))
    }

    // ---- Playback ------------------------------------------------------------
    AppShortcut { sequence: "Space"; enabled: !root.typing; onActivated: PlaylistController.playPause() }
    AppShortcut { sequence: "Ctrl+Right"; onActivated: PlaylistController.next() }
    AppShortcut { sequence: "Ctrl+Left"; onActivated: PlaylistController.previous() }
    AppShortcut { sequence: "Right"; enabled: !root.typing; onActivated: root.seekBy(5000) }
    AppShortcut { sequence: "Left"; enabled: !root.typing; onActivated: root.seekBy(-5000) }
    AppShortcut { sequence: "Shift+Right"; enabled: !root.typing; onActivated: root.seekBy(30000) }
    AppShortcut { sequence: "Shift+Left"; enabled: !root.typing; onActivated: root.seekBy(-30000) }
    AppShortcut { sequence: "Ctrl+Up"; onActivated: root.changeVolume(1) }
    AppShortcut { sequence: "Ctrl+Down"; onActivated: root.changeVolume(-1) }
    AppShortcut { sequence: "M"; enabled: !root.typing; onActivated: PlayerController.muted = !PlayerController.muted }
    AppShortcut { sequence: "Ctrl+S"; onActivated: PlaylistController.shuffle = !PlaylistController.shuffle }
    AppShortcut { sequence: "Ctrl+R"; onActivated: PlaylistController.cycleRepeatMode() }

    // ---- Window --------------------------------------------------------------
    AppShortcut { sequence: "Ctrl+L"; enabled: !root.dialogOpen; onActivated: root.toggleLyrics() }
    AppShortcut { sequence: "Ctrl+F"; enabled: !root.dialogOpen; onActivated: root.focusSearch() }
    AppShortcut { sequence: "Ctrl+1"; enabled: !root.dialogOpen; onActivated: root.toggleQueue() }
    AppShortcut { sequence: "Ctrl+2"; enabled: !root.dialogOpen; onActivated: root.toggleYoutube() }
    AppShortcut { sequence: "Ctrl+3"; enabled: !root.dialogOpen; onActivated: root.toggleLibrary() }
    AppShortcut { sequence: "Ctrl+,"; enabled: !root.dialogOpen; onActivated: root.openSettings() }
    // Only when nothing else wants Esc: open drawers and dialogs close on it
    // themselves, and a text field being typed in gets it first.
    AppShortcut { sequence: "Escape"; enabled: root.escapeEnabled && !root.typing; onActivated: root.escapePressed() }
    AppShortcut { sequence: "Ctrl+Q"; onActivated: root.quit() }

    component AppShortcut: Shortcut {
        context: Qt.ApplicationShortcut
    }
}
