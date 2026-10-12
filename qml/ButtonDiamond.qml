import QtQuick

// Zaman-style face-button legend for a plain QtQuick process.
// The uploaded menu references MenuButtonDiamond, but does not bundle its source.
Item {
    id: guide
    implicitWidth: 108
    implicitHeight: 76
    property color accent: "#EAC28D"
    property color backgroundColor: "#202A37"
    property color inactiveText: "#BBC6D3"
    property var activeFaces: ["south", "east"]
    property string selectedPrompt: "Select"
    property string backPrompt: "Back"

    Repeater {
        model: [
            {face:"north", key:"Y", x:40, y:0},
            {face:"west", key:"X", x:8, y:23},
            {face:"east", key:"B", x:72, y:23},
            {face:"south", key:"A", x:40, y:46}
        ]
        delegate: Rectangle {
            required property var modelData
            readonly property bool isActive: guide.activeFaces.indexOf(modelData.face) >= 0
            x: modelData.x; y: modelData.y
            width: 33; height: 29; radius: 15
            color: isActive ? guide.accent : guide.backgroundColor
            border.width: 1
            border.color: isActive ? "#F0D4A8" : "#405D718C"
            Text {
                anchors.centerIn: parent
                text: modelData.key
                color: isActive ? "#29221C" : guide.inactiveText
                font.bold: true
                font.pixelSize: 13
            }
        }
    }
}
