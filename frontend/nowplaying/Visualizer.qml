pragma ComponentBehavior: Bound

import QtQuick

// Spectrum of what is playing, under the volume bar. Style (Settings →
// Appearance): 0 = bars, 1 = mirrored bars, 2 = wave. Collapses when switched
// off; the analysis only runs while this is on screen.
Item {
    id: root

    property int style: SettingsController.visualizerStyle
    property color color: Theme.accent
    readonly property var levels: VisualizerController.bands
    readonly property int count: VisualizerController.bandCount
    readonly property real fullHeight: 40

    height: SettingsController.visualizerEnabled ? fullHeight : 0
    visible: height > 0
    clip: true

    Behavior on height { NumberAnimation { duration: Theme.anim; easing.type: Easing.OutCubic } }

    Binding {
        target: VisualizerController
        property: "shown"
        value: root.visible && root.Window.window !== null
               && root.Window.window.visibility !== Window.Minimized
               && root.Window.window.visibility !== Window.Hidden
    }

    // Bars and mirrored bars
    Row {
        id: bars

        readonly property real barWidth: Math.max(2, (root.width - (root.count - 1) * spacing) / root.count)

        anchors.horizontalCenter: parent.horizontalCenter
        y: 0
        height: root.fullHeight
        spacing: 2
        visible: root.style !== 2

        Repeater {
            model: root.count

            delegate: Rectangle {
                id: bar

                required property int index
                readonly property real level: root.levels[index] ?? 0
                readonly property bool mirrored: root.style === 1

                width: bars.barWidth
                height: Math.max(width, bars.height * level)
                y: mirrored ? (bars.height - height) / 2 : bars.height - height
                radius: width / 2
                color: root.color
                opacity: 0.35 + 0.65 * level
            }
        }
    }

    // Wave: a smooth filled curve through the band levels.
    Canvas {
        id: wave

        anchors.fill: parent
        anchors.bottomMargin: parent.height - root.fullHeight // keep the baseline while collapsing
        visible: root.style === 2
        renderStrategy: Canvas.Cooperative

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            const n = root.count
            const w = width, h = height
            if (n < 2 || w <= 0 || h <= 0)
                return
            const step = w / (n - 1)
            const yAt = i => h - 1.5 - (h - 3) * (root.levels[i] ?? 0)

            ctx.beginPath()
            ctx.moveTo(0, yAt(0))
            for (let i = 1; i < n; ++i) {
                // Through the midpoints, using the band points as control points.
                const x0 = (i - 1) * step, x1 = i * step
                ctx.quadraticCurveTo(x0, yAt(i - 1), (x0 + x1) / 2, (yAt(i - 1) + yAt(i)) / 2)
            }
            ctx.lineTo(w, yAt(n - 1))

            ctx.lineWidth = 2
            ctx.lineJoin = "round"
            ctx.strokeStyle = root.color
            ctx.stroke()

            ctx.lineTo(w, h)
            ctx.lineTo(0, h)
            ctx.closePath()
            const fill = ctx.createLinearGradient(0, 0, 0, h)
            fill.addColorStop(0, Qt.alpha(root.color, 0.45))
            fill.addColorStop(1, Qt.alpha(root.color, 0.02))
            ctx.fillStyle = fill
            ctx.fill()
        }

        Connections {
            target: VisualizerController
            enabled: wave.visible
            function onBandsChanged() { wave.requestPaint() }
        }
        onVisibleChanged: if (visible) requestPaint()
    }
}
