#pragma once
#include <QByteArray>
#include <QString>
#include <QVector>
namespace GameCatalog {
struct Entry {
    QString id, name, genre, summary, tag, packageLogo;
    bool enabled = true;
};
QString path();
bool parse(const QByteArray &bytes, QVector<Entry> &entries, QString &error);
bool load(QVector<Entry> &entries, QString &error);
}
