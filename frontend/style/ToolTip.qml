import QtQuick
import QtQuick.Templates as T

// The app's tooltip (ChaoticStyle; every other control falls back to Basic).
// Dark rounded card above the control, appears after a short pause with a
// small pop, wraps long text. Colors mirror frontend/Theme.qml (a style module
// can't import the app module).
T.ToolTip {
    id: control

    x: parent ? (parent.width - implicitWidth) / 2 : 0
    y: -implicitHeight - 6

    implicitWidth: Math.min(260, Math.max(implicitBackgroundWidth + leftInset + rightInset,
                                          contentItem.implicitWidth + leftPadding + rightPadding))
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             contentItem.implicitHeight + topPadding + bottomPadding)

    margins: 8
    topPadding: 6
    bottomPadding: 6
    leftPadding: 12
    rightPadding: 12

    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutsideParent | T.Popup.CloseOnReleaseOutsideParent

    // A short pause before it appears, so passing over buttons doesn't flash tooltips.
    enter: Transition {
        SequentialAnimation {
            PropertyAction { property: "opacity"; value: 0 }
            PauseAnimation { duration: 350 }
            ParallelAnimation {
                NumberAnimation { property: "opacity"; to: 1; duration: 140; easing.type: Easing.OutCubic }
                NumberAnimation { property: "scale"; from: 0.88; to: 1; duration: 200; easing.type: Easing.OutBack }
            }
        }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; to: 0; duration: 100 }
    }

    contentItem: Text {
        text: control.text
        font.pixelSize: 12
        color: "#ececf2"
        wrapMode: Text.Wrap
        horizontalAlignment: Text.AlignHCenter
    }

    background: Rectangle {
        color: "#f21f1f28"
        radius: 9
        border.color: "#2effffff"
    }
}
