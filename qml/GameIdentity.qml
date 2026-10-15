import QtQuick
import QtQuick.Layouts

RowLayout {
    id: identity
    property var game: null
    property color ink: "#F0E5D6"
    property real scaleFactor: 1
    spacing: 18 * scaleFactor
    Item {
        Layout.preferredWidth: (titleArt.status === Image.Ready ? 190 : 94) * identity.scaleFactor
        Layout.preferredHeight: 108 * identity.scaleFactor
        visible: titleArt.status === Image.Ready || appIcon.status === Image.Ready
        Image {
            id: titleArt
            anchors.fill: parent
            source: identity.game ? (identity.game.logo || "") : ""
            sourceSize.width: 512
            sourceSize.height: 256
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            smooth: true
            mipmap: true
            visible: status === Image.Ready
        }
        Image {
            id: appIcon
            anchors.fill: parent
            source: identity.game ? (identity.game.icon || "") : ""
            sourceSize.width: 256
            sourceSize.height: 256
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            smooth: true
            mipmap: true
            visible: titleArt.status !== Image.Ready
        }
    }
    Text {
        Layout.fillWidth: true
        text: identity.game ? identity.game.name : ""
        color: identity.ink
        font.pixelSize: 45 * identity.scaleFactor
        font.weight: Font.DemiBold
        wrapMode: Text.WordWrap
        maximumLineCount: 2
        elide: Text.ElideRight
    }
}
