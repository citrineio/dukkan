import QtQuick

Row {
    id: hint
    property string button: "A"
    property string label: "Select"
    property color ink: "#F0E5D6"
    property color secondary: "#BBC6D3"
    property color ring: "#445362"
    property real scaleFactor: 1
    spacing: 9 * scaleFactor
    height: 30 * scaleFactor

    Rectangle {
        width: Math.max(28 * scaleFactor, letter.implicitWidth + 13 * scaleFactor)
        height: 28 * scaleFactor
        radius: 9 * scaleFactor
        color: "#252C37"
        border.width: 1
        border.color: hint.ring
        Text {
            id: letter
            anchors.centerIn: parent
            text: hint.button
            color: hint.ink
            font.pixelSize: 13 * scaleFactor
            font.weight: Font.DemiBold
        }
    }
    Text {
        y: (parent.height - height) / 2
        text: hint.label
        color: hint.secondary
        font.pixelSize: 14 * scaleFactor
    }
}
