#include "vision/runtime/executionscope.h"

#include <QQueue>

namespace Selt {

ExecutionScope ExecutionScope::fullFlow()
{
    return {};
}

ExecutionScope ExecutionScope::singleNode(const QString &nodeId)
{
    ExecutionScope scope;
    scope.kind = ExecutionScopeKind::SingleNode;
    scope.targetNodeId = nodeId;
    return scope;
}

ExecutionScope ExecutionScope::withUpstream(const QString &nodeId)
{
    ExecutionScope scope;
    scope.kind = ExecutionScopeKind::NodeWithUpstream;
    scope.targetNodeId = nodeId;
    return scope;
}

ExecutionScope ExecutionScope::explicitSubgraph(const QSet<QString> &nodeIds)
{
    ExecutionScope scope;
    scope.kind = ExecutionScopeKind::ExplicitSubgraph;
    scope.nodeIds = nodeIds;
    return scope;
}

QString ExecutionScope::description() const
{
    switch (kind) {
    case ExecutionScopeKind::FullFlow:
        return QStringLiteral("完整流程");
    case ExecutionScopeKind::SingleNode:
        return QStringLiteral("单节点: %1").arg(targetNodeId);
    case ExecutionScopeKind::NodeWithUpstream:
        return QStringLiteral("节点及上游: %1").arg(targetNodeId);
    case ExecutionScopeKind::ExplicitSubgraph:
        return QStringLiteral("指定子图: %1 个节点").arg(nodeIds.size());
    }
    return QStringLiteral("未知范围");
}

QSet<QString> ExecutionScope::resolveNodeIds(const Document &document, QString *error) const
{
    QSet<QString> all;
    for (const NodeModel &node : document.nodes())
        all.insert(node.id);
    if (kind == ExecutionScopeKind::FullFlow)
        return all;
    if (kind == ExecutionScopeKind::ExplicitSubgraph)
        return nodeIds & all;
    if (targetNodeId.isEmpty() || !all.contains(targetNodeId)) {
        if (error)
            *error = QStringLiteral("执行目标节点不存在: %1").arg(targetNodeId);
        return {};
    }
    if (kind == ExecutionScopeKind::SingleNode)
        return {targetNodeId};

    QHash<QString, QStringList> upstream;
    for (const ConnectionModel &connection : document.connections())
        upstream[connection.targetNodeId].append(connection.sourceNodeId);
    QSet<QString> result;
    QQueue<QString> queue;
    queue.enqueue(targetNodeId);
    while (!queue.isEmpty()) {
        const QString nodeId = queue.dequeue();
        if (result.contains(nodeId))
            continue;
        result.insert(nodeId);
        for (const QString &sourceId : upstream.value(nodeId))
            queue.enqueue(sourceId);
    }
    return result;
}

} // namespace Selt
