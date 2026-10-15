#pragma once

#include <QJsonObject>
#include <QString>
#include <QVariantList>

namespace GameArtwork {
QString safeUrl(const QString &candidate);
QVariantList screenshots(const QJsonObject &metadata);
// Shared with sessiond's Pegasus sync. Title art is optional; never fabricate it.
QString localLogo(const QString &appId);
QString localIcon(const QString &appId);
}
