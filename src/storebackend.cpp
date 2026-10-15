#include "storebackend.h"
#include "gameartwork.h"
#include "gamecatalog.h"
#include <QDebug>

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

}

StoreBackend::StoreBackend(QObject *parent) : QObject(parent)
{
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
bool StoreBackend::loadCatalog()
{
    QVector<GameCatalog::Entry> entries;
    QString error;
    ++m_catalogGeneration;
    if (!GameCatalog::load(entries, error)) {
        qWarning().noquote() << error;
        m_catalogError = QStringLiteral("Game list unavailable. Press X to retry.");
        m_availabilityChecked = false;
        emit gamesChanged();
        return false;
    }
    QVector<Game> games;
    for (const auto &entry : entries) {
        if (!entry.enabled) continue;
        Game game;
        if (const auto *old = findGame(entry.id)) game = *old;
        game.id = entry.id; game.name = entry.name; game.genre = entry.genre;
        game.summary = entry.summary; game.description = entry.summary;
        game.tag = entry.tag; game.packageLogo = entry.packageLogo;
        game.available = false;
        games.append(game);
    }
    m_games = games;
    m_catalogError.clear();
    m_availabilityChecked = false;
    loadCache();
    loadMetadata();
    emit gamesChanged();
    return true;
}

bool StoreBackend::authorize(const QString &id)
{
    QVector<GameCatalog::Entry> entries;
    QString error;
    if (GameCatalog::load(entries, error)) {
        for (const auto &entry : entries) if (entry.id == id && entry.enabled) return true;
        error = QStringLiteral("Game is no longer enabled in the catalog");
    }
    qWarning().noquote() << error;
    m_catalogError = QStringLiteral("Game list changed or is unavailable. Press X to refresh.");
    emit gamesChanged();
    return false;
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
        const QString localIcon = GameArtwork::localIcon(g.id);
        obj[QStringLiteral("icon")] = localIcon.isEmpty() ? g.icon : localIcon;
        obj[QStringLiteral("logo")] = GameArtwork::localLogo(g.id, g.packageLogo);
        obj[QStringLiteral("screenshot")] = g.screenshot;
        obj[QStringLiteral("screenshots")] = g.screenshots;
        obj[QStringLiteral("downloadSize")] = g.downloadSize;
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
    game.icon = GameArtwork::safeUrl(obj.value(QStringLiteral("icon")).toString());
    game.screenshots = GameArtwork::screenshots(obj);
    game.screenshot = game.screenshots.isEmpty() ? QString()
        : game.screenshots.first().toMap().value(QStringLiteral("url")).toString();
}

void StoreBackend::loadMetadata()
{
    for (const auto &game : m_games) {
        QNetworkRequest req(QUrl(QStringLiteral("https://flathub.org/api/v2/appstream/") + game.id));
        req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Dukkan/0.2"));
        req.setTransferTimeout(12000);
        QNetworkReply *reply = m_network.get(req);
        const QString id = game.id;
        const int generation = m_catalogGeneration;
        connect(reply, &QNetworkReply::finished, this, [this, reply, id, generation]() {
            if (generation == m_catalogGeneration && reply->error() == QNetworkReply::NoError
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
    if (m_busy || m_launchBusy || m_removalCheck) return;
    ++m_listingGeneration;
    if (!loadCatalog()) return;
    const bool preserveActivity = m_refreshFromTransaction;
    m_refreshFromTransaction = false;
    if (!preserveActivity)
        setActivity(m_busy ? m_activity : QStringLiteral("Checking local Flatpak installation and Flathub availability…"));
    startListing({QStringLiteral("list"), QStringLiteral("--user"), QStringLiteral("--app"),
                  QStringLiteral("--columns=ref:f")}, true, preserveActivity);
    startListing({QStringLiteral("remote-ls"), QStringLiteral("--user"), QStringLiteral("--app"),
                  QStringLiteral("--arch=aarch64"), QStringLiteral("--columns=ref:f,download-size"),
                  QStringLiteral("flathub")}, false, preserveActivity);
}

void StoreBackend::startListing(const QStringList &arguments, bool installedList, bool preserveActivity)
{
    QProcess *p = new QProcess(this);
    const int gen = m_listingGeneration;
    connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, p, gen, installedList, preserveActivity](int exitCode, QProcess::ExitStatus status) {
        if (gen == m_listingGeneration) {
            if (status == QProcess::NormalExit && exitCode == 0) {
                const QString out = QString::fromUtf8(p->readAllStandardOutput());
                const QStringList rows = out.split(QRegularExpression(QStringLiteral("[\\r\\n]+")),
                                                   Qt::SkipEmptyParts);
                QSet<QString> ids;
                QHash<QString, QString> sizes;
                for (const QString &row : rows) {
                    const QStringList columns = row.split(QLatin1Char('\t'));
                    const QString ref = columns.first().trimmed();
                    ids.insert(ref);
                    if (!installedList && columns.size() >= 2) {
                        const QString size = columns.at(1).trimmed();
                        if (size.size() <= 32 && !size.isEmpty() && size.at(0).isDigit())
                            sizes.insert(ref, size);
                    }
                }
                for (auto &game : m_games) {
                    const QString ref = game.id + QStringLiteral("/aarch64/stable");
                    const bool exact = ids.contains(ref) || ids.contains(QStringLiteral("app/") + ref);
                    if (installedList) game.installed = exact;
                    else {
                        game.available = exact;
                        game.downloadSize = exact ? sizes.value(ref, sizes.value(QStringLiteral("app/") + ref)) : QString();
                    }
                }
                if (!installedList) m_availabilityChecked = true;
                emit gamesChanged();
                if (!m_busy && !installedList && !preserveActivity)
                    setActivity(QStringLiteral("Catalog ready. Select a game to view details."));
            } else {
                if (!installedList) {
                    m_availabilityChecked = false;
                    for (auto &game : m_games) { game.available = false; game.downloadSize.clear(); }
                    emit gamesChanged();
                }
                if (!m_busy && !preserveActivity)
                    setActivity(installedList
                        ? QStringLiteral("Could not read installed applications. Check Flatpak installation.")
                        : QStringLiteral("Cannot verify aarch64 availability. Check the Flathub remote/network."));
            }
        }
        p->deleteLater();
    });
    connect(p, &QProcess::errorOccurred, this, [this, p, installedList, preserveActivity](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && !m_busy && !preserveActivity)
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
    if (authorize(id)) startTransaction(id, true);
}

void StoreBackend::removeGame(const QString &id)
{
    Game *g = findGame(id);
    if (!g || !g->installed || m_busy || m_launchBusy || m_removalCheck) return;
    if (!authorize(id)) return;
    m_removalCheck = true;
    emit launchChanged();
    setActivity(QStringLiteral("Checking that the game has stopped…"));
    QProcess *probe = new QProcess(this);
    connect(probe, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, probe, id](int code, QProcess::ExitStatus status) {
        if (!m_removalCheck) { probe->deleteLater(); return; }
        const QString output = QString::fromUtf8(probe->readAllStandardOutput());
        m_removalCheck = false;
        emit launchChanged();
        if (code != 0 || status != QProcess::NormalExit) {
            setActivity(QStringLiteral("Uninstall unavailable: could not verify installed-game status."));
        } else if (output.contains(QStringLiteral("phase=Starting")) ||
                   output.contains(QStringLiteral("phase=Running")) ||
                   output.contains(QStringLiteral("phase=Stopping")) ||
                   output.contains(QStringLiteral("phase=CleanupFailed"))) {
            setActivity(QStringLiteral("Close the managed game before uninstalling it."));
        } else if (Game *game = findGame(id); game && game->installed && !m_busy) {
            startTransaction(id, false);
        }
        probe->deleteLater();
    });
    connect(probe, &QProcess::errorOccurred, this, [this, probe](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart) return;
        m_removalCheck = false;
        emit launchChanged();
        setActivity(QStringLiteral("Uninstall unavailable: zamanctl could not start."));
        probe->deleteLater();
    });
    probe->start(qEnvironmentVariable("ZAMANCTL", "/usr/local/bin/zamanctl"),
                 {QStringLiteral("installed-game"), QStringLiteral("status")});
}

void StoreBackend::playGame(const QString &id)
{
    Game *game = findGame(id);
    if (!game || !game->installed || m_busy || m_launchBusy || m_removalCheck) return;
    if (!authorize(id)) return;
    m_launchBusy = true;
    emit launchChanged();
    setActivity(QStringLiteral("Starting ") + game->name + QStringLiteral("…"));
    QProcess *proc = new QProcess(this);
    m_launch = proc;
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, proc](int code, QProcess::ExitStatus status) {
        if (proc != m_launch) return;
        const QString error = QString::fromUtf8(proc->readAllStandardError()).trimmed();
        m_launch = nullptr;
        m_launchBusy = false;
        emit launchChanged();
        setActivity(code == 0 && status == QProcess::NormalExit
            ? QStringLiteral("Game launch accepted.")
            : QStringLiteral("Play failed: ") + (error.isEmpty() ? QStringLiteral("sessiond launch unavailable") : error.left(220)));
        proc->deleteLater();
    });
    connect(proc, &QProcess::errorOccurred, this, [this, proc](QProcess::ProcessError error) {
        if (proc != m_launch || error != QProcess::FailedToStart) return;
        m_launch = nullptr;
        m_launchBusy = false;
        emit launchChanged();
        setActivity(QStringLiteral("Play unavailable: zamanctl could not start."));
        proc->deleteLater();
    });
    proc->start(qEnvironmentVariable("ZAMANCTL", "/usr/local/bin/zamanctl"),
                {QStringLiteral("installed-game"), QStringLiteral("launch"), id});
}

void StoreBackend::startTransaction(const QString &id, bool install)
{
    if (m_busy || m_launchBusy || m_removalCheck || !authorize(id)) return;
    m_busy = true;
    m_activeGame = id;
    m_operation = install ? QStringLiteral("install") : QStringLiteral("remove");
    m_progressPercent = -1;
    m_progressStage = install ? QStringLiteral("Preparing installation…") : QStringLiteral("Preparing removal…");
    m_progressBuffer.clear();
    m_relaySucceeded = false;
    m_relayMessage.clear();
    emit progressChanged();
    emit transactionChanged();
    setActivity((install ? QStringLiteral("Installing ") : QStringLiteral("Removing ")) + id
                + QStringLiteral("… This may take some time."));
    if (!m_relayFile) {
        m_relayFile = new QTemporaryFile(QDir::tempPath() + QStringLiteral("/dukkan-flatpak-XXXXXX.py"), this);
        if (!m_relayFile->open()) {
            m_relayFile->deleteLater();
            m_relayFile = nullptr;
            endTransaction(false, QStringLiteral("Could not prepare Flatpak progress relay"));
            return;
        }
        QFile resource(QStringLiteral(":/tools/flatpak-transaction.py"));
        if (!resource.open(QIODevice::ReadOnly)) {
            m_relayFile->deleteLater();
            m_relayFile = nullptr;
            endTransaction(false, QStringLiteral("Flatpak progress relay resource is missing"));
            return;
        }
        const QByteArray source = resource.readAll();
        if (m_relayFile->write(source) != source.size() || !m_relayFile->flush()) {
            m_relayFile->deleteLater();
            m_relayFile = nullptr;
            endTransaction(false, QStringLiteral("Could not write Flatpak progress relay"));
            return;
        }
    }
    m_transaction = new QProcess(this);
    m_transaction->setProcessChannelMode(QProcess::SeparateChannels);
    const QStringList args{m_relayFile->fileName(), install ? QStringLiteral("install") : QStringLiteral("uninstall"), id};
    QProcess *proc = m_transaction;
    connect(proc, &QProcess::readyReadStandardOutput, this, [this, proc]() {
        consumeRelayOutput(proc->readAllStandardOutput());
    });
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, proc](int code, QProcess::ExitStatus status) {
        if (proc != m_transaction) return;
        consumeRelayOutput(proc->readAllStandardOutput());
        const QString errors = QString::fromUtf8(proc->readAllStandardError()).trimmed();
        endTransaction(code == 0 && status == QProcess::NormalExit && m_relaySucceeded,
                       m_relayMessage.isEmpty() ? errors : m_relayMessage);
        proc->deleteLater();
    });
    connect(proc, &QProcess::errorOccurred, this, [this, proc](QProcess::ProcessError error) {
        if (proc != m_transaction || error != QProcess::FailedToStart) return;
        endTransaction(false, QStringLiteral("Could not start Python progress relay"));
        proc->deleteLater();
    });
    proc->start(QStringLiteral("/usr/bin/python3"), args);
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
    m_refreshFromTransaction = true;
    refresh();
    if (success) syncPegasusLibrary(id, op);
}

void StoreBackend::syncPegasusLibrary(const QString &id, const QString &operation)
{
    const int generation = ++m_librarySyncGeneration;
    setActivity((operation == QStringLiteral("install") ? QStringLiteral("Installed: ")
                : QStringLiteral("Removed: ")) + id + QStringLiteral(". Updating Pegasus library…"));
    QProcess *sync = new QProcess(this);
    connect(sync, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, sync, generation, id, operation](int code, QProcess::ExitStatus status) {
        if (generation == m_librarySyncGeneration && !m_busy && !m_launchBusy) {
            const QString result = (operation == QStringLiteral("install")
                ? QStringLiteral("Installed: ") : QStringLiteral("Removed: ")) + id;
            const QString error = QString::fromUtf8(sync->readAllStandardError()).trimmed();
            setActivity(code == 0 && status == QProcess::NormalExit
                ? result + QStringLiteral(". Pegasus library updated.")
                : result + QStringLiteral(". Pegasus sync failed: ")
                    + (error.isEmpty() ? QStringLiteral("run zamanctl library sync") : error.left(160)));
        }
        sync->deleteLater();
    });
    connect(sync, &QProcess::errorOccurred, this, [this, sync, generation, id](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart) return;
        if (generation == m_librarySyncGeneration && !m_busy && !m_launchBusy)
            setActivity(QStringLiteral("Transaction completed for ") + id
                        + QStringLiteral(". Pegasus sync could not start; run zamanctl library sync."));
        sync->deleteLater();
    });
    sync->start(qEnvironmentVariable("ZAMANCTL", "/usr/local/bin/zamanctl"),
                {QStringLiteral("library"), QStringLiteral("sync")});
}

void StoreBackend::cancelTransaction()
{
    if (!m_transaction || !m_busy) return;
    setActivity(QStringLiteral("Requesting transaction cancellation…"));
    m_transaction->terminate();
}

void StoreBackend::consumeRelayOutput(const QByteArray &data)
{
    m_progressBuffer += data;
    if (m_progressBuffer.size() > 65536)
        m_progressBuffer = m_progressBuffer.right(32768);
    int boundary;
    while ((boundary = m_progressBuffer.indexOf('\n')) >= 0) {
        const QByteArray row = m_progressBuffer.left(boundary);
        m_progressBuffer.remove(0, boundary + 1);
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(row, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) continue;
        const QJsonObject event = document.object();
        const QString type = event.value(QStringLiteral("type")).toString();
        if (type == QStringLiteral("progress")) {
            const QString stage = event.value(QStringLiteral("stage")).toString();
            if (!stage.isEmpty()) m_progressStage = stage;
            const QJsonValue percent = event.value(QStringLiteral("percent"));
            m_progressPercent = percent.isDouble() ? qBound(0, percent.toInt(), 100) : -1;
            emit progressChanged();
        } else if (type == QStringLiteral("result")) {
            m_relaySucceeded = event.value(QStringLiteral("success")).toBool(false);
            m_relayMessage = event.value(QStringLiteral("message")).toString();
        }
    }
}
