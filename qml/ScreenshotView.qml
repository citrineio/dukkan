import QtQuick

Item {
    id: shot
    property string imageUrl: ""
    property color ink: "#BBC6D3"
    property int decodeWidth: 1248
    property int decodeHeight: 702
    Rectangle { anchors.fill: parent; color: "#111720"; radius: 10 }
    Image {
        id: image
        anchors.fill: parent
        source: shot.imageUrl
        sourceSize.width: shot.decodeWidth
        sourceSize.height: shot.decodeHeight
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        smooth: true
        mipmap: true
    }
    Text {
        anchors.centerIn: parent
        width: parent.width - 24
        visible: image.status !== Image.Ready
        text: !shot.imageUrl.length ? "No screenshots available"
            : image.status === Image.Error ? "Image unavailable" : "Loading image…"
        color: shot.ink
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        font.pixelSize: Math.min(22, Math.max(11, parent.height * .05))
    }
}
