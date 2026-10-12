#pragma once

#include <QObject>
#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVector>

class StoreBackend : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList games READ games NOTIFY gamesChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY transactionChanged)
    Q_PROPERTY(QString activeGame READ activeGame NOTIFY transactionChanged)
    Q_PROPERTY(QString activity READ activity NOTIFY activityChanged)
    Q_PROPERTY(int progressPercent READ progressPercent NOTIFY progressChanged)
    Q_PROPERTY(QString progressStage READ progressStage NOTIFY progressChanged)
    Q_PROPERTY(bool availabilityChecked READ availabilityChecked NOTIFY gamesChanged)
    Q_PROPERTY(QString architecture READ architecture CONSTANT)

public:
    explicit StoreBackend(QObject *parent = nullptr);
    ~StoreBackend() override;
    QVariantList games() const;
    bool busy() const { return m_busy; }
    QString activeGame() const { return m_activeGame; }
    QString activity() const { return m_activity; }
    bool availabilityChecked() const { return m_availabilityChecked; }
    int progressPercent() const { return m_progressPercent; } // -1 indicates unknown / indeterminate
    QString progressStage() const { return m_progressStage; }
    QString architecture() const { return QStringLiteral("aarch64"); }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void installGame(const QString &id);
    Q_INVOKABLE void removeGame(const QString &id);
    Q_INVOKABLE void cancelTransaction();

signals:
    void gamesChanged();
    void transactionChanged();
    void activityChanged();
    void progressChanged();
    void transactionFinished(const QString &id, const QString &operation, bool success);

private:
    struct Game {
        QString id;
        QString name;
        QString genre;
        QString summary;
        QString description;
        QString tag;
        QString icon;
        QString screenshot;
        QString metadataSource = QStringLiteral("seed");
        bool installed = false;
        bool available = false;
    };

    void loadSeeds();
    void loadCache();
    void saveCache();
    void loadMetadata();
    void updateMetadata(Game &game, const QJsonObject &obj);
    Game *findGame(const QString &id);
    void startListing(const QStringList &arguments, bool installedList);
    void startTransaction(const QString &id, bool install);
    void setActivity(const QString &message);
    void endTransaction(bool success, const QString &details);
    void consumeProgressOutput(const QByteArray &data);
    void inspectProgressLine(const QString &line);

    QVector<Game> m_games;
    QHash<QString, QJsonObject> m_metadataCache;
    QNetworkAccessManager m_network;
    QProcess *m_transaction = nullptr;
    QString m_activeGame;
    QString m_operation;
    QString m_activity;
    bool m_busy = false;
    int m_progressPercent = -1;
    QString m_progressStage;
    QByteArray m_progressBuffer;
    bool m_availabilityChecked = false;
    int m_listingGeneration = 0;
};
