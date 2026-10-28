pragma Singleton
import QtQuick

QtObject {
    // Colors
    readonly property color background: "#111116"
    readonly property color surface:    "#17171e"
    readonly property color surfaceAlt: "#1f1f28"
    readonly property color border:     "#22ffffff" // translucent: work on any tint
    readonly property color hover:      "#12ffffff"
    readonly property color selection:  "#24ffffff"
    readonly property color accent:     "#9b8cff"
    readonly property color accentSoft: "#339b8cff" // accent at ~20% opacity
    readonly property color text:       "#ececf2"
    readonly property color textMuted:  "#8b8b9e"
    readonly property color textDim:    "#5c5c6e"
    readonly property color error:      "#f06a6a"

    // Typography
    readonly property int fontSizeSmall: 11
    readonly property int fontSize:      13
    readonly property int fontSizeLarge: 16
    readonly property int fontSizeTitle: 21

    // Metrics
    readonly property int spacing:      8
    readonly property int padding:      12
    readonly property int radius:       8
    readonly property int radiusLarge:  14
    readonly property int headerHeight: 46
    readonly property int radiusCard:   18 // floating panels
    readonly property int cardGap:      10 // space around / between panels

    // Animation
    readonly property int animFast:   120
    readonly property int anim:       220
    readonly property int animSlow:   420
    readonly property int easing:     Easing.OutCubic
    readonly property int easingBack: Easing.OutBack
    // Panels: long, soft deceleration ("spring-like")
    readonly property int animPanel:  480
    readonly property int easingPanel: Easing.OutQuint

    // Below this window width the app switches to the phone-like layout.
    readonly property int compactBreakpoint: 700

    // "m:ss" or "h:mm:ss"
    function formatTime(ms) {
        let s = Math.floor(Math.max(0, ms) / 1000)
        const h = Math.floor(s / 3600)
        const m = Math.floor((s % 3600) / 60)
        s = s % 60
        const ss = s < 10 ? "0" + s : "" + s
        if (h > 0)
            return h + ":" + (m < 10 ? "0" + m : m) + ":" + ss
        return m + ":" + ss
    }

    // Cover image for a track: its YouTube thumbnail, else the file's own cover.
    // (Both go through the album-art provider, which rounds the corners.)
    function coverSource(path, thumbnail) {
        if (thumbnail && thumbnail !== "")
            return "image://albumart/" + encodeURIComponent(thumbnail)
        // (The revision makes images ask again after a missing cover was found online.)
        return path && path !== "" ? "image://albumart/" + encodeURIComponent(path) + "?r=" + CoverController.revision : ""
    }

    // "1 song" / "9 songs"
    function songCount(n) {
        return n === 1 ? qsTr("1 song") : qsTr("%1 songs").arg(n)
    }

    // "45 min" / "1 h 5 min"
    function formatLongDuration(ms) {
        if (ms > 0 && ms < 30000)
            return qsTr("<1 min")
        const totalMin = Math.round(Math.max(0, ms) / 60000)
        const h = Math.floor(totalMin / 60)
        const m = totalMin % 60
        return h > 0 ? qsTr("%1 h %2 min").arg(h).arg(m) : qsTr("%1 min").arg(m)
    }

    // Icons (Font Awesome 6 Free Solid codepoints)
    readonly property string iconFamily: "Font Awesome 6 Free"
    readonly property var icons: ({
        "play":       "\uf04b",
        "pause":      "\uf04c",
        "next":       "\uf051",
        "prev":       "\uf048",
        "shuffle":    "\uf074",
        "repeat":     "\uf01e",
        "volume":     "\uf028",
        "volumeLow":  "\uf027",
        "volumeMute": "\uf6a9",
        "list":       "\uf03a",
        "thumbtack":  "\uf08d",
        "music":      "\uf001",
        "folderOpen": "\uf07c",
        "close":      "\uf00d",
        "info":       "\uf05a",
        "sortUp":     "\uf0d8",
        "sortDown":   "\uf0d7",
        "chevronDown":  "\uf078",
        "chevronRight": "\uf054",
        "trash":      "\uf1f8",
        "settings":   "\uf013",
        "power":      "\uf011",
        "lyrics":     "\uf10d",
        "arrowUp":    "\uf062",
        "arrowDown":  "\uf063",
        "plus":       "\u002b",
        "minus":      "\uf068",
        "back":       "\uf060",
        "cloud":      "\uf0c2",
        "search":     "\uf002",
        "bookmark":   "\uf02e",
        "link":       "\uf0c1",
        "user":       "\uf007",
        "heart":      "\uf004",
        "compass":    "\uf14e",
        "listMusic":  "\uf0ca",
        "check":      "\uf00c",
        "refresh":    "\uf021",
        "more":       "\uf142",
        "grip":       "\uf58e",
        "chevronLeftSmall": "\uf053",
        "compactDisc": "\uf51f",
        "folder":     "\uf07b",
        "folderPlus": "\uf65e",
        "eyeSlash":   "\uf070",
        "home":       "\uf015",
        "download":   "\uf019",
        "drive":      "\uf0a0"
    })
}
