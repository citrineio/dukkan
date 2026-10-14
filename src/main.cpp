#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QUrl>

#include "storebackend.h"
#include "gamepadbridge.h"
#include "appearanceprovider.h"

int main(int argc, char *argv[])
{
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Dukkan"));
    app.setOrganizationName(QStringLiteral("Kawn Electro"));
    app.setDesktopFileName(QStringLiteral("com.kawnelectro.Dukkan"));
    app.setApplicationVersion(QStringLiteral("0.3.1"));

    StoreBackend store;
    GamepadBridge pad;
    AppearanceProvider appearance;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("store"), &store);
    engine.rootContext()->setContextProperty(QStringLiteral("pad"), &pad);
    engine.rootContext()->setContextProperty(QStringLiteral("appearance"), &appearance);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) return 1;
    return app.exec();
}
