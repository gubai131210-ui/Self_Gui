#include "plugin_host/pluginhost.h"

#include "vision/registry/visionnoderegistry.h"
#include "vision/runtime/nodeexecutorregistry.h"

#include <QCoreApplication>
#include <QDir>
#include <QPluginLoader>
#include <QSet>

namespace Selt {

PluginHost &PluginHost::instance()
{
    static PluginHost host;
    return host;
}

QVector<PluginLoadInfo> PluginHost::loadFromDirectory(const QString &dirPath)
{
    QVector<PluginLoadInfo> results;
    QDir dir(dirPath);
    if (!dir.exists()) {
        m_diagnostics.append(QStringLiteral("插件目录不存在: %1（可忽略，不影响旧工程）").arg(dirPath));
        return results;
    }

    QSet<QString> builtinTypes;
    for (const QString &t : VisionNodeRegistry::supportedTypes()) {
        if (!VisionNodeRegistry::isExternalType(t))
            builtinTypes.insert(t);
    }

    const QStringList files = dir.entryList(QDir::Files);
    for (const QString &fileName : files) {
        if (!(fileName.endsWith(QLatin1String(".dll"))
              || fileName.endsWith(QLatin1String(".so"))
              || fileName.endsWith(QLatin1String(".dylib")))) {
            continue;
        }

        PluginLoadInfo info;
        info.path = dir.absoluteFilePath(fileName);
        QPluginLoader loader(info.path);
        QObject *instance = loader.instance();
        if (!instance) {
            info.loaded = false;
            info.error = loader.errorString();
            m_diagnostics.append(QStringLiteral("加载失败 %1: %2（已跳过）").arg(info.path, info.error));
            results.append(info);
            continue;
        }

        INodePlugin *plugin = qobject_cast<INodePlugin *>(instance);
        if (!plugin) {
            info.loaded = false;
            info.error = QStringLiteral("未实现 INodePlugin 接口或 SDK 不兼容");
            m_diagnostics.append(info.error + QLatin1Char(' ') + info.path + QStringLiteral("（已跳过）"));
            loader.unload();
            results.append(info);
            continue;
        }

        const PluginMetadata meta = plugin->metadata();
        if (meta.sdkVersion > VersionInfo::pluginSdkVersion) {
            info.loaded = false;
            info.error = QStringLiteral("插件 SDK 版本过高: %1").arg(meta.sdkVersion);
            m_diagnostics.append(info.error + QStringLiteral("（已跳过）"));
            results.append(info);
            continue;
        }
        if (meta.sdkVersion < 1) {
            info.loaded = false;
            info.error = QStringLiteral("插件 SDK 版本无效");
            m_diagnostics.append(info.error);
            results.append(info);
            continue;
        }
        if (meta.pluginId.isEmpty() || meta.nodeTypeIds.isEmpty()) {
            info.loaded = false;
            info.error = QStringLiteral("插件元数据不完整（pluginId/nodeTypeIds）");
            m_diagnostics.append(info.error);
            results.append(info);
            continue;
        }

        // Conflict policy: built-in type IDs win; plugin types that collide are rejected.
        QStringList conflicts;
        for (const QString &typeId : meta.nodeTypeIds) {
            if (builtinTypes.contains(typeId) || m_registeredTypeIds.contains(typeId)
                || VisionNodeRegistry::supportedTypes().contains(typeId)) {
                conflicts.append(typeId);
            }
        }
        if (!conflicts.isEmpty()) {
            info.loaded = false;
            info.error = QStringLiteral("与内置/已加载节点冲突: %1").arg(conflicts.join(QStringLiteral(", ")));
            m_diagnostics.append(info.error + QStringLiteral("（已跳过）"));
            results.append(info);
            continue;
        }

        const QVector<ModuleDescriptor> descriptors = plugin->descriptors();
        if (descriptors.isEmpty()) {
            info.loaded = false;
            info.error = QStringLiteral("插件未提供 ModuleDescriptor");
            m_diagnostics.append(info.error);
            results.append(info);
            continue;
        }

        QString descError;
        if (!VisionNodeRegistry::registerExternalDescriptors(descriptors, &descError)) {
            info.loaded = false;
            info.error = descError;
            m_diagnostics.append(info.error + QStringLiteral("（已跳过）"));
            results.append(info);
            continue;
        }

        plugin->registerExecutors(NodeExecutorRegistry::instance());
        for (const QString &typeId : meta.nodeTypeIds) {
            m_registeredTypeIds.insert(typeId);
            VisionNodeRegistry::setPluginIdForType(typeId, meta.pluginId);
        }

        info.loaded = true;
        info.pluginId = meta.pluginId;
        info.displayName = meta.displayName;
        info.pluginVersion = meta.pluginVersion;
        info.sdkVersion = meta.sdkVersion;
        info.abiTag = meta.abiTag;
        info.compiler = meta.compiler;
        m_diagnostics.append(QStringLiteral("已加载插件 %1 (%2), 节点 %3")
                                 .arg(meta.displayName, info.path)
                                 .arg(meta.nodeTypeIds.join(QLatin1Char(','))));
        results.append(info);
        m_loaded.append(info);
    }
    return results;
}

QStringList PluginHost::diagnoseDependencies(const QJsonArray &dependencies) const
{
    QStringList diagnostics;
    for (const QJsonValue &value : dependencies) {
        const QJsonObject dependency = value.toObject();
        const QString pluginId = dependency.value(QStringLiteral("pluginId")).toString();
        const QString nodeTypeId = dependency.value(QStringLiteral("nodeTypeId")).toString();
        const int sdkVersion = dependency.value(QStringLiteral("sdkVersion")).toInt(0);
        const QString abiTag = dependency.value(QStringLiteral("abiTag")).toString();
        if (pluginId.isEmpty())
            continue;
        bool loaded = false;
        for (const PluginLoadInfo &plugin : m_loaded) {
            if (plugin.pluginId == pluginId && plugin.loaded) {
                if (sdkVersion > 0 && plugin.sdkVersion != sdkVersion) {
                    diagnostics.append(QStringLiteral("插件 %1 SDK 不匹配（工程=%2，当前=%3，节点 %4）")
                                           .arg(pluginId)
                                           .arg(sdkVersion)
                                           .arg(plugin.sdkVersion)
                                           .arg(nodeTypeId));
                    loaded = true;
                    break;
                }
                if (!abiTag.isEmpty() && !plugin.abiTag.isEmpty() && plugin.abiTag != abiTag) {
                    diagnostics.append(QStringLiteral("插件 %1 ABI 不兼容（工程=%2，当前=%3，节点 %4）")
                                           .arg(pluginId, abiTag, plugin.abiTag, nodeTypeId));
                    loaded = true;
                    break;
                }
                loaded = true;
                break;
            }
        }
        if (!loaded) {
            diagnostics.append(QStringLiteral("缺少插件 %1（节点 %2）")
                                   .arg(pluginId, nodeTypeId));
        }
    }
    return diagnostics;
}

QStringList PluginHost::deploymentRequirements() const
{
    QStringList lines;
    lines.append(QStringLiteral("运行环境要求: Qt6 Widgets/Svg/Test 运行库, OpenCV core/imgproc/imgcodecs/videoio"));
    lines.append(QStringLiteral("插件目录: bin/plugins/nodes"));
    if (m_loaded.isEmpty()) {
        lines.append(QStringLiteral("当前未加载任何外部插件"));
    } else {
        for (const PluginLoadInfo &plugin : m_loaded) {
            lines.append(QStringLiteral("插件 %1 v%2 | sdk=%3 | abi=%4 | compiler=%5")
                             .arg(plugin.pluginId,
                                  plugin.pluginVersion.isEmpty() ? QStringLiteral("-") : plugin.pluginVersion,
                                  QString::number(plugin.sdkVersion),
                                  plugin.abiTag.isEmpty() ? QStringLiteral("-") : plugin.abiTag,
                                  plugin.compiler.isEmpty() ? QStringLiteral("-") : plugin.compiler));
        }
    }
    return lines;
}

} // namespace Selt
