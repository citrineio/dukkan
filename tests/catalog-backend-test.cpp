#include "storebackend.h"
#include <QGuiApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QNetworkProxy>
#include <QTemporaryDir>
#include <QThread>
#include <cstdlib>
#include <iostream>
static void check(bool ok) { if (!ok) { std::cerr << "catalog backend check failed\n"; std::exit(1); } }
static void write(const QString &path, const QByteArray &bytes) { QFile f(path); check(f.open(QIODevice::WriteOnly)); check(f.write(bytes)==bytes.size()); }
int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy,"127.0.0.1",9));
    QTemporaryDir dir;
    qputenv("XDG_CACHE_HOME",dir.path().toUtf8());
    const auto path=dir.filePath("catalog.json");
    qputenv("ZAMAN_CATALOG_PATH",path.toUtf8());
    qputenv("PATH",dir.path().toUtf8()+":"+qgetenv("PATH"));
    const auto fake=dir.filePath("flatpak");
    write(fake,"#!/bin/sh\nprintf 'app/org.example.First/aarch64/stable\\napp/org.example.Second/aarch64/stable\\n'\n");
    check(QFile::setPermissions(fake,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
    write(path,R"({"schema_version":1,"games":[{"id":"org.example.First"}]})");
    StoreBackend store;
    check(store.games().size()==1 && store.catalogError().isEmpty());
    QElapsedTimer timer; timer.start();
    while ((!store.availabilityChecked() || !store.games()[0].toMap()["installed"].toBool()) && timer.elapsed()<3000) { app.processEvents(); QThread::msleep(5); }
    check(store.availabilityChecked());
    write(path,R"({"schema_version":1,"games":[{"id":"org.example.First"},{"id":"org.example.Second"}]})");
    store.refresh(); check(store.games().size()==2);
    write(path,R"({"schema_version":1,"games":[{"id":"org.example.First","enabled":false},{"id":"org.example.Second"}]})");
    store.refresh(); check(store.games().size()==1 && store.games()[0].toMap()["id"]=="org.example.Second");
    timer.restart();
    while ((!store.availabilityChecked() || !store.games()[0].toMap()["installed"].toBool()) && timer.elapsed()<3000) { app.processEvents(); QThread::msleep(5); }
    check(store.availabilityChecked());
    // A disable between refresh and Play must be checked before spawning zamanctl.
    write(path,R"({"schema_version":1,"games":[]})");
    store.playGame("org.example.Second"); check(!store.launchBusy() && !store.catalogError().isEmpty());
    write(path,"{"); store.refresh(); check(!store.catalogError().isEmpty() && !store.availabilityChecked());
    store.installGame("org.example.Second"); check(!store.busy());
    QFile::remove(path); store.refresh(); check(!store.catalogError().isEmpty());
    write(path,R"({"schema_version":1,"games":[]})"); store.refresh();
    check(store.games().isEmpty() && store.catalogError().isEmpty());
    return 0;
}
