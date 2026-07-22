#ifndef FLOWEXECUTIONSNAPSHOT_H
#define FLOWEXECUTIONSNAPSHOT_H

#include "core/model/document.h"
#include "vision/runtime/executionscope.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>
#include <memory>

namespace Selt {

class FlowExecutionSnapshot
{
public:
    static FlowExecutionSnapshot create(const Document &document,
                                        const ExecutionScope &scope,
                                        const QStringList &resourceIds = {},
                                        const QJsonArray &pluginDependencies = {});

    const Document &document() const { return *m_document; }
    const ExecutionScope &scope() const { return m_scope; }
    QDateTime createdAt() const { return m_createdAt; }
    QStringList resourceIds() const { return m_resourceIds; }
    QJsonArray pluginDependencies() const { return m_pluginDependencies; }
    QStringList nodeIds() const;
    QString snapshotId() const { return m_snapshotId; }
    int nodeSchemaVersion() const { return m_nodeSchemaVersion; }

private:
    std::shared_ptr<Document> m_document;
    ExecutionScope m_scope;
    QDateTime m_createdAt;
    QStringList m_resourceIds;
    QJsonArray m_pluginDependencies;
    QString m_snapshotId;
    int m_nodeSchemaVersion{1};
};

} // namespace Selt

#endif // FLOWEXECUTIONSNAPSHOT_H
