#ifndef VISIONNODEREGISTRY_H
#define VISIONNODEREGISTRY_H

#include "core/model/nodemodel.h"
#include "core/model/connectionmodel.h"
#include "vision/model/moduledescriptor.h"

#include <QString>
#include <QStringList>
#include <QVector>

class VisionNodeRegistry
{
public:
    static QStringList supportedTypes();
    static QStringList categories();
    static QStringList typesInCategory(const QString &category);
    static QString displayName(const QString &type);
    static QString category(const QString &type);
    static ModuleDescriptor descriptor(const QString &type);
    static QVector<ModuleDescriptor> allDescriptors();
    static NodeModel create(const QString &type, const QPointF &position = QPointF(0, 0));
    /// Normalize multi-port layout and repair legacy miswired template-match inputs.
    static bool upgradeGraph(QVector<NodeModel> &nodes, QVector<ConnectionModel> &connections);
    static QJsonObject defaultParameters(const QString &type);
    static bool validateParameters(const QString &type, const QJsonObject &params, QString *error = nullptr);
    static QSizeF fixedNodeSize();

    /// Dynamically register a plugin/external module descriptor.
    /// Built-in IDs and already-registered IDs are rejected.
    static bool registerExternalDescriptor(const ModuleDescriptor &desc, QString *error = nullptr);
    static bool registerExternalDescriptors(const QVector<ModuleDescriptor> &descriptors,
                                            QString *error = nullptr);
    static bool isExternalType(const QString &typeId);
    static QString pluginIdForType(const QString &typeId);
    static void setPluginIdForType(const QString &typeId, const QString &pluginId);
};

#endif // VISIONNODEREGISTRY_H
