import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    visible: true
    visibility: Window.FullScreen
    width: 1920
    height: 1080
    title: "Dukkan — Zaman OS"
    color: zamanTheme.bg

    QtObject {
        id: zamanTheme
        readonly property var values: appearance.tokens || ({})
        readonly property color bg: values.background || "#0C1018"
        readonly property color rail: values.rail || "#12161E"
        readonly property color surface: values.surface || "#171D28"
        readonly property color elevated: values.elevated || "#202A37"
        readonly property color accent: values.accent || "#EAC28D"
        readonly property color accentStrong: values.accentStrong || "#F0D4A8"
        readonly property color installed: values.installed || "#88A8C9"
        readonly property color text: values.text || "#F0E5D6"
        readonly property color muted: values.muted || "#BBC6D3"
        readonly property color border: values.border || "#5D718C"
        readonly property int radius: values.cornerRadius !== undefined ? values.cornerRadius : 20
        readonly property int focusMs: values.focusMs !== undefined ? values.focusMs : 210
    }

    property bool jobsOpen: false
    property string pendingJobId: ""
    readonly property var selectedTransaction: { var jobs = store.jobs; return selectedGame ? jobs.find(job => job.app === selectedGame.id && ["queued", "waiting", "running", "cancelling"].indexOf(job.phase) >= 0) || null : null; }
    readonly property bool selectedPending: { var jobs = store.jobs; return selectedGame ? store.gamePending(selectedGame.id) : false; }
    property string section: "Discover"
    property int selectedIndex: 0
    property int navIndex: 0
    property string focusArea: "catalog" // catalog | hero | nav | details | dialog | keyboard
    property int detailAction: 0 // 0 Play/Install, 1 Uninstall
    property bool detailsOpen: false
    property bool galleryFullscreen: false
    property int galleryIndex: 0
    property string detailGameId: ""
    readonly property var galleryImages: selectedGame ? (selectedGame.screenshots || []) : []
    readonly property var galleryImage: galleryImages.length ? galleryImages[galleryIndex] : null
    property bool keyboardOpen: false
    property string modal: ""
    property int modalChoice: 0
    property string pendingId: ""
    property string pendingName: ""
    property string searchQuery: ""
    property int keyRow: 0
    property int keyColumn: 0
    property int keyboardView: 0 // lower | upper | symbols
    property bool enableGamepadActions: active
    readonly property real uiScale: Math.max(0.72, Math.min(1.25, height / 1080))
    function px(n) { return Math.round(n * uiScale) }
    readonly property var keyRows: keyboardView === 2 ? [
        ["!","@","#","$","%","^","&","*","(",")"],
        ["-","_","=","+","[","]","{","}","\\","⌫"],
        [";",":","'","\"",",",".","<",">","/","?"],
        ["`","~","|","0","1","2","3","SPACE","CLEAR","DONE"]
    ] : [
        keyboardView === 1 ? ["Q","W","E","R","T","Y","U","I","O","P"] : ["q","w","e","r","t","y","u","i","o","p"],
        keyboardView === 1 ? ["A","S","D","F","G","H","J","K","L","⌫"] : ["a","s","d","f","g","h","j","k","l","⌫"],
        keyboardView === 1 ? ["Z","X","C","V","B","N","M","1","2","3"] : ["z","x","c","v","b","n","m","1","2","3"],
        ["4","5","6","7","8","9","0","SPACE","CLEAR","DONE"]
    ]
    readonly property string keyboardViewLabel: keyboardView === 0 ? "lowercase" : keyboardView === 1 ? "UPPERCASE" : "symbols"
    property var visibleGames: {
        var result = []
        var query = searchQuery.trim().toLowerCase()
        var games = store.games
        for (var i = 0; i < games.length; ++i) {
            var g = games[i]
            if (section === "Library" && !g.installed) continue
            if (query.length && (g.name + " " + g.genre + " " + g.id).toLowerCase().indexOf(query) < 0) continue
            result.push(g)
        }
        return result
    }
    property var selectedGame: visibleGames.length > 0
        ? visibleGames[Math.max(0, Math.min(selectedIndex, visibleGames.length - 1))] : null
    readonly property string activeTransactionName: {
        var games = store.games
        for (var i = 0; i < games.length; ++i)
            if (games[i].id === store.activeGame) return games[i].name
        return "Game"
    }
    property string modalTitle: modal === "install" ? "Install this game?"
        : modal === "remove" ? "Uninstall this game?" : modal === "retry" ? "Retry this transaction?" : "Stop this download?"
    property string modalMessage: modal === "install"
        ? "Dukkan will install " + pendingName + " from Flathub. "
          + (selectedGame && selectedGame.downloadSize ? "App download: " + selectedGame.downloadSize + ". " : "App download size unavailable. ")
          + "Additional runtimes may require more data."
        : modal === "remove"
        ? "Uninstall " + pendingName + " from this device? Saved application data will be retained."
        : modal === "retry" ? "A new transaction will verify the current installation before retrying." : "Request cancellation of the selected transaction? It may take a moment."
    property string negativeLabel: modal === "cancel" ? "Keep running" : "Go back"
    property string positiveLabel: modal === "install" ? "Install game" : modal === "remove" ? "Uninstall game" : modal === "retry" ? "Retry" : "Request stop"

    onVisibleGamesChanged: {
        if (selectedIndex >= visibleGames.length) selectedIndex = Math.max(0, visibleGames.length - 1)
    }
    onSelectedGameChanged: {
        if (!selectedGame || !selectedGame.installed) detailAction = 0
        var nextId = selectedGame ? selectedGame.id : ""
        if (nextId !== detailGameId) {
            detailGameId = nextId
            galleryIndex = 0
            galleryFullscreen = false
        }
    }
    onGalleryImagesChanged: {
        galleryIndex = Math.max(0, Math.min(galleryIndex, galleryImages.length - 1))
        if (!galleryImages.length) galleryFullscreen = false
    }
    function moveGallery(delta) {
        if (!detailsOpen || galleryImages.length < 2) return
        galleryIndex = (galleryIndex + delta + galleryImages.length) % galleryImages.length
        thumbnails.positionViewAtIndex(galleryIndex, ListView.Contain)
    }
    function openGallery() {
        if (!detailsOpen || modal !== "" || !galleryImages.length) return
        galleryFullscreen = true
    }
    function leaveStore() {
        root.close()
    }
    function changeSection(next) {
        if (next === "Activity") { jobsOpen = true; return; }
        section = next
        navIndex = next === "Discover" ? 0 : 1
        selectedIndex = 0
        searchQuery = ""
        detailsOpen = false
        galleryFullscreen = false
        focusArea = "catalog"
        shelf.positionViewAtBeginning()
    }
    function openDetails() {
        if (!selectedGame) return
        detailsOpen = true
        galleryFullscreen = false
        galleryIndex = 0
        focusArea = "details"
        detailAction = 0
        descriptionFlick.contentY = 0
    }
    function requestGameAction() {
        if (!selectedGame || store.catalogError || selectedPending || store.requestPending || !store.workerAvailable || store.launchBusy || store.removalCheck) return
        if (selectedGame.installed) { store.playGame(selectedGame.id); return }
        if (!selectedGame.installed && (!store.availabilityChecked || !selectedGame.available)) return
        pendingId = selectedGame.id
        pendingName = selectedGame.name
        modal = "install"
        modalChoice = 0
        focusArea = "dialog"
    }
    function requestUninstall() {
        if (!selectedGame || !selectedGame.installed || store.catalogError || selectedPending || store.requestPending || !store.workerAvailable || store.launchBusy || store.removalCheck) return
        pendingId = selectedGame.id
        pendingName = selectedGame.name
        modal = "remove"
        modalChoice = 0
        focusArea = "dialog"
    }
    function requestCancel(jobId) {
        pendingJobId = jobId || store.activeJob
        if (!pendingJobId) return
        modal = "cancel"
        modalChoice = 0
        focusArea = "dialog"
    }
    function dismissModal() {
        modal = ""
        modalChoice = 0
        focusArea = detailsOpen ? "details" : "catalog"
    }
    function executeModalChoice() {
        if (modalChoice === 1) {
            if (modal === "install") store.installGame(pendingId)
            else if (modal === "remove") store.removeGame(pendingId)
            else if (modal === "cancel") store.cancelJob(pendingJobId)
            else if (modal === "retry") store.retryJob(pendingJobId)
        }
        dismissModal()
    }
    function openKeyboard() {
        if (detailsOpen || modal !== "") return
        keyboardOpen = true
        keyboardView = 0
        keyRow = 0
        keyColumn = 0
        focusArea = "keyboard"
        keyboardLayer.forceActiveFocus()
    }
    function closeKeyboard() {
        keyboardOpen = false
        focusArea = "catalog"
        shelf.forceActiveFocus()
    }
    function keyActivate(label) {
        if (label === "DONE") { closeKeyboard(); return }
        if (label === "SPACE") searchQuery += " "
        else if (label === "⌫") searchQuery = searchQuery.slice(0, -1)
        else if (label === "CLEAR") searchQuery = ""
        else searchQuery += label
        selectedIndex = 0
        shelf.positionViewAtBeginning()
    }
    function cycleKeyboardView() { keyboardView = (keyboardView + 1) % 3 }
    function keyMove(direction) {
        if (direction === "left") keyColumn = Math.max(0, keyColumn - 1)
        else if (direction === "right") keyColumn = Math.min(keyRows[keyRow].length - 1, keyColumn + 1)
        else if (direction === "up") keyRow = Math.max(0, keyRow - 1)
        else if (direction === "down") keyRow = Math.min(keyRows.length - 1, keyRow + 1)
    }
    function navigateShelf(direction) {
        if (!visibleGames.length) return
        if (direction === "left" && selectedIndex === 0) { focusArea = "nav"; return }
        if (direction === "left") selectedIndex = Math.max(0, selectedIndex - 1)
        else if (direction === "right") selectedIndex = Math.min(visibleGames.length - 1, selectedIndex + 1)
        shelf.positionViewAtIndex(selectedIndex, ListView.Contain)
    }
    function handleAction(action) {
        if (!enableGamepadActions) return
        if (keyboardOpen) {
            if (["left","right","up","down"].indexOf(action) !== -1) keyMove(action)
            else if (action === "select") keyActivate(keyRows[keyRow][keyColumn])
            else if (action === "refresh") keyActivate("⌫")
            else if (action === "library") cycleKeyboardView()
            else if (action === "back" || action === "search") closeKeyboard()
            return
        }
        if (modal !== "") {
            if (action === "left" || action === "up") modalChoice = 0
            else if (action === "right" || action === "down") modalChoice = 1
            else if (action === "select") executeModalChoice()
            else if (action === "back") dismissModal()
            return
        }
        if (jobsOpen) { activityPage.input(action); return; }
        // Fullscreen is a separate input surface. Never let A/Y/X/Start reach
        // the store actions underneath it. B restores the exact detail state.
        if (galleryFullscreen) {
            if (action === "back") galleryFullscreen = false
            else if (action === "previousImage") moveGallery(-1)
            else if (action === "nextImage") moveGallery(1)
            return
        }
        if (detailsOpen && (action === "previousImage" || action === "nextImage")) {
            moveGallery(action === "previousImage" ? -1 : 1)
            return
        }
        if (detailsOpen && (action === "gallery" || action === "library")) {
            openGallery()
            return
        }
        if (action === "refresh") { if (root.selectedPending) requestCancel(root.selectedTransaction.id); else if (store.busy) { jobsOpen = true; } else store.refresh(); return }
        if (action === "library") { changeSection(section === "Library" ? "Discover" : "Library"); return }
        if (action === "search") { openKeyboard(); return }
        if (detailsOpen) {
            if (action === "back") { detailsOpen = false; focusArea = "catalog" }
            else if (action === "select") { if (detailAction === 1) requestUninstall(); else requestGameAction() }
            else if (action === "left") detailAction = 0
            else if (action === "right" && selectedGame && selectedGame.installed) detailAction = 1
            else if (action === "down") descriptionFlick.contentY = Math.max(0, Math.min(descriptionFlick.contentHeight - descriptionFlick.height, descriptionFlick.contentY + px(145)))
            else if (action === "up") descriptionFlick.contentY = Math.max(0, descriptionFlick.contentY - px(145))
            return
        }
        if (focusArea === "nav") {
            if (action === "up") navIndex = Math.max(0, navIndex - 1)
            else if (action === "down") navIndex = Math.min(2, navIndex + 1)
            else if (action === "right") focusArea = "catalog"
            else if (action === "select") changeSection(["Discover", "Library", "Activity"][navIndex])
            else if (action === "back") focusArea = "catalog"
            return
        }
        if (focusArea === "hero") {
            if (action === "select") openDetails()
            else if (action === "down") focusArea = "catalog"
            else if (action === "left") focusArea = "nav"
            else if (action === "back") focusArea = "catalog"
            return
        }
        if (action === "left" || action === "right") navigateShelf(action)
        else if (action === "up") focusArea = selectedGame ? "hero" : "nav"
        else if (action === "select") openDetails()
        else if (action === "back") { if (section !== "Discover") changeSection("Discover"); else leaveStore() }
    }

    Connections { target: pad; function onAction(name) { root.handleAction(name) } }
    Shortcut { sequence: "Left"; enabled: !root.keyboardOpen; onActivated: root.handleAction("left") }
    Shortcut { sequence: "Right"; enabled: !root.keyboardOpen; onActivated: root.handleAction("right") }
    Shortcut { sequence: "Up"; enabled: !root.keyboardOpen; onActivated: root.handleAction("up") }
    Shortcut { sequence: "Down"; enabled: !root.keyboardOpen; onActivated: root.handleAction("down") }
    Shortcut { sequence: "Return"; enabled: !root.keyboardOpen; onActivated: root.handleAction("select") }
    Shortcut { sequence: "Enter"; enabled: !root.keyboardOpen; onActivated: root.handleAction("select") }
    Shortcut { sequence: "Escape"; enabled: !root.keyboardOpen; onActivated: root.handleAction("back") }
    Shortcut { sequence: "PgUp"; enabled: !root.keyboardOpen; onActivated: root.handleAction("previousImage") }
    Shortcut { sequence: "PgDown"; enabled: !root.keyboardOpen; onActivated: root.handleAction("nextImage") }
    Shortcut { sequence: "Space"; enabled: root.detailsOpen && !root.keyboardOpen; onActivated: root.handleAction("gallery") }
    Shortcut { sequence: "Ctrl+Q"; onActivated: root.leaveStore() }
    Shortcut { sequence: "F5"; onActivated: root.handleAction("refresh") }
    Shortcut { sequence: "Ctrl+F"; onActivated: root.openKeyboard() }
    Shortcut { sequence: "F11"; onActivated: root.visibility = root.visibility === Window.FullScreen ? Window.Windowed : Window.FullScreen }

    // Depth without the overbearing neon glows of the earlier prototype.
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0; color: "#1B222D" }
            GradientStop { position: 0.46; color: zamanTheme.bg }
            GradientStop { position: 1; color: "#10151D" }
        }
    }
    Rectangle { x: root.width * .36; y: 0; width: root.width * .37; height: px(3); color: zamanTheme.accent; opacity: .45 }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: px(46)
        anchors.rightMargin: px(46)
        anchors.topMargin: px(24)
        anchors.bottomMargin: px(20)
        spacing: px(16)

        // Header deliberately quiet: the content, not controls, is the focal point.
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: px(78)
            spacing: px(20)
            Row {
                Layout.preferredWidth: px(300)
                Layout.alignment: Qt.AlignVCenter
                spacing: px(12)
                Rectangle { width: px(8); height: px(36); radius: px(4); color: zamanTheme.accent }
                Column {
                    spacing: px(2)
                    Text { text: "DUKKAN"; color: zamanTheme.text; font.pixelSize: px(29); font.weight: Font.Bold; font.letterSpacing: 2.4 }
                    Text { text: "THE ZAMAN STORE"; color: zamanTheme.muted; font.pixelSize: px(11); font.letterSpacing: 2 }
                }
            }
            Item { Layout.fillWidth: true }
            Rectangle {
                Layout.preferredWidth: px(330)
                Layout.preferredHeight: px(47)
                radius: px(12)
                color: zamanTheme.surface
                border.color: root.keyboardOpen ? zamanTheme.accent : "#344253"
                border.width: 1
                Row {
                    anchors.fill: parent
                    anchors.leftMargin: px(15)
                    spacing: px(14)
                    Text { y: (parent.height - height) / 2; text: "⌕"; color: zamanTheme.accent; font.pixelSize: px(28) }
                    Text {
                        y: (parent.height - height) / 2
                        width: px(265)
                        color: root.searchQuery ? zamanTheme.text : zamanTheme.muted
                        font.pixelSize: px(16)
                        text: root.searchQuery ? root.searchQuery : "Search the collection"
                        elide: Text.ElideRight
                    }
                }
                MouseArea { anchors.fill: parent; onClicked: root.openKeyboard() }
            }
            Rectangle {
                Layout.preferredWidth: px(156)
                Layout.preferredHeight: px(47)
                color: zamanTheme.surface
                radius: px(12)
                border.color: "#344253"
                Row {
                    anchors.centerIn: parent
                    spacing: px(9)
                    Rectangle { y: (parent.height - height) / 2; width: px(7); height: px(7); radius: width / 2; color: pad.connected ? "#8AC9AB" : zamanTheme.muted }
                    Text { y: (parent.height - height) / 2; text: pad.connected ? "Controller ready" : "Keyboard ready"; font.pixelSize: px(13); color: zamanTheme.muted }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: px(24)

            // Navigation lives in a separate low-contrast rail.
            Rectangle {
                Layout.preferredWidth: px(206)
                Layout.fillHeight: true
                radius: px(18)
                color: zamanTheme.rail
                border.color: "#273343"
                border.width: 1
                Column {
                    anchors.fill: parent
                    anchors.margins: px(17)
                    spacing: px(11)
                    Text { text: "EXPLORE"; color: zamanTheme.muted; font.pixelSize: px(12); font.letterSpacing: 2; topPadding: px(15); bottomPadding: px(14) }
                    Repeater {
                        model: ["Discover", "Library", "Activity"]
                        delegate: Rectangle {
                            required property int index
                            required property string modelData
                            width: parent.width
                            height: px(62)
                            radius: px(12)
                            readonly property bool highlighted: root.focusArea === "nav" && root.navIndex === index && !root.detailsOpen
                            color: highlighted ? zamanTheme.accent : "transparent"
                            border.color: highlighted ? zamanTheme.accent : "transparent"
                            Behavior on color { ColorAnimation { duration: zamanTheme.focusMs } }
                            Rectangle { x: 0; y: (parent.height - height) / 2; width: px(4); height: px(34); radius: 2; visible: root.section === modelData && !parent.highlighted; color: zamanTheme.accent }
                            Row {
                                y: (parent.height - height) / 2
                                anchors.left: parent.left
                                anchors.leftMargin: px(17)
                                spacing: px(14)
                                Text { text: index === 0 ? "◇" : "▦"; color: parent.parent.highlighted ? zamanTheme.bg : (root.section === modelData ? zamanTheme.accent : zamanTheme.muted); font.pixelSize: px(22); y: (parent.height - height) / 2 }
                                Text { text: modelData; color: parent.parent.highlighted ? zamanTheme.bg : zamanTheme.text; font.pixelSize: px(19); font.weight: root.section === modelData ? Font.DemiBold : Font.Normal; y: (parent.height - height) / 2 }
                            }
                            MouseArea { anchors.fill: parent; onClicked: root.changeSection(modelData) }
                        }
                    }
                    Item { width: 1; height: px(28) }
                    Rectangle { width: parent.width; height: 1; color: "#31404F" }
                    Text { text: "CURATED FOR"; color: zamanTheme.muted; font.pixelSize: px(11); font.letterSpacing: 1.5; topPadding: px(17) }
                    Text { text: "ARM64"; color: zamanTheme.text; font.pixelSize: px(21); font.weight: Font.Medium }
                    Text { text: "Native games from Flathub."; width: parent.width; wrapMode: Text.WordWrap; color: zamanTheme.muted; font.pixelSize: px(14); lineHeight: 1.3 }
                    Item { width: 1; height: px(14) }
                    Rectangle { width: parent.width; height: 1; color: "#31404F" }
                    Image { source: "../assets/dukkan-shopfront.png"; width: parent.width; height: Math.min(parent.width, root.px(155)); fillMode: Image.PreserveAspectFit; smooth: true }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: px(18)

                // Featured title updates as the user moves along the shelf.
                Rectangle {
                    id: featurePanel
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.min(px(424), root.height * .44)
                    radius: px(22)
                    color: zamanTheme.surface
                    border.width: 1
                    border.color: "#374352"
                    clip: true
                    Rectangle {
                        anchors.fill: parent
                        gradient: Gradient {
                            GradientStop { position: 0; color: "#252D39" }
                            GradientStop { position: .67; color: zamanTheme.surface }
                            GradientStop { position: 1; color: "#17212C" }
                        }
                    }
                    Rectangle { x: px(34); y: px(26); width: px(56); height: px(4); radius: 2; color: zamanTheme.accent }
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: px(34)
                        spacing: px(30)
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            spacing: px(10)
                            Text { text: root.section === "Discover" ? "FEATURED  /  DISCOVER" : "YOUR LIBRARY  /  INSTALLED"; color: zamanTheme.accent; font.pixelSize: px(13); font.letterSpacing: 2; font.weight: Font.DemiBold; topPadding: px(13) }
                            Item { Layout.preferredHeight: px(13) }
                            Text {
                                Layout.fillWidth: true
                                text: root.selectedGame ? root.selectedGame.name : "Nothing here yet"
                                color: zamanTheme.text
                                font.pixelSize: px(51)
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                                maximumLineCount: 2
                                wrapMode: Text.WordWrap
                            }
                            Text { text: root.selectedGame ? root.selectedGame.genre.toUpperCase() + "   ·   " + (root.selectedGame.installed ? "INSTALLED" : "FLATHUB") : "LIBRARY"; color: zamanTheme.installed; font.pixelSize: px(14); font.weight: Font.DemiBold; font.letterSpacing: 1.1 }
                            Text {
                                Layout.fillWidth: true
                                Layout.maximumHeight: px(100)
                                text: root.selectedGame ? root.selectedGame.summary : "Install a game from Discover to see it here."
                                color: zamanTheme.muted
                                font.pixelSize: px(21)
                                wrapMode: Text.WordWrap
                                lineHeight: 1.2
                                maximumLineCount: 3
                                elide: Text.ElideRight
                            }
                            Item { Layout.fillHeight: true }
                            Rectangle {
                                id: heroButton
                                Layout.preferredWidth: px(231)
                                Layout.preferredHeight: px(58)
                                radius: px(12)
                                color: root.focusArea === "hero" ? zamanTheme.accentStrong : root.selectedGame ? zamanTheme.accent : zamanTheme.elevated
                                border.color: root.focusArea === "hero" ? "#FFFFFF" : "transparent"
                                border.width: root.focusArea === "hero" ? 3 : 0
                                Row {
                                    anchors.centerIn: parent
                                    spacing: px(12)
                                    Text { text: "VIEW GAME"; color: root.selectedGame ? "#29221C" : zamanTheme.muted; font.pixelSize: px(16); font.weight: Font.Bold; y: (parent.height - height) / 2 }
                                    Text { text: "↗"; color: root.selectedGame ? "#29221C" : zamanTheme.muted; font.pixelSize: px(25); y: (parent.height - height) / 2 }
                                }
                                MouseArea { anchors.fill: parent; enabled: !!root.selectedGame; onClicked: { root.focusArea = "hero"; root.openDetails() } }
                            }
                            Item { Layout.preferredHeight: px(7) }
                        }
                        Rectangle {
                            Layout.preferredWidth: Math.max(px(265), Math.min(root.width * .305, px(520)))
                            Layout.fillHeight: true
                            Layout.topMargin: px(13)
                            Layout.bottomMargin: px(10)
                            radius: px(18)
                            color: "#111720"
                            clip: true
                            GameArt {
                                anchors.fill: parent
                                screenshotUrl: root.selectedGame ? root.selectedGame.screenshot : ""
                                iconUrl: root.selectedGame ? root.selectedGame.icon : ""
                                fallbackTitle: root.selectedGame ? root.selectedGame.name : "Dukkan"
                                backdrop: "#131F2C"
                                muted: zamanTheme.muted
                            }
                            Rectangle { anchors.fill: parent; radius: px(18); color: "transparent"; border.color: "#506175"; border.width: 1 }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: px(45)
                    Text { text: root.section === "Discover" ? "Made to play" : "Ready to play"; color: zamanTheme.text; font.pixelSize: px(25); font.weight: Font.DemiBold }
                    Item { Layout.fillWidth: true }
                    Text { text: "SELECT A GAME TO EXPLORE"; color: zamanTheme.muted; font.pixelSize: px(12); font.letterSpacing: 1.5 }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    ListView {
                        id: shelf
                        anchors.fill: parent
                        orientation: ListView.Horizontal
                        spacing: px(16)
                        interactive: true
                        clip: true
                        focus: root.focusArea === "catalog" && !root.detailsOpen
                        boundsBehavior: Flickable.StopAtBounds
                        model: root.visibleGames
                        delegate: GameTile {
                            required property int index
                            required property var modelData
                            width: Math.max(px(235), Math.min(px(354), (shelf.width - px(50)) / 3.75))
                            height: Math.min(shelf.height - px(14), px(322))
                            game: modelData
                            theme: zamanTheme
                            scaleFactor: root.uiScale
                            chosen: root.selectedIndex === index && root.focusArea === "catalog" && !root.detailsOpen && root.modal === "" && !root.keyboardOpen
                            onActivated: { root.selectedIndex = index; root.focusArea = "catalog"; root.openDetails() }
                        }
                        Component.onCompleted: forceActiveFocus()
                    }
                    Text {
                        anchors.centerIn: parent
                        visible: root.visibleGames.length === 0
                        text: root.section === "Library" ? "Your library is empty. Explore and install a game." : "No games match your search."
                        color: zamanTheme.muted
                        font.pixelSize: px(20)
                    }
                }
            }
        }

        // Each page owns one progress surface; details has its own inline view.
        TransactionProgress {
            objectName: "homeTransactionProgress"
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? implicitHeight : 0
            visible: store.busy && !root.detailsOpen
            transaction: store
            gameName: root.activeTransactionName
            scaleFactor: root.uiScale
            accent: zamanTheme.accent
            ink: zamanTheme.text
            muted: zamanTheme.muted
            surface: zamanTheme.elevated
            onStopRequested: root.requestCancel()
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: px(40)
            spacing: px(28)
            Text { Layout.fillWidth: true; elide: Text.ElideRight; text: store.activity || "ZAMAN  /  DUKKAN"; color: zamanTheme.muted; font.pixelSize: px(12); font.letterSpacing: 1.5 }
            Item { Layout.fillWidth: true }
            ControlHint { button: "A"; label: "Details"; ink: zamanTheme.text; secondary: zamanTheme.muted; scaleFactor: root.uiScale }
            ControlHint { button: "B"; label: "Back / exit"; ink: zamanTheme.text; secondary: zamanTheme.muted; scaleFactor: root.uiScale }
            ControlHint { button: "Y"; label: "Library"; ink: zamanTheme.text; secondary: zamanTheme.muted; scaleFactor: root.uiScale }
            ControlHint { button: "START"; label: "Search"; ink: zamanTheme.text; secondary: zamanTheme.muted; scaleFactor: root.uiScale }
        }
    }

    Connections {
        target: store
        function onNavigateTransaction(id) { root.jobsOpen = true; activityPage.selectId(id); }
    }
    Shortcut { sequence: "Ctrl+T"; enabled: !root.keyboardOpen && !root.modal; onActivated: root.jobsOpen = true }
    TransactionList {
        id: activityPage
        anchors.fill: parent
        z: 28
        visible: root.jobsOpen
        backend: store
        uiScale: root.uiScale
        onBackRequested: root.jobsOpen = false
        onCancelRequested: jobId => root.requestCancel(jobId)
        onRetryRequested: jobId => { root.pendingJobId = jobId; root.modal = "retry"; root.modalChoice = 0; }
        onGameRequested: appId => {
            root.changeSection("Discover");
            for (var i = 0; i < root.visibleGames.length; ++i) if (root.visibleGames[i].id === appId) {
                root.selectedIndex = i; root.jobsOpen = false; root.openDetails(); return;
            }
        }
    }

    Rectangle {
        z: 35
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: px(8)
        width: Math.min(parent.width - px(60), catalogMessage.implicitWidth + px(32))
        height: catalogMessage.implicitHeight + px(18)
        radius: px(6)
        color: zamanTheme.elevated
        visible: store.catalogError.length > 0
        Text {
            id: catalogMessage
            anchors.centerIn: parent
            width: parent.width - px(32)
            text: store.catalogError
            color: zamanTheme.accent
            font.pixelSize: px(18)
            wrapMode: Text.WordWrap
        }
    }

    // Dedicated detail surface: prominent information, compact description, one action.
    Rectangle {
        anchors.fill: parent
        z: 10
        visible: root.detailsOpen
        color: zamanTheme.bg
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: px(65)
            spacing: px(25)
            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: px(50)
                Text { text: "DUKKAN"; color: zamanTheme.accent; font.pixelSize: px(18); font.letterSpacing: 2; font.weight: Font.DemiBold }
                Text { text: " /  GAME DETAILS"; color: zamanTheme.muted; font.pixelSize: px(17) }
                Item { Layout.fillWidth: true }
                ControlHint { button: "B"; label: "Back to games"; ink: zamanTheme.text; secondary: zamanTheme.muted; scaleFactor: root.uiScale }
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: px(42)
                Rectangle {
                    Layout.preferredWidth: (root.width - px(172)) * .56
                    Layout.fillHeight: true
                    radius: px(23)
                    color: zamanTheme.surface
                    border.color: "#374352"
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: px(18)
                        spacing: px(16)
                        Item { Layout.fillHeight: true }
                        ScreenshotView {
                            Layout.fillWidth: true
                            Layout.preferredHeight: width * 9 / 16
                            imageUrl: root.galleryImage ? root.galleryImage.url : ""
                            ink: zamanTheme.muted
                            MouseArea {
                                anchors.fill: parent
                                enabled: root.galleryImages.length > 0
                                onClicked: root.openGallery()
                            }
                        }
                        ListView {
                            id: thumbnails
                            Layout.fillWidth: true
                            Layout.preferredHeight: px(100)
                            visible: root.galleryImages.length > 1
                            orientation: ListView.Horizontal
                            spacing: px(12)
                            clip: true
                            model: root.galleryImages
                            delegate: Rectangle {
                                required property int index
                                required property var modelData
                                width: px(160)
                                height: px(96)
                                radius: px(9)
                                color: "transparent"
                                // This marks the current image, not a second input focus.
                                border.width: index === root.galleryIndex ? 2 : 0
                                border.color: zamanTheme.muted
                                ScreenshotView {
                                    anchors.fill: parent
                                    anchors.margins: px(4)
                                    imageUrl: modelData.thumbnailUrl || modelData.url
                                    decodeWidth: 320
                                    decodeHeight: 180
                                    ink: zamanTheme.muted
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: root.galleryIndex = index
                                    onDoubleClicked: { root.galleryIndex = index; root.openGallery() }
                                }
                            }
                        }
                        Text {
                            Layout.fillWidth: true
                            text: root.galleryImage ? root.galleryImage.caption : ""
                            visible: text.length > 0
                            color: zamanTheme.muted
                            font.pixelSize: px(16)
                            elide: Text.ElideRight
                            horizontalAlignment: Text.AlignHCenter
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: px(40)
                            visible: root.galleryImages.length > 0
                            GalleryButton { ink: zamanTheme.text; surface: zamanTheme.elevated; scaleFactor: root.uiScale;
                                text: "L1  Previous"
                                enabled: root.galleryImages.length > 1
                                focusPolicy: Qt.NoFocus
                                onClicked: root.moveGallery(-1)
                            }
                            Item { Layout.fillWidth: true }
                            Text { text: (root.galleryIndex + 1) + " / " + root.galleryImages.length; color: zamanTheme.text; font.pixelSize: px(18) }
                            Item { Layout.fillWidth: true }
                            GalleryButton { ink: zamanTheme.text; surface: zamanTheme.elevated; scaleFactor: root.uiScale;
                                text: "R1  Next"
                                enabled: root.galleryImages.length > 1
                                focusPolicy: Qt.NoFocus
                                onClicked: root.moveGallery(1)
                            }
                        }
                        GalleryButton { ink: zamanTheme.text; surface: zamanTheme.elevated; scaleFactor: root.uiScale;
                            Layout.alignment: Qt.AlignHCenter
                            visible: root.galleryImages.length > 0
                            text: "Y  View fullscreen"
                            focusPolicy: Qt.NoFocus
                            onClicked: root.openGallery()
                        }
                        Item { Layout.fillHeight: true }
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: px(14)
                    Text { text: "GAME  /  " + (root.selectedGame ? root.selectedGame.genre.toUpperCase() : ""); color: zamanTheme.accent; font.pixelSize: px(15); font.letterSpacing: 1.5; font.weight: Font.DemiBold }
                    GameIdentity {
                        Layout.fillWidth: true
                        Layout.preferredHeight: px(112)
                        game: root.selectedGame
                        ink: zamanTheme.text
                        scaleFactor: root.uiScale
                    }
                    Text {
                        Layout.fillWidth: true
                        text: root.selectedGame ? root.selectedGame.summary : ""
                        color: zamanTheme.text
                        font.pixelSize: px(23)
                        wrapMode: Text.WordWrap
                        maximumLineCount: 3
                        elide: Text.ElideRight
                        lineHeight: 1.15
                    }
                    Flow {
                        Layout.fillWidth: true
                        Layout.preferredHeight: childrenRect.height
                        spacing: px(10)
                        Rectangle {
                            width: installedLabel.implicitWidth + px(27)
                            height: px(32)
                            color: "#2A3A43"
                            radius: px(8)
                            Text { id: installedLabel; anchors.centerIn: parent; text: root.selectedGame && root.selectedGame.installed ? "INSTALLED" : "FLATHUB"; color: zamanTheme.installed; font.pixelSize: px(13); font.weight: Font.DemiBold; font.letterSpacing: 1 }
                        }
                        Rectangle {
                            width: metadataLabel.implicitWidth + px(22)
                            height: px(32)
                            color: "#30313A"
                            radius: px(8)
                            Text { id: metadataLabel; anchors.centerIn: parent; text: root.selectedGame ? (root.selectedGame.metadataSource === "online" ? "FLATHUB · LIVE METADATA" : root.selectedGame.metadataSource === "cache" ? "CACHED METADATA" : "CURATED FALLBACK DATA") : ""; color: zamanTheme.muted; font.pixelSize: px(12); font.weight: Font.Medium }
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        visible: !!root.selectedGame && !root.selectedGame.installed
                        text: root.selectedGame && root.selectedGame.downloadSize
                            ? "APP DOWNLOAD  " + root.selectedGame.downloadSize + "  ·  RUNTIMES EXTRA"
                            : "APP DOWNLOAD SIZE UNAVAILABLE  ·  RUNTIMES EXTRA"
                        color: zamanTheme.muted
                        font.pixelSize: px(14)
                        elide: Text.ElideRight
                    }
                    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#35404E" }
                    Text { text: "ABOUT THE GAME"; color: zamanTheme.muted; font.pixelSize: px(13); font.weight: Font.DemiBold; font.letterSpacing: 1.5 }
                    Flickable {
                        id: descriptionFlick
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        contentWidth: width
                        contentHeight: descriptionText.implicitHeight
                        boundsBehavior: Flickable.StopAtBounds
                        Text {
                            id: descriptionText
                            width: descriptionFlick.width
                            text: root.selectedGame ? root.selectedGame.description : ""
                            color: zamanTheme.muted
                            font.pixelSize: px(18)
                            textFormat: Text.PlainText
                            wrapMode: Text.WordWrap
                            lineHeight: 1.25
                        }
                    }
                    TransactionProgress {
                        objectName: "detailTransactionProgress"
                        Layout.fillWidth: true
                        Layout.preferredHeight: implicitHeight
                        visible: root.selectedPending
                        stacked: true
                        transaction: ({operation: root.selectedTransaction ? root.selectedTransaction.operation : "", progressPercent: root.selectedTransaction && typeof root.selectedTransaction.percent === "number" ? root.selectedTransaction.percent : -1, progressStage: root.selectedTransaction ? root.selectedTransaction.stage : "", busy: root.selectedPending})
                        gameName: root.selectedGame ? root.selectedGame.name : ""
                        scaleFactor: root.uiScale
                        accent: zamanTheme.accent
                        ink: zamanTheme.text
                        muted: zamanTheme.muted
                        surface: zamanTheme.elevated
                        onStopRequested: root.requestCancel(root.selectedTransaction ? root.selectedTransaction.id : "")
                    }
                    RowLayout {
                        visible: !root.selectedPending
                        Layout.fillWidth: true
                        Layout.fillHeight: false
                        Layout.preferredHeight: px(72)
                        Layout.minimumHeight: px(72)
                        Layout.maximumHeight: px(72)
                        spacing: px(13)
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            radius: px(13)
                            color: root.selectedPending || store.requestPending || !store.workerAvailable || store.launchBusy || store.removalCheck || !root.selectedGame || (!root.selectedGame.installed && !root.selectedGame.available) || root.detailAction !== 0 ? zamanTheme.elevated : zamanTheme.accent
                            border.color: root.detailAction === 0 ? zamanTheme.accentStrong : "transparent"
                            border.width: root.detailAction === 0 ? 3 : 0
                            Text {
                                anchors.centerIn: parent
                                text: store.launchBusy ? "STARTING GAME…" : store.removalCheck ? "CHECKING GAME…"
                                      : !store.workerAvailable ? "SERVICE UNAVAILABLE"
                                      : store.requestPending ? "REQUESTING…"
                                      : !root.selectedGame ? "SELECT A GAME"
                                      : root.selectedGame.installed ? "PLAY"
                                      : !store.availabilityChecked ? "CHECKING AVAILABILITY"
                                      : root.selectedGame.available ? "INSTALL FROM FLATHUB" : "NOT AVAILABLE ON ARM64"
                                color: root.selectedPending || store.requestPending || !store.workerAvailable || store.launchBusy || store.removalCheck || !root.selectedGame || (!root.selectedGame.installed && !root.selectedGame.available) || root.detailAction !== 0 ? zamanTheme.text : "#29221C"
                                font.pixelSize: px(19)
                                font.weight: Font.DemiBold
                            }
                            MouseArea { anchors.fill: parent; enabled: !root.selectedPending && !store.requestPending && store.workerAvailable && !store.launchBusy && !store.removalCheck; onClicked: { root.detailAction = 0; root.requestGameAction() } }
                        }
                        Rectangle {
                            Layout.preferredWidth: px(205)
                            Layout.fillHeight: true
                            visible: !!root.selectedGame && root.selectedGame.installed
                            radius: px(13)
                            color: root.detailAction === 1 ? zamanTheme.accent : zamanTheme.elevated
                            border.color: root.detailAction === 1 ? zamanTheme.accentStrong : "#506070"
                            border.width: root.detailAction === 1 ? 3 : 1
                            Text { anchors.centerIn: parent; text: "UNINSTALL"; color: root.detailAction === 1 ? "#29221C" : zamanTheme.text; font.pixelSize: px(18); font.weight: Font.DemiBold }
                            MouseArea { anchors.fill: parent; enabled: !root.selectedPending && !store.requestPending && store.workerAvailable && !store.launchBusy && !store.removalCheck; onClicked: { root.detailAction = 1; root.requestUninstall() } }
                        }
                    }
                    Text { text: root.selectedPending ? "X  Request stop · Activity shows queue details" : "← / →  Action   ·   A  Select   ·   B  Back\n↑ / ↓  Scroll   ·   Y  View fullscreen"; color: zamanTheme.muted; font.pixelSize: px(13) }
                }
            }
        }
    }

    Rectangle {
        objectName: "fullscreenGallery"
        anchors.fill: parent
        z: 25
        visible: root.galleryFullscreen
        color: "#080B11"
        MouseArea { anchors.fill: parent; onClicked: {} }
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: px(35)
            spacing: px(18)
            RowLayout {
                Layout.fillWidth: true
                Text { text: root.selectedGame ? root.selectedGame.name : ""; color: zamanTheme.text; font.pixelSize: px(25) }
                Item { Layout.fillWidth: true }
                GalleryButton { ink: zamanTheme.text; surface: zamanTheme.elevated; scaleFactor: root.uiScale; text: "B  Back to details"; focusPolicy: Qt.NoFocus; onClicked: root.galleryFullscreen = false }
            }
            ScreenshotView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                imageUrl: root.galleryImage ? root.galleryImage.fullUrl : ""
                decodeWidth: 2560
                decodeHeight: 1440
                ink: zamanTheme.muted
            }
            RowLayout {
                Layout.fillWidth: true
                GalleryButton { ink: zamanTheme.text; surface: zamanTheme.elevated; scaleFactor: root.uiScale; text: "L1  Previous"; enabled: root.galleryImages.length > 1; focusPolicy: Qt.NoFocus; onClicked: root.moveGallery(-1) }
                Text {
                    Layout.fillWidth: true
                    text: root.galleryImage ? root.galleryImage.caption : ""
                    elide: Text.ElideRight
                    horizontalAlignment: Text.AlignHCenter
                    color: zamanTheme.muted
                    font.pixelSize: px(18)
                }
                Text { text: (root.galleryIndex + 1) + " / " + root.galleryImages.length; color: zamanTheme.text; font.pixelSize: px(18) }
                GalleryButton { ink: zamanTheme.text; surface: zamanTheme.elevated; scaleFactor: root.uiScale; text: "R1  Next"; enabled: root.galleryImages.length > 1; focusPolicy: Qt.NoFocus; onClicked: root.moveGallery(1) }
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        z: 30
        visible: root.modal !== ""
        color: "#d9091017"
        MouseArea { anchors.fill: parent; onClicked: {} }
        Rectangle {
            width: Math.min(root.width - px(65), px(790))
            height: px(385)
            anchors.centerIn: parent
            radius: px(22)
            color: zamanTheme.surface
            border.color: "#526071"
            border.width: 1
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: px(36)
                spacing: px(13)
                Text { text: root.modalTitle; color: zamanTheme.text; font.pixelSize: px(33); font.weight: Font.DemiBold }
                Text { Layout.fillWidth: true; text: root.modalMessage; color: zamanTheme.muted; font.pixelSize: px(19); wrapMode: Text.WordWrap; lineHeight: 1.2 }
                Text { text: (root.modal === "cancel" || root.modal === "retry") ? root.pendingJobId : root.pendingId; color: zamanTheme.muted; font.pixelSize: px(12) }
                Item { Layout.fillHeight: true }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: px(74)
                    spacing: px(16)
                    Repeater {
                        model: [root.negativeLabel, root.positiveLabel]
                        delegate: Rectangle {
                            required property int index
                            required property string modelData
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            radius: px(12)
                            color: root.modalChoice === index ? zamanTheme.accent : zamanTheme.elevated
                            border.color: root.modalChoice === index ? zamanTheme.accentStrong : "#384755"
                            border.width: root.modalChoice === index ? 3 : 1
                            Behavior on color { ColorAnimation { duration: zamanTheme.focusMs } }
                            Text { anchors.centerIn: parent; text: modelData; color: root.modalChoice === index ? "#29221C" : zamanTheme.text; font.pixelSize: px(20); font.weight: Font.DemiBold }
                            MouseArea { anchors.fill: parent; onClicked: { root.modalChoice = index; root.executeModalChoice() } }
                        }
                    }
                }
                Text { Layout.alignment: Qt.AlignHCenter; text: "D-PAD  Choose       A  Confirm       B  Go back"; font.pixelSize: px(13); color: zamanTheme.muted }
            }
        }
    }

    Rectangle {
        id: keyboardLayer
        anchors.fill: parent
        visible: root.keyboardOpen
        z: 45
        focus: root.keyboardOpen
        color: "#ee0b1018"
        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Left) { root.keyMove("left"); event.accepted = true }
            else if (event.key === Qt.Key_Right) { root.keyMove("right"); event.accepted = true }
            else if (event.key === Qt.Key_Up) { root.keyMove("up"); event.accepted = true }
            else if (event.key === Qt.Key_Down) { root.keyMove("down"); event.accepted = true }
            else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) { root.keyActivate(root.keyRows[root.keyRow][root.keyColumn]); event.accepted = true }
            else if (event.key === Qt.Key_Backspace) { root.keyActivate("⌫"); event.accepted = true }
            else if (event.key === Qt.Key_Escape) { root.closeKeyboard(); event.accepted = true }
            else if (!(event.modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier))
                     && event.text.length === 1 && /^[\x20-\x7e]$/.test(event.text)) {
                root.keyActivate(event.text === " " ? "SPACE" : event.text); event.accepted = true
            }
        }
        MouseArea { anchors.fill: parent; onClicked: {} }
        Rectangle {
            anchors.centerIn: parent
            width: Math.min(root.width - px(60), px(1070))
            height: px(630)
            radius: px(23)
            color: zamanTheme.surface
            border.color: "#4A5563"
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: px(35)
                spacing: px(16)
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "Search games"; font.pixelSize: px(32); font.weight: Font.DemiBold; color: zamanTheme.text }
                    Item { Layout.fillWidth: true }
                    Text { text: "Y  " + root.keyboardViewLabel; font.pixelSize: px(17); color: zamanTheme.accent }
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: px(65)
                    color: zamanTheme.elevated
                    radius: px(13)
                    Text { anchors.fill: parent; anchors.margins: px(17); verticalAlignment: Text.AlignVCenter; text: root.searchQuery.length ? root.searchQuery + "│" : "Type with your controller..."; color: root.searchQuery.length ? zamanTheme.text : zamanTheme.muted; font.pixelSize: px(21); elide: Text.ElideRight }
                }
                Text { text: root.visibleGames.length + " games match in the curated collection"; color: zamanTheme.muted; font.pixelSize: px(15) }
                Repeater {
                    model: root.keyRows.length
                    delegate: RowLayout {
                        required property int index
                        property int rowNumber: index
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: px(7)
                        Repeater {
                            model: root.keyRows[rowNumber]
                            delegate: Rectangle {
                                required property int index
                                required property string modelData
                                readonly property bool chosen: root.keyRow === rowNumber && root.keyColumn === index
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                radius: px(9)
                                color: chosen ? zamanTheme.accent : zamanTheme.elevated
                                border.color: chosen ? zamanTheme.accentStrong : "#3B4757"
                                border.width: chosen ? 2 : 1
                                Text { anchors.centerIn: parent; text: modelData; color: parent.chosen ? "#29221C" : zamanTheme.text; font.pixelSize: px(modelData.length > 3 ? 14 : 21); font.weight: Font.DemiBold }
                                MouseArea { anchors.fill: parent; onClicked: { root.keyRow = rowNumber; root.keyColumn = index; root.keyActivate(modelData) } }
                            }
                        }
                    }
                }
                Text { Layout.alignment: Qt.AlignHCenter; text: "D-PAD  Move     A  Type     Y  Change keyboard     X  Delete     B / START  Done"; color: zamanTheme.muted; font.pixelSize: px(14) }
            }
        }
    }
}
