#ifndef PLUGINHOST_H
#define PLUGINHOST_H

#include "sdk/include/selt/iplugin.h"

#include <QSet>
#include <QJsonArray>
#include <QString>
#include <QStringList>
#include <QVector>

namespace Selt {

struct PluginLoadInfo
{
    QString path;
    QString pluginId;
    QString displayName;
    QString pluginVersion;
    int sdkVersion{0};
    QString abiTag;
    QString compiler;
    bool loaded{false};
    QString error;
};

class PluginHost
{
public:
    static PluginHost &instance();

    /// Scan directory for Qt plugins implementing INodePlugin.
    /// Failures are recorded as diagnostics and never block host startup.
    QVector<PluginLoadInfo> loadFromDirectory(const QString &dirPath);
    QVector<PluginLoadInfo> loadedPlugins() const { return m_loaded; }
    QStringList diagnostics() const { return m_diagnostics; }
    QSet<QString> registeredTypeIds() const { return m_registeredTypeIds; }
    QStringList diagnoseDependencies(const QJsonArray &dependencies) const;
    QStringList deploymentRequirements() const;

private:
    PluginHost() = default;
    QVector<PluginLoadInfo> m_loaded;
    QStringList m_diagnostics;
    QSet<QString> m_registeredTypeIds;
};

} // namespace Selt

#endif
