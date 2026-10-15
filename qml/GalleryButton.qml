import QtQuick
import QtQuick.Controls

Button {
    id: control
    property color ink: "#F0E5D6"
    property color surface: "#202A37"
    property real scaleFactor: 1
    focusPolicy: Qt.NoFocus
    implicitWidth: label.implicitWidth + 28 * scaleFactor
    implicitHeight: 42 * scaleFactor
    opacity: enabled ? 1 : .4
    contentItem: Text {
        id: label
        text: control.text
        color: control.ink
        font.pixelSize: 16 * control.scaleFactor
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        color: control.surface
        radius: 9 * control.scaleFactor
        border.color: "#506070"
        border.width: control.down ? 2 : 1
    }
}
