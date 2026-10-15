#include "gamecatalog.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

QString GameCatalog::path()
{
    return qEnvironmentVariable("ZAMAN_CATALOG_PATH", "/usr/local/share/zaman/catalog.json");
}
bool GameCatalog::parse(const QByteArray &bytes, QVector<Entry> &entries, QString &error)
{
    auto fail = [&error]() { error = QStringLiteral("Invalid shared game catalog"); return false; };
    if (bytes.size() > 1048576) return fail();
    QJsonParseError jsonError;
    const auto doc = QJsonDocument::fromJson(bytes, &jsonError);
    if (jsonError.error != QJsonParseError::NoError || !doc.isObject()) return fail();
    const auto root = doc.object();
    if (root.size() != 2 || !root.value("schema_version").isDouble()
        || root.value("schema_version").toDouble() != 1 || !root.value("games").isArray()) return fail();
    const auto rows = root.value("games").toArray();
    if (rows.size() > 512) return fail();
    const QSet<QString> keys{"id", "enabled", "name", "genre", "summary", "tag", "package_logo"};
    const QRegularExpression idPattern(QStringLiteral("\\A[A-Za-z][A-Za-z0-9_-]*(\\.[A-Za-z0-9_-]+){2,}\\z"));
    QSet<QString> seen;
    QVector<Entry> result;
    for (const auto &row : rows) {
        if (!row.isObject()) return fail();
        const auto obj = row.toObject();
        for (auto it = obj.begin(); it != obj.end(); ++it) if (!keys.contains(it.key())) return fail();
        Entry entry;
        entry.id = obj.value("id").toString();
        if (entry.id.size() > 255 || !idPattern.match(entry.id).hasMatch() || seen.contains(entry.id)) return fail();
        seen.insert(entry.id);
        if (obj.contains("enabled") && !obj.value("enabled").isBool()) return fail();
        entry.enabled = obj.value("enabled").toBool(true);
        const auto text = [&](const char *key, int max, QString &value) {
            if (!obj.contains(key)) return true;
            if (!obj.value(key).isString()) return false;
            const auto raw = obj.value(key).toString();
            if (raw.toUcs4().size() > max) return false;
            for (const auto c : raw) if (c.unicode() < 32 || c.unicode() == 127) return false;
            value = raw.trimmed(); return true;
        };
        if (!text("name",160,entry.name) || !text("genre",80,entry.genre)
            || !text("summary",600,entry.summary) || !text("tag",80,entry.tag)
            || !text("package_logo",512,entry.packageLogo)) return fail();
        if (entry.name.isEmpty()) entry.name = entry.id.section('.',-1);
        if (entry.genre.isEmpty()) entry.genre = QStringLiteral("Game");
        if (!entry.packageLogo.isEmpty()) {
            if (entry.packageLogo.startsWith('/') || entry.packageLogo.contains('\\')) return fail();
            for (const auto &part : entry.packageLogo.split('/'))
                if (part.isEmpty() || part == "." || part == "..") return fail();
        }
        result.append(entry);
    }
    entries = result; error.clear(); return true;
}
bool GameCatalog::load(QVector<Entry> &entries, QString &error)
{
    const auto filename = path();
    if (!QDir::isAbsolutePath(filename)) { error = QStringLiteral("Catalog path must be absolute"); return false; }
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) { error = QStringLiteral("Cannot read catalog: ") + filename; return false; }
    return parse(file.read(1048577), entries, error);
}
