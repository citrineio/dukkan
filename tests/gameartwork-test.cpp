#include "gameartwork.h"
#include <QCoreApplication>
#include <QDir>
#include <QImage>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QUrl>
#include <cstdio>
#include <cstdlib>

static void check(bool good, const char *message)
{
    if (!good) { std::fprintf(stderr, "%s\n", message); std::exit(1); }
}
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const auto metadata = QJsonDocument::fromJson(R"JSON({"screenshots":[
      {"sizes":[
        {"src":"https://dl.flathub.org/original.png","width":"1920"},
        {"src":"https://dl.flathub.org/display.png","width":1248},
        {"src":"https://dl.flathub.org/thumb.png","width":"624"}],"caption":"Race"},
      {"sizes":[{"src":"https://dl.flathub.org/original.png","width":1920}]},
      {"1248x702":"https://flathub.org/legacy.png"},
      {"sizes":[{"src":"https://evil.invalid/no.png","width":1248}]},
      {"sizes":[{"src":"file:///etc/passwd","width":1248}]}
    ]})JSON").object();
    const auto shots = GameArtwork::screenshots(metadata);
    check(shots.size() == 2, "one image per shot; duplicate and unsafe shots are omitted");
    const auto first = shots.first().toMap();
    check(first.value("url").toString().endsWith("display.png"), "choose 1248px detail image");
    check(first.value("fullUrl").toString().endsWith("original.png"), "retain full resolution");
    check(first.value("thumbnailUrl").toString().endsWith("thumb.png"), "retain thumbnail");
    check(first.value("caption") == "Race", "retain caption");
    check(shots.last().toMap().value("url").toString().endsWith("legacy.png"), "old cache supported");
    for (const QString &bad : {"http://dl.flathub.org/a", "https://dl.flathub.org.evil/a",
                              "https://user@flathub.org/a", "https://flathub.org:444/a"})
        check(GameArtwork::safeUrl(bad).isEmpty(), "reject untrusted artwork URL");
    check(GameArtwork::screenshots({}).isEmpty(), "missing screenshots are safe");

    QTemporaryDir temp;
    qputenv("XDG_DATA_HOME", temp.path().toUtf8());
    const QString id = "org.supertuxproject.SuperTux";
    const QString directory = temp.path() + "/zaman/game-artwork/" + id;
    QDir().mkpath(directory);
    QImage image(16, 8, QImage::Format_ARGB32); image.fill(Qt::red);
    check(image.save(directory + "/logo.png"), "fixture saves");
    check(GameArtwork::localLogo(id) == QUrl::fromLocalFile(directory + "/logo.png").toString(),
          "shared title logo is discovered");
    check(GameArtwork::localIcon(id).isEmpty(), "missing exported icon is safe");
    std::puts("PASS: live/legacy screenshot metadata, URL validation, local title discovery.");
}
