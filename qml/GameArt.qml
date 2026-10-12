import QtQuick

// Artwork container used in both tiles and large feature panels.
// Icons are never stretched to fill widescreen artwork slots.
Item {
    id: art
    property string screenshotUrl: ""
    property string iconUrl: ""
    property string fallbackTitle: "GAME"
    property color backdrop: "#202A37"
    property color muted: "#BBC6D3"
    property bool preferScreenshot: true
    readonly property string artUrl: preferScreenshot && screenshotUrl.length ? screenshotUrl : iconUrl
    readonly property bool showingScreenshot: preferScreenshot && screenshotUrl.length > 0

    Rectangle { anchors.fill: parent; color: art.backdrop; radius: 12 }
    Rectangle {
        anchors.fill: parent
        radius: 12
        opacity: 0.16
        gradient: Gradient {
            GradientStop { position: 0; color: "#88A8C9" }
            GradientStop { position: 1; color: "#171D28" }
        }
    }
    Image {
        id: imageView
        anchors.centerIn: parent
        width: showingScreenshot ? Math.max(0, parent.width - 12) : Math.min(parent.width * 0.50, 210)
        height: showingScreenshot ? Math.max(0, parent.height - 12) : Math.min(parent.height * 0.62, 210)
        source: art.artUrl
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true
        asynchronous: true
    }
    Text {
        anchors.centerIn: parent
        width: parent.width - 44
        visible: imageView.status === Image.Error || (imageView.status === Image.Null && !art.artUrl.length)
        text: art.fallbackTitle
        horizontalAlignment: Text.AlignHCenter
        color: art.muted
        font.pixelSize: Math.min(25, Math.max(14, parent.height * 0.12))
        font.weight: Font.DemiBold
        wrapMode: Text.WordWrap
    }
}
