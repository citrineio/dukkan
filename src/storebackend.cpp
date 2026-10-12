#include "storebackend.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QTextDocumentFragment>
#include <QUrl>

namespace {
QString cachePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
           + QStringLiteral("/flathub-catalog.json");
}

QString safeAssetUrl(const QString &candidate)
{
    const QUrl url(candidate);
    if (url.scheme() != QStringLiteral("https"))
        return {};
    // Only use the Flathub CDN or known Flathub website for UI artwork.
    const QString host = url.host().toLower();
    if (host == QStringLiteral("dl.flathub.org") || host == QStringLiteral("flathub.org"))
        return url.toString();
    return {};
}

QString screenshotUrl(const QJsonObject &obj)
{
    const QJsonArray shots = obj.value(QStringLiteral("screenshots")).toArray();
    for (const auto &shot : shots) {
        const auto item = shot.toObject();
        for (const auto &key : {"1248x702", "752x423", "624x351", "original", "source"}) {
            const QString url = safeAssetUrl(item.value(QLatin1String(key)).toString());
            if (!url.isEmpty()) return url;
        }
    }
    return {};
}
}

StoreBackend::StoreBackend(QObject *parent) : QObject(parent)
{
    loadSeeds();
    loadCache();
    loadMetadata();
    refresh();
}

StoreBackend::~StoreBackend()
{
    // Flatpak availability and installation checks are child QProcesses.
    // If the UI cannot load, do not destroy them while they are still running.
    // Disconnect callbacks first: they must not mutate the store during teardown.
    const auto processes = findChildren<QProcess *>();
    for (QProcess *process : processes) {
        QObject::disconnect(process, nullptr, this, nullptr);
        if (process->state() == QProcess::NotRunning)
            continue;
        process->terminate();
        if (!process->waitForFinished(1200)) {
            process->kill();
            process->waitForFinished(1200);
        }
    }
}

void StoreBackend::loadSeeds()
{
    QFile f(QStringLiteral(":/data/games.json"));
    if (!f.open(QIODevice::ReadOnly)) {
        setActivity(QStringLiteral("Catalog resource missing"));
        return;
    }
    const auto doc = QJsonDocument::fromJson(f.readAll());
    for (const auto &item : doc.array()) {
        const auto obj = item.toObject();
        Game game;
        game.id = obj.value(QStringLiteral("id")).toString();
        game.name = obj.value(QStringLiteral("name")).toString();
        game.genre = obj.value(QStringLiteral("genre")).toString();
        game.summary = obj.value(QStringLiteral("summary")).toString();
        game.description = game.summary;
        game.tag = obj.value(QStringLiteral("tag")).toString();
        if (!game.id.isEmpty()) m_games.append(game);
    }
}

QVariantList StoreBackend::games() const
{
    QVariantList list;
    for (const auto &g : m_games) {
        QVariantMap obj;
        obj[QStringLiteral("id")] = g.id;
        obj[QStringLiteral("name")] = g.name;
        obj[QStringLiteral("genre")] = g.genre;
        obj[QStringLiteral("summary")] = g.summary;
        obj[QStringLiteral("description")] = g.description;
        obj[QStringLiteral("tag")] = g.tag;
        obj[QStringLiteral("icon")] = g.icon;
        obj[QStringLiteral("screenshot")] = g.screenshot;
        obj[QStringLiteral("metadataSource")] = g.metadataSource;
        obj[QStringLiteral("installed")] = g.installed;
        obj[QStringLiteral("available")] = g.available;
        list.append(obj);
    }
    return list;
}

StoreBackend::Game *StoreBackend::findGame(const QString &id)
{
    for (auto &game : m_games)
        if (game.id == id) return &game;
    return nullptr;
}

void StoreBackend::loadCache()
{
    QFile file(cachePath());
    if (!file.open(QIODevice::ReadOnly)) return;
    const auto doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) return;
    const auto root = doc.object();
    for (auto &game : m_games) {
        if (!root.value(game.id).isObject()) continue;
        const auto info = root.value(game.id).toObject();
        m_metadataCache.insert(game.id, info);
        updateMetadata(game, info);
        game.metadataSource = QStringLiteral("cache");
    }
}

void StoreBackend::saveCache()
{
    QJsonObject root;
    for (auto i = m_metadataCache.cbegin(); i != m_metadataCache.cend(); ++i)
        root.insert(i.key(), i.value());
    const QString path = cachePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
        file.commit();
    }
}

void StoreBackend::updateMetadata(Game &game, const QJsonObject &obj)
{
    const QString name = obj.value(QStringLiteral("name")).toString();
    const QString summary = obj.value(QStringLiteral("summary")).toString();
    const QString description = obj.value(QStringLiteral("description")).toString();
    if (!name.isEmpty()) game.name = name;
    if (!summary.isEmpty()) game.summary = summary;
    if (!description.isEmpty()) {
        game.description = QTextDocumentFragment::fromHtml(description).toPlainText().trimmed();
        game.description = game.description.left(1400);
    }
    game.icon = safeAssetUrl(obj.value(QStringLiteral("icon")).toString());
    game.screenshot = screenshotUrl(obj);
}

void StoreBackend::loadMetadata()
{
    for (const auto &game : m_games) {
        QNetworkRequest req(QUrl(QStringLiteral("https://flathub.org/api/v2/appstream/") + game.id));
        req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Dukkan/0.2"));
        req.setTransferTimeout(12000);
        QNetworkReply *reply = m_network.get(req);
        const QString id = game.id;
        connect(reply, &QNetworkReply::finished, this, [this, reply, id]() {
            if (reply->error() == QNetworkReply::NoError
                && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
                const QByteArray bytes = reply->readAll();
                if (bytes.size() <= 1048576) {
                    const QJsonDocument doc = QJsonDocument::fromJson(bytes);
                    if (doc.isObject()) {
                        // Only associate metadata with the matching curated app ID.
                        const QJsonObject obj = doc.object();
                        if (obj.value(QStringLiteral("id")).toString() == id) {
                            if (Game *g = findGame(id)) {
                                updateMetadata(*g, obj);
                                g->metadataSource = QStringLiteral("online");
                                m_metadataCache[id] = obj;
                                saveCache();
                                emit gamesChanged();
                            }
                        }
                    }
                }
            }
            reply->deleteLater();
        });
    }
}

void StoreBackend::setActivity(const QString &message)
{
    m_activity = message;
    emit activityChanged();
}

void StoreBackend::refresh()
{
    const int gen = ++m_listingGeneration;
    Q_UNUSED(gen);
    setActivity(m_busy ? m_activity : QStringLiteral("Checking local Flatpak installation and Flathub availability…"));
    startListing({QStringLiteral("list"), QStringLiteral("--user"), QStringLiteral("--app"),
                  QStringLiteral("--columns=application")}, true);
    startListing({QStringLiteral("remote-ls"), QStringLiteral("--user"), QStringLiteral("--app"),
                  QStringLiteral("--arch=aarch64"), QStringLiteral("--columns=application"),
                  QStringLiteral("flathub")}, false);
}

void StoreBackend::startListing(const QStringList &arguments, bool installedList)
{
    QProcess *p = new QProcess(this);
    const int gen = m_listingGeneration;
    connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, p, gen, installedList](int exitCode, QProcess::ExitStatus status) {
        if (gen == m_listingGeneration) {
            if (status == QProcess::NormalExit && exitCode == 0) {
                const QString out = QString::fromUtf8(p->readAllStandardOutput());
                const QStringList rows = out.split(QRegularExpression(QStringLiteral("[\\r\\n]+")),
                                                   Qt::SkipEmptyParts);
                QSet<QString> ids;
                for (const QString &row : rows) ids.insert(row.trimmed());
                for (auto &game : m_games) {
                    if (installedList) game.installed = ids.contains(game.id);
                    else game.available = ids.contains(game.id);
                }
                if (!installedList) m_availabilityChecked = true;
                emit gamesChanged();
                if (!m_busy && !installedList)
                    setActivity(QStringLiteral("Catalog ready. Select a game to view details."));
            } else {
                if (!installedList) {
                    m_availabilityChecked = false;
                    for (auto &game : m_games) game.available = false;
                    emit gamesChanged();
                }
                if (!m_busy)
                    setActivity(installedList
                        ? QStringLiteral("Could not read installed applications. Check Flatpak installation.")
                        : QStringLiteral("Cannot verify aarch64 availability. Check the Flathub remote/network."));
            }
        }
        p->deleteLater();
    });
    connect(p, &QProcess::errorOccurred, this, [this, p, installedList](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && !m_busy)
            setActivity(installedList
                ? QStringLiteral("Flatpak executable not found.")
                : QStringLiteral("Unable to query Flathub; is Flatpak installed?"));
        if (error == QProcess::FailedToStart) p->deleteLater();
    });
    p->start(QStringLiteral("flatpak"), arguments);
}

void StoreBackend::installGame(const QString &id)
{
    Game *g = findGame(id);
    if (!g || !g->available || !m_availabilityChecked || g->installed || m_busy) return;
    startTransaction(id, true);
}

void StoreBackend::removeGame(const QString &id)
{
    Game *g = findGame(id);
    if (!g || !g->installed || m_busy) return;
    startTransaction(id, false);
}

void StoreBackend::startTransaction(const QString &id, bool install)
{
    if (m_busy) return;
    m_busy = true;
    m_activeGame = id;
    m_operation = install ? QStringLiteral("install") : QStringLiteral("remove");
    m_progressPercent = -1;
    m_progressStage = install ? QStringLiteral("Preparing installation…") : QStringLiteral("Preparing removal…");
    m_progressBuffer.clear();
    emit progressChanged();
    emit transactionChanged();
    setActivity((install ? QStringLiteral("Installing ") : QStringLiteral("Removing ")) + id
                + QStringLiteral("… This may take some time."));
    m_transaction = new QProcess(this);
    m_transaction->setProcessChannelMode(QProcess::MergedChannels);
    const QStringList args = install
        ? QStringList{QStringLiteral("install"), QStringLiteral("--user"),
                      QStringLiteral("--noninteractive"), QStringLiteral("--assumeyes"),
                      QStringLiteral("--arch=aarch64"), QStringLiteral("flathub"), id}
        : QStringList{QStringLiteral("uninstall"), QStringLiteral("--user"),
                      QStringLiteral("--noninteractive"), QStringLiteral("--assumeyes"), id};
    QProcess *proc = m_transaction;
    connect(proc, &QProcess::readyReadStandardOutput, this, [this, proc]() {
        consumeProgressOutput(proc->readAllStandardOutput());
    });
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, proc](int code, QProcess::ExitStatus status) {
        if (proc != m_transaction) return;
        const QString output = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        if (!output.isEmpty()) consumeProgressOutput(output.toUtf8());
        endTransaction(code == 0 && status == QProcess::NormalExit, output);
        proc->deleteLater();
    });
    connect(proc, &QProcess::errorOccurred, this, [this, proc](QProcess::ProcessError error) {
        if (proc != m_transaction || error != QProcess::FailedToStart) return;
        endTransaction(false, QStringLiteral("Could not start flatpak executable"));
        proc->deleteLater();
    });
    proc->start(QStringLiteral("flatpak"), args);
}

void StoreBackend::endTransaction(bool success, const QString &details)
{
    const QString id = m_activeGame;
    const QString op = m_operation;
    m_transaction = nullptr;
    m_busy = false;
    m_activeGame.clear();
    m_operation.clear();
    m_progressPercent = -1;
    m_progressStage = success ? QStringLiteral("Completed") : QStringLiteral("Stopped or failed");
    m_progressBuffer.clear();
    emit progressChanged();
    emit transactionChanged();
    setActivity(success
        ? (op == QStringLiteral("install") ? QStringLiteral("Installed: ") : QStringLiteral("Removed: ")) + id
        : QStringLiteral("Transaction failed: ") + details.left(220));
    emit transactionFinished(id, op, success);
    refresh();
}

void StoreBackend::cancelTransaction()
{
    if (!m_transaction || !m_busy) return;
    setActivity(QStringLiteral("Requesting transaction cancellation…"));
    m_transaction->terminate();
}


// Flatpak's CLI emits human-readable progress (often with carriage returns).
// Percentages are best-effort *current operation-step* values, NOT guaranteed
// transaction totals; with no reliable percentage the QML bar stays indeterminate.
void StoreBackend::inspectProgressLine(const QString &line)
{
    QString readable = line.simplified();
    readable.remove(QRegularExpression(QStringLiteral("\\x1B\\[[0-9;]*[A-Za-z]")));
    if (readable.isEmpty()) return;
    const QRegularExpression pctExpr(QStringLiteral("(?:^|[^0-9])([0-9]{1,3})\\s*%"));
    const auto match = pctExpr.match(readable);
    if (match.hasMatch()) {
        bool valid = false;
        const int pct = match.captured(1).toInt(&valid);
        if (valid && pct >= 0 && pct <= 100) m_progressPercent = pct;
    }
    m_progressStage = readable.left(180);
    emit progressChanged();
    setActivity(m_progressStage);
}

void StoreBackend::consumeProgressOutput(const QByteArray &data)
{
    if (data.isEmpty()) return;
    m_progressBuffer.append(data);
    // Keep memory bounded even if a tool produces unbroken diagnostic output.
    if (m_progressBuffer.size() > 8192)
        m_progressBuffer = m_progressBuffer.right(4096);
    const QString text = QString::fromUtf8(m_progressBuffer);
    const int boundary = qMax(text.lastIndexOf(QLatin1Char('\n')),
                              text.lastIndexOf(QLatin1Char('\r')));
    if (boundary < 0) {
        // A single un-terminated progress update may still include a percent.
        if (text.contains(QLatin1Char('%'))) inspectProgressLine(text);
        return;
    }
    const QString finished = text.left(boundary + 1);
    m_progressBuffer = text.mid(boundary + 1).toUtf8();
    const QStringList lines = finished.split(QRegularExpression(QStringLiteral("[\\r\\n]+")),
                                             Qt::SkipEmptyParts);
    if (!lines.isEmpty()) inspectProgressLine(lines.last());
}
