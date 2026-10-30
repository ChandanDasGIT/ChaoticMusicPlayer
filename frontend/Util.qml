pragma Singleton
import QtQuick

// Small helpers shared by the views: time / count formatting and cover addresses.
QtObject {
    property int openMenus: 0 // menus open now (controls/ThemedMenu.qml)

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

    // YouTube cards: an artist ("UC..." channel) or a playlist / album.
    function ytIsArtist(browseId) {
        return (browseId || "").startsWith("UC")
    }
    // Opens a card: the artist's page, or the playlist / album's songs.
    function ytOpen(card) {
        if (ytIsArtist(card.browseId))
            YouTubeController.openArtist(card.browseId, card.title)
        else
            YouTubeController.openCollection(card.browseId, card.title)
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

    // Japanese and Chinese share most characters but draw some differently; so
    // do Korean hanja. Without a hint the system may pick a Chinese font for
    // them. For text with Japanese kana or Korean hangul: a matching Japanese /
    // Korean font, if one is installed ("" = the normal font).
    readonly property var installedFonts: Qt.fontFamilies()
    function cjkFamily(text) {
        if (!text)
            return ""
        // The first one installed: Linux and Android have Noto CJK, Windows its own
        const want = /[\u3040-\u30ff\u31f0-\u31ff]/.test(text)
                     ? ["Noto Sans CJK JP", "Noto Sans JP", "Source Han Sans JP", "Yu Gothic UI", "Yu Gothic", "Meiryo UI", "Meiryo"]
                   : /[\uac00-\ud7af\u1100-\u11ff]/.test(text)
                     ? ["Noto Sans CJK KR", "Noto Sans KR", "Source Han Sans KR", "Malgun Gothic"]
                   : []
        for (const family of want)
            if (installedFonts.indexOf(family) >= 0)
                return family
        return ""
    }
}
