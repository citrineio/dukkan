import QtQuick
import QtQuick.Layouts

Rectangle {
    id: progress
    property var transaction: null
    property string gameName: ""
    property bool stacked: false
    property real scaleFactor: 1
    property color accent: "#EAC28D"
    property color ink: "#F0E5D6"
    property color muted: "#BBC6D3"
    property color surface: "#202A37"
    signal stopRequested()
    implicitHeight: (stacked ? 112 : 60) * scaleFactor
    color: surface
    radius: 12 * scaleFactor
    border.color: "#4B5765"
    RowLayout {
        anchors.fill: parent
        anchors.margins: 12 * progress.scaleFactor
        spacing: 16 * progress.scaleFactor
        ColumnLayout {
            Layout.fillWidth: true
            spacing: (progress.stacked ? 10 : 4) * progress.scaleFactor
            RowLayout {
                Layout.fillWidth: true
                spacing: 12 * progress.scaleFactor
                Text {
                    text: progress.transaction && progress.transaction.operation === "install" ? "INSTALLING" : "UNINSTALLING"
                    color: progress.accent
                    font.pixelSize: (progress.stacked ? 15 : 13) * progress.scaleFactor
                    font.weight: Font.DemiBold
                }
                Text {
                    Layout.fillWidth: true
                    text: progress.gameName
                    color: progress.ink
                    elide: Text.ElideRight
                    font.pixelSize: (progress.stacked ? 19 : 14) * progress.scaleFactor
                }
                Text {
                    objectName: "transactionPercentLabel"
                    text: !progress.transaction || progress.transaction.progressPercent < 0 ? "WORKING"
                        : progress.transaction.progressPercent + "% · CURRENT STEP"
                    color: progress.accent
                    font.pixelSize: 13 * progress.scaleFactor
                    font.weight: Font.DemiBold
                }
            }
            Text {
                Layout.fillWidth: true
                visible: progress.stacked
                text: progress.transaction ? progress.transaction.progressStage : ""
                color: progress.muted
                font.pixelSize: 15 * progress.scaleFactor
                elide: Text.ElideRight
            }
            ProgressTrack {
                Layout.fillWidth: true
                Layout.preferredHeight: 8 * progress.scaleFactor
                percent: progress.transaction ? progress.transaction.progressPercent : -1
                active: !!progress.transaction && progress.transaction.busy
                accent: progress.accent
                scaleFactor: progress.scaleFactor
            }
        }
        GalleryButton {
            text: "X  Stop"
            ink: progress.ink
            surface: progress.surface
            scaleFactor: progress.scaleFactor
            enabled: !!progress.transaction && progress.transaction.busy
            onClicked: progress.stopRequested()
        }
    }
}
