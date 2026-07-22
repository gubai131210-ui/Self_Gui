#ifndef EXECUTIONSCOPE_H
#define EXECUTIONSCOPE_H

#include "core/model/document.h"

#include <QSet>
#include <QString>

namespace Selt {

enum class ExecutionScopeKind {
    FullFlow,
    SingleNode,
    NodeWithUpstream,
    ExplicitSubgraph
};

struct ExecutionScope
{
    ExecutionScopeKind kind{ExecutionScopeKind::FullFlow};
    QString targetNodeId;
    QSet<QString> nodeIds;

    static ExecutionScope fullFlow();
    static ExecutionScope singleNode(const QString &nodeId);
    static ExecutionScope withUpstream(const QString &nodeId);
    static ExecutionScope explicitSubgraph(const QSet<QString> &nodeIds);

    QString description() const;
    QSet<QString> resolveNodeIds(const Document &document, QString *error = nullptr) const;
};

} // namespace Selt

#endif // EXECUTIONSCOPE_H
