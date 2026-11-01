import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A new version of the app: offers it, then shows its download (Android only:
// updater is AppUpdater, null elsewhere).
Popup {
    id: root

    property var updater: null
    property string version
    readonly property bool downloading: updater !== null && updater.downloading
    readonly property bool needsPermission: updater !== null && updater.needsPermission
    readonly property string error: updater !== null ? updater.error : ""
    readonly property int progress: updater !== null ? updater.progress : 0

    anchors.centerIn: Overlay.overlay
    width: Math.min(Overlay.overlay ? Overlay.overlay.width - 2 * Theme.padding : 380, 380)
    modal: true
    onVisibleChanged: Util.openDialogs += visible ? 1 : -1
    onClosed: if (updater) updater.cancel() // (what was downloaded stays, for next time)
    focus: true
    padding: Theme.padding * 1.5
    // A tap beside the dialog shouldn't stop a download
    closePolicy: downloading ? Popup.CloseOnEscape : Popup.CloseOnEscape | Popup.CloseOnPressOutside

    Connections {
        target: root.updater
        function onUpdateAvailable(version) {
            root.version = version
            root.open()
        }
        function onInstallStarted() { root.close() }
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.anim }
        NumberAnimation { property: "scale"; from: 0.92; to: 1; duration: Theme.anim; easing.type: Theme.easingBack }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; to: 0; duration: Theme.animFast }
        NumberAnimation { property: "scale"; to: 0.96; duration: Theme.animFast }
    }

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radiusLarge * 1.5
        border.color: Theme.border
    }

    Overlay.modal: Rectangle {
        color: Theme.scrim
        Behavior on opacity { NumberAnimation { duration: Theme.anim } }
    }

    contentItem: ColumnLayout {
        spacing: Theme.padding

        Label {
            Layout.fillWidth: true
            text: root.needsPermission ? qsTr("Allow installing")
                : root.downloading ? qsTr("Downloading version %1").arg(root.version)
                : qsTr("Update available")
            color: Theme.text
            font.pixelSize: Theme.fontSizeLarge
            font.weight: Font.DemiBold
            wrapMode: Text.WordWrap
        }

        Label {
            Layout.fillWidth: true
            visible: !root.downloading
            text: root.needsPermission
                  ? qsTr("Android asks you to allow Tuned Chaos under “Install unknown apps”. Turn it on and come back: the update then installs.")
                  : qsTr("Version %1 is available. You have version %2.").arg(root.version).arg(Qt.application.version)
            color: Theme.textMuted
            font.pixelSize: Theme.fontSize
            wrapMode: Text.WordWrap
        }

        // The download
        Rectangle {
            Layout.fillWidth: true
            visible: root.downloading
            implicitHeight: 6
            radius: height / 2
            color: Theme.fill

            Rectangle {
                width: parent.width * root.progress / 100
                height: parent.height
                radius: height / 2
                color: Theme.accent
            }
        }

        Label {
            Layout.fillWidth: true
            visible: root.downloading
            text: qsTr("%1%").arg(root.progress)
            color: Theme.textMuted
            font.pixelSize: Theme.fontSizeSmall
        }

        Label {
            Layout.fillWidth: true
            visible: root.error !== ""
            text: root.error
            color: Theme.error
            font.pixelSize: Theme.fontSize
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: Theme.spacing

            PillButton {
                text: root.downloading ? qsTr("Cancel") : qsTr("Later")
                implicitHeight: 36
                onClicked: root.close()
            }

            PillButton {
                visible: !root.downloading
                text: root.needsPermission ? qsTr("Open settings")
                    : root.error !== "" ? qsTr("Try again")
                    : qsTr("Update")
                filled: true
                implicitHeight: 36
                onClicked: root.needsPermission ? root.updater.openInstallSettings() : root.updater.startUpdate()
            }
        }

        // (Its own line: three buttons don't fit next to each other on a phone)
        PillButton {
            Layout.alignment: Qt.AlignRight
            visible: !root.downloading && !root.needsPermission
            text: qsTr("Skip this version")
            transparentIdle: true
            iconColor: Theme.textMuted
            implicitHeight: 36
            onClicked: {
                root.updater.skipThisVersion()
                root.close()
            }
        }
    }
}
