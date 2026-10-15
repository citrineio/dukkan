#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QQuickStyle>
#include <QImage>
#include <QEventLoop>
#include <QTimer>
#include <QTemporaryDir>
#include <QDir>
#include <QUrl>
#include <cstdio>
#include <cstdlib>

static bool qmlError = false;
static void messages(QtMsgType, const QMessageLogContext &, const QString &message)
{
    std::fprintf(stderr, "%s\n", qPrintable(message));
    if (message.contains("ReferenceError") || message.contains("TypeError")
        || message.contains("Binding loop") || message.contains("Cannot assign")
        || message.contains("Detected anchors")) qmlError = true;
}
static void check(bool good, const char *message)
{
    if (!good) { std::fprintf(stderr, "%s\n", message); std::exit(1); }
}
static void pump()
{
    QEventLoop loop; QTimer::singleShot(100, &loop, &QEventLoop::quit); loop.exec();
}
static void action(QObject *window, const char *name)
{
    check(QMetaObject::invokeMethod(window, "handleAction", Q_ARG(QVariant, QVariant(name))),
          "dispatch QML action");
    pump();
}
static QQuickItem *visibleTrack(QObject *window)
{
    QQuickItem *found = nullptr;
    for (auto *track : window->findChildren<QQuickItem *>("transactionProgressTrack")) {
        if (!track->isVisible()) continue;
        check(!found, "only one progress track is visible on the current page");
        found = track;
    }
    return found;
}
int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle("Basic");
    qInstallMessageHandler(messages);
    QTemporaryDir temp;
    QImage image(640, 360, QImage::Format_ARGB32); image.fill(QColor("#334C64"));
    check(image.save(temp.path() + "/shot.png"), "fixture screenshot saved");
    const QString imageUrl = QUrl::fromLocalFile(temp.path() + "/shot.png").toString();
    QQmlApplicationEngine engine;
    QQmlComponent mocks(&engine);
    mocks.setData(R"QML(import QtQml
QtObject {
    property var games: []
    property string catalogError: ""
    property bool busy: false
    property bool launchBusy: false
    property bool removalCheck: false
    property bool availabilityChecked: true
    property string activeGame: ""
    property string activity: "Ready"
    property string operation: ""
    property string progressStage: ""
    property int progressPercent: -1
    property bool connected: true
    property var tokens: ({})
    property int launches: 0
    property int removals: 0
    property int cancellations: 0
    signal action(string name)
    function playGame(id) { launches++ }
    function removeGame(id) { removals++ }
    function installGame(id) {}
    function refresh() {}
    function cancelTransaction() { cancellations++ }
})QML", QUrl());
    QObject *mock = mocks.create();
    check(mock, "mock QML compiles"); mock->setParent(&engine);
    QVariantList images;
    for (int index = 0; index < 3; ++index)
        images.append(QVariantMap{{"url", imageUrl}, {"fullUrl", imageUrl},
                                  {"thumbnailUrl", imageUrl}, {"caption", "Screenshot"}});
    QVariantMap game{{"id", "test.game.Game"}, {"name", "Test Game"}, {"genre", "Racing"},
                     {"summary", "A gallery test game"}, {"description", "Description for the game."},
                     {"installed", true}, {"available", true}, {"metadataSource", "cache"},
                     {"logo", imageUrl}, {"icon", imageUrl}, {"screenshot", imageUrl},
                     {"screenshots", images}, {"downloadSize", ""}};
    game.insert("tag", "RACING");
    mock->setProperty("games", QVariantList{game});
    for (const char *name : {"store", "pad", "appearance"})
        engine.rootContext()->setContextProperty(name, mock);
    engine.load(QUrl::fromLocalFile(QString(SOURCE_ROOT) + "/qml/Main.qml"));
    check(!engine.rootObjects().isEmpty(), "full Dukkan QML loads");
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    check(window, "root is a window");
    window->setVisibility(QWindow::Windowed);
    window->resize(1920, 1080);
    window->setProperty("enableGamepadActions", true);
    check(QMetaObject::invokeMethod(window, "openDetails"), "details opens"); pump();
    action(window, "right"); // Select Uninstall before opening the viewer.
    action(window, "nextImage");
    check(window->property("galleryIndex").toInt() == 1, "R1 changes image in details");
    const QString captures = qEnvironmentVariable("UI_CAPTURE_DIR");
    if (!captures.isEmpty()) {
        QDir().mkpath(captures);
        check(window->grabWindow().save(captures + "/details.png"), "capture details");
    }
    action(window, "library");
    check(window->property("galleryFullscreen").toBool(), "Y opens fullscreen");
    action(window, "nextImage");
    action(window, "select"); action(window, "refresh"); action(window, "search");
    check(mock->property("launches").toInt() == 0 && mock->property("removals").toInt() == 0,
          "viewer blocks destructive/store actions");
    if (!captures.isEmpty()) check(window->grabWindow().save(captures + "/fullscreen.png"), "capture viewer");
    action(window, "back");
    check(!window->property("galleryFullscreen").toBool() && window->property("detailsOpen").toBool(),
          "B returns to details");
    check(window->property("detailAction").toInt() == 1 && window->property("galleryIndex").toInt() == 2,
          "B preserves image and action");
    window->resize(1280, 720); pump();
    if (!captures.isEmpty()) check(window->grabWindow().save(captures + "/details-720p.png"), "capture 720p");
    action(window, "back");
    check(!window->property("detailsOpen").toBool(), "second B returns to catalog");

    // Real QObject notifications must drive the same numeric progress in each
    // page. No Flatpak process or transaction is started by this test.
    mock->setProperty("activeGame", "test.game.Game");
    mock->setProperty("operation", "install");
    mock->setProperty("progressStage", "Installing 2/4…");
    mock->setProperty("progressPercent", 37);
    mock->setProperty("busy", true);
    pump();
    auto *track = visibleTrack(window);
    check(track && track->property("percent").toInt() == 37, "home reports live percent");
    auto *homeTrack = track;
    check(QMetaObject::invokeMethod(window, "openDetails"), "open details during transaction");
    pump();
    track = visibleTrack(window);
    check(track && track->property("percent").toInt() == 37, "details inherits current percent");
    check(track != homeTrack, "detail surface replaces the hidden home progress surface");
    mock->setProperty("progressPercent", 72); pump();
    check(track->property("percent").toInt() == 72, "detail percent updates without reopening");
    if (!captures.isEmpty()) check(window->grabWindow().save(captures + "/detail-progress.png"), "capture detail progress");
    mock->setProperty("progressPercent", -1); pump();
    check(track->property("percent").toInt() == -1, "unknown percent remains indeterminate");
    QObject *detailProgress = window->findChild<QObject *>("detailTransactionProgress");
    check(detailProgress && QMetaObject::invokeMethod(detailProgress, "stopRequested"), "Stop is reachable");
    pump();
    check(window->property("modal").toString() == "cancel", "Stop opens existing confirmation");
    check(mock->property("cancellations").toInt() == 0, "Stop does not cancel before confirmation");
    action(window, "back");
    check(mock->property("busy").toBool(), "B leaves transaction running");
    mock->setProperty("progressPercent", 72);
    action(window, "back");
    track = visibleTrack(window);
    check(track && track->property("percent").toInt() == 72, "home retains progress after leaving details");
    if (!captures.isEmpty()) check(window->grabWindow().save(captures + "/home-progress.png"), "capture home progress");
    mock->setProperty("busy", false); pump();
    check(!visibleTrack(window), "finished transactions hide progress");
    check(!qmlError, "no QML reference, type, layout, or binding errors");
    std::puts("PASS: full QML, gallery, single live progress surface, and Stop confirmation.");
}
