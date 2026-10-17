#pragma once

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStringList>

// Navigation is a one-shot client request, not the default page or job state.
// Remember recently handled nonces across UI restarts and out-of-order replies.
class TransactionNavigation
{
public:
    static QString statePath()
    {
        QString root = qEnvironmentVariable("XDG_STATE_HOME");
        if (!QDir::isAbsolutePath(root)) root = QDir::homePath() + QStringLiteral("/.local/state");
        return root + QStringLiteral("/dukkan/client-navigation.json");
    }

    explicit TransactionNavigation(const QString &path = statePath()) : m_path(path)
    {
        QFile file(path);
        if (!file.exists()) return;
        if (!file.open(QIODevice::ReadOnly) || file.size() > 16384) {
            m_valid = false; m_error = QStringLiteral("Cannot read Dukkan navigation state"); return;
        }
        QJsonParseError parse;
        const auto document = QJsonDocument::fromJson(file.readAll(), &parse);
        const auto object = document.object();
        if (parse.error != QJsonParseError::NoError || object.value("schema").toInt() != 1
            || !object.value("nonces").isArray() || object.value("nonces").toArray().size() > 128) {
            m_valid = false; m_error = QStringLiteral("Invalid Dukkan navigation state"); return;
        }
        for (const auto &value : object.value("nonces").toArray()) {
            const auto nonce = value.toString();
            if (!token(nonce)) { m_valid = false; m_error = QStringLiteral("Invalid Dukkan navigation nonce"); return; }
            if (!m_seen.contains(nonce)) m_seen.append(nonce);
        }
    }

    bool take(const QString &nonce, const QString &jobId)
    {
        if (!m_valid || !token(nonce) || !token(jobId) || m_seen.contains(nonce)) return false;
        auto next = m_seen;
        next.append(nonce);
        while (next.size() > 128) next.removeFirst();
        QJsonArray array;
        for (const auto &value : next) array.append(value);
        const auto bytes = QJsonDocument(QJsonObject{{"schema", 1}, {"nonces", array}}).toJson(QJsonDocument::Compact);
        if (!QDir().mkpath(QFileInfo(m_path).absolutePath())) {
            m_error = QStringLiteral("Cannot create Dukkan navigation directory"); return false;
        }
        QSaveFile file(m_path);
        if (!file.open(QIODevice::WriteOnly) || !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)
            || file.write(bytes) != bytes.size() || !file.commit()) {
            m_error = QStringLiteral("Cannot save Dukkan navigation state"); return false;
        }
        m_seen = next;
        m_error.clear();
        return true;
    }

    QString error() const { return m_error; }

private:
    static bool token(const QString &value)
    {
        static const QRegularExpression format(QStringLiteral("^[a-f0-9]{32}$"));
        return format.match(value).hasMatch();
    }
    QString m_path;
    QStringList m_seen;
    QString m_error;
    bool m_valid = true;
};
