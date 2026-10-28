import QtQuick

// Font Awesome glyph by name, e.g. Icon { name: "play" }. Names live in Theme.icons.
Text {
    property string name
    property real size: 16

    text: Theme.icons[name] ?? ""
    color: Theme.text
    font.family: Theme.iconFamily
    font.weight: Font.Black // the Solid face is weight 900
    font.pixelSize: size
    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter
}
