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
#include <QTimer>
#include <QUuid>
#include <QLocalSocket>
#include <unistd.h>

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
    connect(&m_jobTimer, &QTimer::timeout, this, [this]() {
        if (!m_pollPending) transactionRequest({{QStringLiteral("action"), QStringLiteral("snapshot")}}, true);
    });
    m_jobTimer.start(1000);
    QTimer::singleShot(0, this, [this]() { transactionRequest({{QStringLiteral("action"), QStringLiteral("snapshot")}}, true); });
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
    if (m_launchBusy || m_removalCheck) return;
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
    if (!g || !g->available || !m_availabilityChecked || g->installed || gamePending(id) || m_requestPending || !m_workerAvailable) return;
    if (authorize(id)) startTransaction(id, true);
}

void StoreBackend::removeGame(const QString &id)
{
    Game *game = findGame(id);
    if (!game || !game->installed || gamePending(id) || m_launchBusy || m_requestPending || !m_workerAvailable) return;
    if (authorize(id)) startTransaction(id, false);
}

void StoreBackend::playGame(const QString &id)
{
    Game *game = findGame(id);
    if (!game || !game->installed || m_launchBusy || m_removalCheck) return;
    if (m_busy) { setActivity(QStringLiteral("Finish or cancel pending transactions before starting a managed game.")); return; }
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

bool StoreBackend::gamePending(const QString &id) const
{
    for (const auto &value : m_jobs) {
        const auto job = value.toMap();
        const auto phase = job.value(QStringLiteral("phase")).toString();
        if (job.value(QStringLiteral("app")).toString() == id &&
            !QStringList{"succeeded", "failed", "cancelled", "interrupted"}.contains(phase)) return true;
    }
    return false;
}

void StoreBackend::startTransaction(const QString &id, bool install)
{
    if (m_requestPending || !m_workerAvailable || gamePending(id) || !authorize(id)) return;
    transactionRequest({{QStringLiteral("action"), QStringLiteral("submit")},
        {QStringLiteral("app"), id}, {QStringLiteral("operation"), install ? "install" : "uninstall"},
        {QStringLiteral("request"), QUuid::createUuid().toString(QUuid::Id128)}});
}

void StoreBackend::cancelTransaction() { cancelJob(m_activeJob); }
void StoreBackend::cancelJob(const QString &id)
{
    if (!id.isEmpty() && !m_requestPending && m_workerAvailable)
        transactionRequest({{QStringLiteral("action"), QStringLiteral("cancel")}, {QStringLiteral("id"), id}});
}
void StoreBackend::retryJob(const QString &id)
{
    if (!id.isEmpty() && !m_requestPending && m_workerAvailable)
        transactionRequest({{QStringLiteral("action"), QStringLiteral("retry")}, {QStringLiteral("id"), id},
            {QStringLiteral("request"), QUuid::createUuid().toString(QUuid::Id128)}});
}

void StoreBackend::transactionRequest(const QJsonObject &request, bool poll)
{
    if (poll) m_pollPending = true;
    else { m_requestPending = true; emit jobsChanged(); }
    auto socket = new QLocalSocket(this);
    auto buffer = new QByteArray;
    auto finished = new bool(false);
    auto finish = [this, socket, buffer, finished, poll](bool success, const QString &error) {
        if (*finished) return;
        *finished = true;
        if (poll) m_pollPending = false;
        else m_requestPending = false;
        m_workerAvailable = success;
        if (!success) setActivity(error);
        emit jobsChanged();
        socket->abort();
        socket->deleteLater();
    };
    connect(socket, &QObject::destroyed, [buffer, finished]() { delete buffer; delete finished; });
    connect(socket, &QLocalSocket::connected, this, [socket, request]() {
        socket->write(QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n');
    });
    connect(socket, &QLocalSocket::readyRead, this, [this, socket, buffer, finished, finish, poll]() {
        if (*finished) return;
        *buffer += socket->readAll();
        if (buffer->size() > 2097152) { finish(false, QStringLiteral("Transaction service reply is too large")); return; }
        if (!buffer->contains('\n')) return;
        const auto object = QJsonDocument::fromJson(buffer->left(buffer->indexOf('\n'))).object();
        if (object.value("type").toString() != "transactions") {
            finish(false, object.value("error").toString(QStringLiteral("Invalid transaction service reply"))); return;
        }
        applyTransactionSnapshot(object);
        if (!poll) setActivity(QStringLiteral("Transaction request accepted. Work continues when Dukkan closes."));
        finish(true, {});
    });
    connect(socket, &QLocalSocket::errorOccurred, this, [finish](QLocalSocket::LocalSocketError) {
        finish(false, QStringLiteral("Transaction service unavailable. Accepted jobs remain saved; check dukkan-transactions.service."));
    });
    auto timer = new QTimer(socket);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, this, [finish]() { finish(false, QStringLiteral("Transaction reply timed out. Check Activity before submitting again.")); });
    timer->start(5000);
    const auto runtime = qEnvironmentVariable("XDG_RUNTIME_DIR", QStringLiteral("/run/user/") + QString::number(getuid()));
    socket->connectToServer(runtime + QStringLiteral("/dukkan-transactions/control.sock"));
}

void StoreBackend::applyTransactionSnapshot(const QJsonObject &snapshot)
{
    const auto previous = m_jobs;
    m_jobs = snapshot.value("jobs").toArray().toVariantList();
    m_activeJob.clear();
    m_activeGame.clear();
    m_operation.clear();
    m_progressStage.clear();
    m_progressPercent = -1;
    m_busy = false;
    for (const auto &value : m_jobs) {
        const auto job = value.toMap();
        const auto phase = job.value("phase").toString();
        const bool terminal = QStringList{"succeeded", "failed", "cancelled", "interrupted"}.contains(phase);
        if (!terminal && m_activeJob.isEmpty()) {
            m_busy = true;
            m_activeJob = job.value("id").toString();
            m_activeGame = job.value("app").toString();
            m_operation = job.value("operation").toString() == "install" ? "install" : "remove";
            m_progressStage = job.value("stage").toString();
            m_progressPercent = job.value("percent").isNull() ? -1 : job.value("percent").toInt();
        }
        if (terminal) for (const auto &oldValue : previous) {
            const auto old = oldValue.toMap();
            if (old.value("id") == job.value("id") && old.value("phase") != job.value("phase")) {
                emit transactionFinished(job.value("app").toString(), job.value("operation").toString(), phase == "succeeded");
                // Only refresh local installation state; the service owns Pegasus sync and notifications.
                startListing({"list", "--user", "--app", "--columns=ref:f"}, true, true);
            }
        }
    }
    emit transactionChanged();
    emit progressChanged();
    if (previous != m_jobs) emit jobsChanged();
    const auto navigation = snapshot.value("navigation").toObject();
    const auto nonce = navigation.value("nonce").toString();
    const auto target = navigation.value("id").toString();
    bool knownTarget = false;
    for (const auto &value : m_jobs) {
        if (value.toMap().value("id").toString() == target) { knownTarget = true; break; }
    }
    if (knownTarget && m_navigation.take(nonce, target)) {
        emit navigateTransaction(target);
    } else if (!m_navigation.error().isEmpty() && nonce != m_failedNavigationNonce) {
        m_failedNavigationNonce = nonce;
        qWarning().noquote() << m_navigation.error();
    }
}
