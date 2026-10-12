#pragma once

#include <QObject>
#include <QFileSystemWatcher>
#include <QString>
#include <QVariantMap>

// Read-only theme bridge. A shared Zaman appearance.json is optional; we never
// create or modify Zaman configuration from inside the Dukkan prototype.
class AppearanceProvider final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap tokens READ tokens NOTIFY tokensChanged)
    Q_PROPERTY(QString source READ source NOTIFY tokensChanged)

public:
    explicit AppearanceProvider(QObject *parent = nullptr);
    QVariantMap tokens() const { return m_tokens; }
    QString source() const { return m_source; }

signals:
    void tokensChanged();

private:
    void reload();
    void rewatch();
    static QVariantMap defaults();

    QFileSystemWatcher m_watcher;
    QString m_filePath;
    QVariantMap m_tokens;
    QString m_source;
};
