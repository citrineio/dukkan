#include "appearanceprovider.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QColor>
#include <QVariant>
#include <QStringList>
#include <QJsonParseError>
#include <QtGlobal>

namespace {
QString checkedColor(const QJsonObject &obj, const QString &name, const QString &fallback)
{
    const QString candidate = obj.value(name).toString();
    const QColor color(candidate);
    return color.isValid() ? color.name(QColor::HexArgb) : fallback;
}
}

QVariantMap AppearanceProvider::defaults()
{
    return {
        {QStringLiteral("background"), QStringLiteral("#0C1018")},
        {QStringLiteral("rail"), QStringLiteral("#12161E")},
        {QStringLiteral("surface"), QStringLiteral("#171D28")},
        {QStringLiteral("elevated"), QStringLiteral("#202A37")},
        {QStringLiteral("accent"), QStringLiteral("#EAC28D")},
        {QStringLiteral("accentStrong"), QStringLiteral("#F0D4A8")},
        {QStringLiteral("installed"), QStringLiteral("#88A8C9")},
        {QStringLiteral("text"), QStringLiteral("#F0E5D6")},
        {QStringLiteral("muted"), QStringLiteral("#BBC6D3")},
        {QStringLiteral("border"), QStringLiteral("#5D718C")},
        {QStringLiteral("cornerRadius"), 20},
        {QStringLiteral("focusMs"), 210},
    };
}

AppearanceProvider::AppearanceProvider(QObject *parent)
    : QObject(parent),
      m_filePath(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
                 + QStringLiteral("/zaman/appearance.json")),
      m_tokens(defaults()),
      m_source(QStringLiteral("Dukkan fallback"))
{
    connect(&m_watcher, &QFileSystemWatcher::fileChanged,
            this, &AppearanceProvider::reload);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged,
            this, &AppearanceProvider::reload);
    reload();
}

void AppearanceProvider::rewatch()
{
    const QStringList oldPaths = m_watcher.files() + m_watcher.directories();
    if (!oldPaths.isEmpty()) m_watcher.removePaths(oldPaths);
    if (QFileInfo::exists(m_filePath)) m_watcher.addPath(m_filePath);
    QDir dir = QFileInfo(m_filePath).absoluteDir();
    // Watch nearest existing ancestor, so an appearance.json added later
    // becomes visible without needing to restart Dukkan.
    while (!dir.exists() && !dir.isRoot()) dir.cdUp();
    if (dir.exists()) m_watcher.addPath(dir.absolutePath());
}

void AppearanceProvider::reload()
{
    QVariantMap next = defaults();
    QString nextSource = QStringLiteral("Dukkan fallback");
    QFile file(m_filePath);
    if (file.open(QIODevice::ReadOnly)) {
        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
            const QJsonObject obj = doc.object();
            const QJsonObject colors = obj.value(QStringLiteral("colors")).toObject();
            for (const QString &key : {
                     QStringLiteral("background"), QStringLiteral("rail"),
                     QStringLiteral("surface"), QStringLiteral("elevated"),
                     QStringLiteral("accent"), QStringLiteral("accentStrong"),
                     QStringLiteral("installed"), QStringLiteral("text"),
                     QStringLiteral("muted"), QStringLiteral("border")}) {
                next.insert(key, checkedColor(colors, key, next.value(key).toString()));
            }
            const int radius = obj.value(QStringLiteral("shape")).toObject()
                                       .value(QStringLiteral("cornerRadius")).toInt(16);
            const int duration = obj.value(QStringLiteral("motion")).toObject()
                                         .value(QStringLiteral("focusMs")).toInt(140);
            next.insert(QStringLiteral("cornerRadius"), qBound(4, radius, 36));
            next.insert(QStringLiteral("focusMs"), qBound(0, duration, 500));
            nextSource = QStringLiteral("Zaman appearance.json");
        } else {
            nextSource = QStringLiteral("Dukkan fallback (invalid appearance.json)");
        }
    }
    rewatch();
    if (next != m_tokens || nextSource != m_source) {
        m_tokens = next;
        m_source = nextSource;
        emit tokensChanged();
    }
}
