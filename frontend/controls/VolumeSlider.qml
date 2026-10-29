import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Mute toggle + volume slider + equalizer and sleep timer buttons. The mouse
// wheel changes the volume by the step set in Settings.
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

    // The sleep timer (accent while it runs; the tooltip says what is left)
    ControlButton {
        id: sleepButton
        iconName: "moon"
        iconSize: 13
        active: SleepTimerController.active
        ToolTip.visible: hovered
        ToolTip.text: !SleepTimerController.active ? qsTr("Sleep timer")
                    : SleepTimerController.endOfSong ? qsTr("Pausing at the end of this song")
                    : qsTr("Pausing in %1").arg(Util.formatTime(SleepTimerController.remainingMs))
        onClicked: sleepMenu.popup(sleepButton, 0, sleepButton.height + 4)

        ThemedMenu {
            id: sleepMenu

            Instantiator {
                model: [15, 30, 45, 60, 90]

                delegate: Action {
                    required property int modelData

                    text: qsTr("In %1 minutes").arg(modelData)
                    onTriggered: SleepTimerController.start(modelData)
                }

                onObjectAdded: (index, object) => sleepMenu.insertAction(index, object)
                onObjectRemoved: (index, object) => sleepMenu.removeAction(object)
            }

            Action {
                text: qsTr("At the end of this song")
                enabled: PlayerController.hasTrack
                onTriggered: SleepTimerController.startEndOfSong()
            }

            Action {
                text: qsTr("Turn off")
                enabled: SleepTimerController.active
                onTriggered: SleepTimerController.cancel()
            }
        }
    }
}
