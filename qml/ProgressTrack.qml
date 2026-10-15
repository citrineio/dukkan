import QtQuick

Rectangle {
    id: track
    objectName: "transactionProgressTrack"
    property int percent: -1
    property bool active: false
    property color accent: "#EAC28D"
    property real scaleFactor: 1
    color: "#343E4D"
    radius: height / 2
    clip: true
    Rectangle {
        objectName: "determinateProgressFill"
        visible: track.percent >= 0
        width: parent.width * Math.max(0, Math.min(100, track.percent)) / 100
        height: parent.height
        radius: parent.radius
        color: track.accent
        Behavior on width { NumberAnimation { duration: 180 } }
    }
    Rectangle {
        id: moving
        visible: track.percent < 0
        height: parent.height
        width: Math.max(40 * track.scaleFactor, parent.width * .28)
        radius: parent.radius
        color: track.accent
        NumberAnimation {
            target: moving
            property: "x"
            from: -moving.width
            to: track.width
            duration: 1350
            loops: Animation.Infinite
            running: track.active && track.visible && track.percent < 0
        }
    }
}
