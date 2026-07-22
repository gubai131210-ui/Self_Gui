#ifndef IPLUGIN_H
#define IPLUGIN_H

#include "foundation/seltversion.h"
#include "vision/model/moduledescriptor.h"
#include "vision/runtime/inodeexecutor.h"

#include <QObject>
#include <QString>
#include <QVector>

namespace Selt {

struct PluginMetadata
{
    QString pluginId;
    QString displayName;
    QString pluginVersion{QStringLiteral("1.0.0")};
    int sdkVersion{VersionInfo::pluginSdkVersion};
    QString abiTag;
    QString compiler;
    QStringList nodeTypeIds;
    /// Declared capabilities: algorithm-node, camera-device, result-sink, ...
    QStringList capabilities;
};

class INodePlugin
{
public:
    virtual ~INodePlugin() = default;
    virtual PluginMetadata metadata() const = 0;
    virtual QVector<ModuleDescriptor> descriptors() const = 0;
    virtual void registerExecutors(class NodeExecutorRegistry &registry) = 0;
};

} // namespace Selt

#define SELT_NODE_PLUGIN_IID "selt.vision.nodeplugin/1.0"
Q_DECLARE_INTERFACE(Selt::INodePlugin, SELT_NODE_PLUGIN_IID)

#endif // IPLUGIN_H
