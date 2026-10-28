pragma Singleton
import QtQuick

// Small helpers shared by the views: time / count formatting and cover addresses.
QtObject {
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
}
