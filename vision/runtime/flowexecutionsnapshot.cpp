#include "vision/runtime/flowexecutionsnapshot.h"

#include "foundation/seltid.h"
#include "vision/registry/visionnoderegistry.h"

#include <QSet>

namespace Selt {

FlowExecutionSnapshot FlowExecutionSnapshot::create(const Document &document,
                                                    const ExecutionScope &scope,
                                                    const QStringList &resourceIds,
                                                    const QJsonArray &pluginDependencies)
{
    FlowExecutionSnapshot snapshot;
    snapshot.m_scope = scope;
    snapshot.m_createdAt = QDateTime::currentDateTime();
    snapshot.m_snapshotId = IdGenerator::create(QStringLiteral("snapshot"));
    snapshot.m_document = std::make_shared<Document>();

    QString scopeError;
    const QSet<QString> selected = scope.resolveNodeIds(document, &scopeError);
    QVector<NodeModel> nodes;
    QVector<ConnectionModel> connections;
    for (const NodeModel &node : document.nodes()) {
        if (selected.contains(node.id)) {
            nodes.append(node);
            const QString resourceId = node.parameters.value(QStringLiteral("templateResourceId")).toString();
            if (!resourceId.isEmpty() && !snapshot.m_resourceIds.contains(resourceId))
                snapshot.m_resourceIds.append(resourceId);
            const QString pluginId = VisionNodeRegistry::pluginIdForType(node.type);
            if (!pluginId.isEmpty()) {
                snapshot.m_pluginDependencies.append(QJsonObject{
                    {QStringLiteral("pluginId"), pluginId},
                    {QStringLiteral("nodeTypeId"), node.type}});
            }
        }
    }
    for (const ConnectionModel &connection : document.connections()) {
        if (selected.contains(connection.sourceNodeId)
            && selected.contains(connection.targetNodeId)) {
            connections.append(connection);
        }
    }
    for (const QString &resourceId : resourceIds) {
        if (!snapshot.m_resourceIds.contains(resourceId))
            snapshot.m_resourceIds.append(resourceId);
    }
    for (const QJsonValue &value : pluginDependencies)
        snapshot.m_pluginDependencies.append(value);
    snapshot.m_document->replaceAll(document.title(), document.settings(), nodes, connections);
    return snapshot;
}

QStringList FlowExecutionSnapshot::nodeIds() const
{
    QStringList ids;
    for (const NodeModel &node : m_document->nodes())
        ids.append(node.id);
    return ids;
}

} // namespace Selt
