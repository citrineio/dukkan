#include "gamecatalog.h"
#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <cstdlib>
#include <iostream>
void check(bool value) { if (!value) { std::cerr << "catalog check failed\n"; std::exit(1); } }
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QVector<GameCatalog::Entry> entries;
    QString error;
    check(GameCatalog::parse(R"({"schema_version":1,"games":[{"id":"org.example.Game"}]})",entries,error));
    check(entries.size()==1 && entries[0].name=="Game" && entries[0].enabled);
    for (const auto &bad : {R"({"schema_version":true,"games":[]})",R"({"schema_version":1,"games":[{"id":"../bad"}]})",R"({"schema_version":1,"games":[{"id":"org.example.Game","summary":"bad\nlaunch: bad"}]})",R"({"schema_version":1,"games":[{"id":"org.example.Game","package_logo":"../bad"}]})",R"({"schema_version":1,"games":[{"id":"org.example.Game"},{"id":"org.example.Game"}]})"})
        check(!GameCatalog::parse(bad,entries,error));
    QTemporaryDir dir;
    const auto path = dir.filePath("catalog.json");
    qputenv("ZAMAN_CATALOG_PATH",path.toUtf8());
    check(!GameCatalog::load(entries,error));
    QFile file(path); check(file.open(QIODevice::WriteOnly));
    file.write(R"({"schema_version":1,"games":[{"id":"org.example.NewGame","enabled":false}]})"); file.close();
    check(GameCatalog::load(entries,error)); check(!entries[0].enabled);
    check(file.open(QIODevice::WriteOnly)); file.write(R"({"schema_version":1,"games":[]})"); file.close();
    check(GameCatalog::load(entries,error) && entries.isEmpty());
    return 0;
}
