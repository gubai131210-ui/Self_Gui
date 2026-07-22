#ifndef VISIONFLOW_H
#define VISIONFLOW_H

#include "core/model/connectionmodel.h"
#include "core/model/nodemodel.h"
#include "vision/domain/projectvariables.h"

#include <QObject>
#include <QHash>
#include <QString>
#include <QVector>

enum class FlowRunPolicy {
    Sequential,
    Parallel // reserved for later phases
};

class VisionFlow : public QObject
{
    Q_OBJECT
public:
    explicit VisionFlow(const QString &flowId, const QString &name, QObject *parent = nullptr);

    QString flowId() const { return m_flowId; }
    QString name() const { return m_name; }
    void setName(const QString &name);

    bool isMainFlow() const { return m_isMainFlow; }
    void setMainFlow(bool main) { m_isMainFlow = main; }

    FlowRunPolicy runPolicy() const { return m_runPolicy; }
    void setRunPolicy(FlowRunPolicy policy) { m_runPolicy = policy; }

    const QVector<NodeModel> &nodes() const { return m_nodes; }
    const QVector<ConnectionModel> &connections() const { return m_connections; }

    bool hasNode(const QString &id) const;
    NodeModel node(const QString &id) const;
    QString addNode(NodeModel node);
    bool updateNode(const NodeModel &node);
    bool removeNode(const QString &id);

    bool hasConnection(const QString &id) const;
    ConnectionModel connection(const QString &id) const;
    QString addConnection(ConnectionModel connection);
    bool removeConnection(const QString &id);

    Selt::ProjectVariableStore &flowVariables() { return m_flowVariables; }
    const Selt::ProjectVariableStore &flowVariables() const { return m_flowVariables; }
    void setFlowVariables(const Selt::ProjectVariableStore &vars);

    Selt::ProjectVariableStore &groupVariables(const QString &groupId);
    const Selt::ProjectVariableStore *groupVariablesOrNull(const QString &groupId) const;
    QStringList groupVariableIds() const { return m_groupVariables.keys(); }
    void setGroupVariables(const QString &groupId, const Selt::ProjectVariableStore &vars);
    void clearGroupVariables();

    void clear();
    /// Replace graph after validating IDs and connection endpoints. Returns false on invalid input.
    bool replaceGraph(const QVector<NodeModel> &nodes, const QVector<ConnectionModel> &connections,
                      QString *error = nullptr);

    QJsonObject variablesToJson() const;
    bool variablesFromJson(const QJsonObject &obj, QString *error = nullptr);

signals:
    void nameChanged(const QString &name);
    void nodeAdded(const NodeModel &node);
    void nodeRemoved(const QString &id);
    void nodeUpdated(const NodeModel &node);
    void connectionAdded(const ConnectionModel &connection);
    void connectionRemoved(const QString &id);
    void graphReset();
    void graphChanged();
    void variablesChanged();

private:
    void rebuildIndexes();
    void emitGraphChanged();

    QString m_flowId;
    QString m_name;
    bool m_isMainFlow{false};
    FlowRunPolicy m_runPolicy{FlowRunPolicy::Sequential};
    QVector<NodeModel> m_nodes;
    QVector<ConnectionModel> m_connections;
    QHash<QString, int> m_nodeIndex;
    QHash<QString, int> m_connectionIndex;
    Selt::ProjectVariableStore m_flowVariables;
    QHash<QString, Selt::ProjectVariableStore> m_groupVariables;
};

#endif // VISIONFLOW_H
