import QtQuick

Rectangle {
    id: activity
    property var backend
    property real uiScale: 1
    property string selectedId: ""
    readonly property var jobs: backend ? backend.jobs : []
    readonly property var selected: jobs.find(job => job.id === selectedId) || null
    signal backRequested()
    signal cancelRequested(string jobId)
    signal retryRequested(string jobId)
    signal gameRequested(string appId)
    color: "#F70C1018"
    function px(n) { return Math.round(n * uiScale); }
    function terminal(job) { return job && ["succeeded", "failed", "cancelled", "interrupted"].indexOf(job.phase) >= 0; }
    function selectId(id) { selectedId = id; sync(); }
    function sync() {
        if (!jobs.some(job => job.id === selectedId)) selectedId = jobs.length ? jobs[0].id : "";
        Qt.callLater(() => rows.positionViewAtIndex(Math.max(0, jobs.findIndex(job => job.id === selectedId)), ListView.Contain));
    }
    onJobsChanged: sync()
    function input(action) {
        if (action === "back") { backRequested(); return; }
        if (!selected) return;
        var index = jobs.findIndex(job => job.id === selectedId);
        if (action === "up" || action === "down") {
            selectedId = jobs[(index + (action === "up" ? -1 : 1) + jobs.length) % jobs.length].id;
            sync();
        } else if (action === "select") gameRequested(selected.app);
        else if (action === "refresh" && !terminal(selected)) cancelRequested(selected.id);
        else if (action === "library" && ["failed", "cancelled", "interrupted"].indexOf(selected.phase) >= 0) retryRequested(selected.id);
    }
    Column {
        x: activity.px(80); y: activity.px(55)
        width: parent.width - activity.px(160)
        spacing: activity.px(14)
        Text { text: "Activity"; color: "#F0E5D6"; font.pixelSize: activity.px(34); font.bold: true }
        Text {
            width: parent.width
            text: activity.backend && activity.backend.workerAvailable ? "Jobs continue when you return Home or close Dukkan." : "Transaction service unavailable. Accepted jobs remain saved."
            color: "#BBC6D3"; font.pixelSize: activity.px(18); wrapMode: Text.Wrap
        }
        Text { visible: !activity.jobs.length; text: "No transactions yet."; color: "#BBC6D3"; font.pixelSize: activity.px(22) }
    }
    ListView {
        id: rows
        x: activity.px(80); y: activity.px(150)
        width: parent.width - activity.px(160); height: parent.height - activity.px(350)
        model: activity.jobs; spacing: activity.px(8); clip: true
        delegate: Rectangle {
            required property var modelData
            readonly property bool chosen: activity.selectedId === modelData.id
            width: rows.width; height: activity.px(100); radius: activity.px(12)
            color: chosen ? "#EAC28D" : "#202A37"
            Column {
                x: activity.px(18); y: activity.px(10); width: parent.width - activity.px(36); spacing: activity.px(5)
                Text { width: parent.width; text: modelData.name + " · " + (modelData.operation === "install" ? "Install" : "Remove") + " · " + modelData.phase; color: chosen ? "#211E1B" : "#F0E5D6"; font.pixelSize: activity.px(22); elide: Text.ElideRight; textFormat: Text.PlainText }
                Text { width: parent.width; text: modelData.stage + (typeof modelData.percent === "number" ? " · " + modelData.percent + "%" : ""); color: chosen ? "#473627" : "#BBC6D3"; font.pixelSize: activity.px(17); elide: Text.ElideRight; textFormat: Text.PlainText }
                Text { width: parent.width; text: modelData.libraryError ? "Library refresh pending: " + modelData.libraryError : modelData.error || ""; color: chosen ? "#473627" : "#BBC6D3"; font.pixelSize: activity.px(14); elide: Text.ElideRight; textFormat: Text.PlainText }
            }
            MouseArea { anchors.fill: parent; onClicked: activity.selectId(modelData.id) }
        }
    }
    Text {
        x: activity.px(80); y: parent.height - activity.px(180); width: parent.width - activity.px(160)
        text: activity.selected ? "Transaction " + activity.selected.id + "\n" + (activity.selected.result || activity.selected.stage) : ""
        color: "#BBC6D3"; font.pixelSize: activity.px(16); textFormat: Text.PlainText
    }
    Text {
        x: activity.px(80); y: parent.height - activity.px(85); width: parent.width - activity.px(160)
        text: "↑ / ↓  Select     A  Game     " + (activity.selected && !activity.terminal(activity.selected) ? "X  Cancel     " : activity.selected && activity.selected.phase !== "succeeded" ? "Y  Retry     " : "") + "B  Back"
        color: "#F0E5D6"; font.pixelSize: activity.px(20)
    }
}
