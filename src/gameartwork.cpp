#include "gameartwork.h"

#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonArray>
#include <QSet>
#include <QStandardPaths>
#include <QUrl>

namespace {
QString dataRoot()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
}

QString readableImage(const QString &path)
{
    if (QFileInfo(path).size() > 16 * 1024 * 1024) return {};
    QImageReader reader(path);
    const QSize size = reader.size();
    if (!size.isValid() || size.width() > 8192 || size.height() > 8192
        || !reader.canRead()) return {};
    return QUrl::fromLocalFile(path).toString();
}
}

QString GameArtwork::safeUrl(const QString &candidate)
{
    const QUrl url(candidate);
    if (!url.isValid() || url.scheme() != QStringLiteral("https")
        || !url.userInfo().isEmpty() || (url.port() != -1 && url.port() != 443)) return {};
    const QString host = url.host().toLower();
    if (host == QStringLiteral("dl.flathub.org") || host == QStringLiteral("flathub.org"))
        return url.toString();
    return {};
}

QVariantList GameArtwork::screenshots(const QJsonObject &metadata)
{
    QVariantList result;
    QSet<QString> seen;
    for (const QJsonValue &value : metadata.value(QStringLiteral("screenshots")).toArray()) {
        const QJsonObject shot = value.toObject();
        QString display, full, thumbnail;
        int bestDisplay = -1, largest = -1, bestThumbnail = -1;
        for (const QJsonValue &sizeValue : shot.value(QStringLiteral("sizes")).toArray()) {
            const auto size = sizeValue.toObject();
            const QString url = safeUrl(size.value(QStringLiteral("src")).toString());
            const int width = size.value(QStringLiteral("width")).toVariant().toInt();
            if (url.isEmpty() || width <= 0) continue;
            if (width > largest) { largest = width; full = url; }
            if (width <= 1248 && width > bestDisplay) { bestDisplay = width; display = url; }
            if (width <= 624 && width > bestThumbnail) { bestThumbnail = width; thumbnail = url; }
        }
        // Cached catalogs from earlier API versions use dimension keys.
        if (display.isEmpty()) {
            for (const auto &key : {"1248x702", "752x423", "624x351", "original", "source"}) {
                display = safeUrl(shot.value(QLatin1String(key)).toString());
                if (!display.isEmpty()) break;
            }
        }
        if (full.isEmpty()) full = display;
        if (display.isEmpty()) display = full;
        if (thumbnail.isEmpty()) thumbnail = display;
        if (display.isEmpty() || seen.contains(full)) continue;
        seen.insert(full);
        result.append(QVariantMap{{QStringLiteral("url"), display},
                                  {QStringLiteral("fullUrl"), full},
                                  {QStringLiteral("thumbnailUrl"), thumbnail},
                                  {QStringLiteral("caption"), shot.value(QStringLiteral("caption")).toString().left(200)}});
        if (result.size() == 24) break; // Bound decoding and thumbnail work.
    }
    return result;
}

QString GameArtwork::localLogo(const QString &appId, const QString &packageLogo)
{
    const QString override = readableImage(dataRoot() + QStringLiteral("/zaman/game-artwork/")
                                            + appId + QStringLiteral("/logo.png"));
    if (!override.isEmpty()) return override;
    if (packageLogo.isEmpty()) return {};
    return readableImage(dataRoot() + QStringLiteral("/flatpak/app/") + appId
        + QStringLiteral("/aarch64/stable/active/files/") + packageLogo);
}

QString GameArtwork::localIcon(const QString &appId)
{
    const QString root = dataRoot() + QStringLiteral("/flatpak/exports/share/icons/hicolor/");
    for (const auto &size : {"512x512", "256x256", "128x128", "64x64", "48x48", "32x32"}) {
        const QString icon = readableImage(root + QLatin1String(size) + QStringLiteral("/apps/")
                                            + appId + QStringLiteral(".png"));
        if (!icon.isEmpty()) return icon;
    }
    return {};
}
