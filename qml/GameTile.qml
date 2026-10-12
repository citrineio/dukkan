import QtQuick

Rectangle {
    id: tile
    property var game: ({})
    property var theme: ({
        surface: "#171D28", elevated: "#202A37", accent: "#EAC28D",
        text: "#F0E5D6", muted: "#BBC6D3", installed: "#88A8C9", focusMs: 210
    })
    property bool chosen: false
    property real scaleFactor: 1
    signal activated()

    radius: 16 * scaleFactor
    color: chosen ? theme.elevated : theme.surface
    border.width: chosen ? 3 : 1
    border.color: chosen ? theme.accent : "#364251"
    scale: chosen ? 1.018 : 1.0
    Behavior on scale { NumberAnimation { duration: theme.focusMs; easing.type: Easing.OutCubic } }
    Behavior on color { ColorAnimation { duration: theme.focusMs } }
    Behavior on border.color { ColorAnimation { duration: theme.focusMs } }
    clip: true

    GameArt {
        id: tileArt
        x: 8 * scaleFactor
        y: 8 * scaleFactor
        width: parent.width - 16 * scaleFactor
        height: parent.height * 0.56
        screenshotUrl: tile.game.screenshot || ""
        iconUrl: tile.game.icon || ""
        fallbackTitle: tile.game.name || "Game"
        backdrop: theme.elevated
        muted: theme.muted
    }
    Rectangle {
        visible: tile.chosen
        x: 14 * scaleFactor
        y: 15 * scaleFactor
        width: 53 * scaleFactor
        height: 4 * scaleFactor
        radius: 2
        color: theme.accent
    }
    Text {
        x: 20 * scaleFactor
        y: parent.height * 0.61
        width: parent.width - 40 * scaleFactor
        text: tile.game.name || ""
        color: tile.chosen ? theme.accent : theme.text
        font.pixelSize: 22 * scaleFactor
        font.weight: Font.DemiBold
        elide: Text.ElideRight
    }
    Text {
        x: 20 * scaleFactor
        y: parent.height * 0.61 + 35 * scaleFactor
        width: parent.width - 40 * scaleFactor
        text: tile.game.genre || ""
        color: theme.muted
        font.pixelSize: 15 * scaleFactor
        elide: Text.ElideRight
    }
    Rectangle {
        x: 20 * scaleFactor
        y: parent.height - 30 * scaleFactor
        width: stateText.implicitWidth + 20 * scaleFactor
        height: 23 * scaleFactor
        radius: 7 * scaleFactor
        color: tile.game.installed ? "#2D414B" : "#2D3038"
        Text {
            id: stateText
            anchors.centerIn: parent
            text: tile.game.installed ? "INSTALLED" : "FLATHUB"
            color: tile.game.installed ? theme.installed : theme.muted
            font.pixelSize: 11 * scaleFactor
            font.weight: Font.DemiBold
            font.letterSpacing: 1
        }
    }
    Rectangle {
        visible: tile.chosen
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        width: 44 * scaleFactor
        height: 5 * scaleFactor
        color: theme.accent
        radius: 2
    }
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onClicked: tile.activated()
    }
}
