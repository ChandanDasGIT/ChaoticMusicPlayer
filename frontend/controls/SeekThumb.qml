import QtQuick

// The seek bar's thumb, in the shape chosen in Settings. `size` is the plain
// circle's diameter; the other shapes are drawn to look as big, centered on it.
Item {
    id: root

    property string kind: "circle"
    property real size: 10
    property bool moving: false // the disc spins
    property color color: Theme.text
    // How far the bar's line stays away on each side (hollow shapes stay clear of it)
    readonly property real gap: kind === "ring" ? size / 2 : kind === "arrow" ? size * 1.35 : 0
    readonly property string iconName: ({ heart: "heart", star: "star", note: "music", disc: "compactDisc",
                                          play: "play" })[kind] ?? ""
    // The bar's wave at the thumb, for the arrow's tail (phase 0 = at the thumb)
    property real wavePhase: 0
    property real waveAmplitude: 3.2
    property real waveLength: 24
    // On a bar, the arrow's tail melts into it: its colors behind and ahead of the thumb
    property bool mixing: false
    property color barBehind: Theme.accent
    property color barAhead: Theme.textDim
    // Scrubbing: +1 forward, -1 backward. Play and the arrow turn to face it.
    property int direction: 1
    property real flip: direction

    Behavior on flip { NumberAnimation { duration: Theme.animFast; easing.type: Theme.easing } }

    // The pointer moved `dx` while scrubbing: the disc rolls along with it.
    function scrubbed(dx) {
        if (dx === 0)
            return
        direction = dx > 0 ? 1 : -1
        if (kind === "disc")
            glyph.rotation = (glyph.rotation + dx * 4) % 360
    }

    onWavePhaseChanged: if (kind === "arrow") arrow.requestPaint()
    onWaveAmplitudeChanged: if (kind === "arrow") arrow.requestPaint()
    onDirectionChanged: if (kind === "arrow") arrow.requestPaint()

    width: size
    height: size
    onKindChanged: glyph.rotation = 0 // (the disc's spin doesn't carry over)

    Rectangle {
        anchors.centerIn: parent
        visible: root.kind === "circle" || root.kind === "ring"
        width: root.size
        height: root.size
        radius: width / 2
        color: root.kind === "ring" ? "transparent" : root.color
        border.width: root.kind === "ring" ? root.size * 0.23 : 0
        border.color: root.color
    }

    Rectangle {
        anchors.centerIn: parent
        visible: root.kind === "pill"
        width: root.size * 0.47
        height: root.size * 1.73
        radius: width / 2
        color: root.color
    }

    Rectangle {
        anchors.centerIn: parent
        visible: root.kind === "diamond"
        width: root.size * 1.04
        height: width
        rotation: 45
        color: root.color
    }

    Icon {
        id: glyph
        anchors.centerIn: parent
        visible: root.iconName !== ""
        name: root.iconName
        size: root.size * 1.4
        color: root.color
        transform: Scale { origin.x: glyph.width / 2; xScale: root.kind === "play" ? root.flip : 1 }
    }

    // "~>": the tail is a piece of the bar's own wave, stopping just behind the
    // arrowhead's corner. A paused bar is flat, but the tail keeps its "~".
    Canvas {
        id: arrow
        anchors.centerIn: parent
        visible: root.kind === "arrow"
        width: root.size * 3.2
        height: root.size * 2.4
        antialiasing: true
        transform: Scale { origin.x: arrow.width / 2; xScale: root.kind === "arrow" ? root.flip : 1 }

        onWidthChanged: requestPaint()
        onVisibleChanged: if (visible) requestPaint()
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            if (!visible)
                return
            const unit = root.size / 10
            ctx.translate(width / 2, height / 2)
            ctx.lineWidth = 2.3 * unit
            ctx.lineCap = "round"
            ctx.lineJoin = "round"
            ctx.strokeStyle = root.color.toString()

            const start = -root.gap
            const end = 6.7 * unit
            const bar = root.direction > 0 ? root.barBehind : root.barAhead
            const mixed = (t) => {
                // Two streams: the edge between the colors isn't straight, and it drifts.
                const edge = t + 0.14 * Math.sin(4 * Math.PI * t - 1.5 * root.wavePhase)
                const m = Math.max(0, Math.min(1, edge / 0.6))
                const k = root.mixing ? m * m * (3 - 2 * m) : 1
                const c = root.color
                return "rgba(" + Math.round(255 * (bar.r + (c.r - bar.r) * k)) + ","
                       + Math.round(255 * (bar.g + (c.g - bar.g) * k)) + ","
                       + Math.round(255 * (bar.b + (c.b - bar.b) * k)) + ","
                       + (bar.a + (c.a - bar.a) * k) + ")"
            }
            const blend = ctx.createLinearGradient(start, 0, end, 0)
            for (let i = 0; i <= 12; ++i)
                blend.addColorStop(i / 12, mixed(i / 12))
            ctx.strokeStyle = blend
            ctx.beginPath()
            for (let i = 0; i <= 36; ++i) {
                const t = i / 36
                const x = start + (end - start) * t
                const amplitude = root.waveAmplitude
                                  + (Math.max(root.waveAmplitude, 3) - root.waveAmplitude) * Math.sin(Math.PI * t)
                // (Mirrored going backward: the bar's wave is read from the other side.)
                const y = amplitude * Math.sin(2 * Math.PI * x * root.direction / root.waveLength - root.wavePhase)
                if (i === 0)
                    ctx.moveTo(x, y)
                else
                    ctx.lineTo(x, y)
            }
            ctx.stroke()

            ctx.strokeStyle = root.color.toString()
            ctx.beginPath()
            ctx.moveTo(5.6 * unit, -5.7 * unit)
            ctx.lineTo(11.3 * unit, 0)
            ctx.lineTo(5.6 * unit, 5.7 * unit)
            ctx.stroke()
        }
    }

    FrameAnimation {
        running: root.visible && root.moving && root.kind === "disc"
        onTriggered: glyph.rotation = (glyph.rotation + frameTime * 120) % 360
    }
}
