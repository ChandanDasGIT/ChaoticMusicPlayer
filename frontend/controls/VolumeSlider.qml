import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Mute toggle + volume slider + equalizer button. The mouse wheel changes the
// volume by the step set in Settings.
RowLayout {
    id: root

    signal equalizerRequested()

    spacing: Theme.spacing / 2

    WheelHandler {
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        onWheel: (event) => {
            const notches = event.angleDelta.y / 120
            PlayerController.muted = false
            PlayerController.volume = Math.max(0, Math.min(1,
                PlayerController.volume + notches * SettingsController.volumeStep / 100))
        }
    }

    ControlButton {
        iconName: PlayerController.muted || PlayerController.volume === 0 ? "volumeMute"
                : PlayerController.volume < 0.5 ? "volumeLow" : "volume"
        iconSize: 14
        onClicked: PlayerController.muted = !PlayerController.muted
    }

    ThemedSlider {
        Layout.fillWidth: true
        from: 0
        to: 1
        value: PlayerController.volume
        opacity: PlayerController.muted ? 0.5 : 1
        onMoved: PlayerController.volume = value
    }

    // Opens the equalizer (accent while it is on)
    ControlButton {
        iconName: "equalizer"
        iconSize: 13
        active: SettingsController.eqEnabled
        ToolTip.visible: hovered
        ToolTip.text: SettingsController.eqEnabled ? qsTr("Equalizer (on)") : qsTr("Equalizer")
        onClicked: root.equalizerRequested()
    }
}
