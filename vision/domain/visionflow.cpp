#include "vision/domain/visionflow.h"

#include "foundation/seltid.h"
#include "vision/registry/visionnoderegistry.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QSet>

VisionFlow::VisionFlow(const QString &flowId, const QString &name, QObject *parent)
    : QObject(parent)
    , m_flowId(flowId.isEmpty() ? Selt::IdGenerator::create(QStringLiteral("flow")) : flowId)
    , m_name(name)
{
}

void VisionFlow::setName(const QString &name)
{
    if (m_name == name)
        return;
    m_name = name;
    emit nameChanged(m_name);
}

bool VisionFlow::hasNode(const QString &id) const
{
    return m_nodeIndex.contains(id);
}

NodeModel VisionFlow::node(const QString &id) const
{
    const int idx = m_nodeIndex.value(id, -1);
    return idx >= 0 ? m_nodes.at(idx) : NodeModel{};
}

QString VisionFlow::addNode(NodeModel node)
{
    if (node.id.isEmpty())
        node.id = Selt::IdGenerator::create(QStringLiteral("node"));
    if (m_nodeIndex.contains(node.id))
        return {};
    m_nodes.append(node);
    m_nodeIndex.insert(node.id, m_nodes.size() - 1);
    emit nodeAdded(node);
    emitGraphChanged();
    return node.id;
}

bool VisionFlow::updateNode(const NodeModel &node)
{
    const int idx = m_nodeIndex.value(node.id, -1);
    if (idx < 0)
        return false;
    m_nodes[idx] = node;
    emit nodeUpdated(node);
    emitGraphChanged();
    return true;
}

bool VisionFlow::removeNode(const QString &id)
{
    const int idx = m_nodeIndex.value(id, -1);
    if (idx < 0)
        return false;

    QStringList toRemove;
    for (const ConnectionModel &c : m_connections) {
        if (c.sourceNodeId == id || c.targetNodeId == id)
            toRemove.append(c.id);
    }
    for (const QString &cid : toRemove)
        removeConnection(cid);

    m_nodes.removeAt(idx);
    rebuildIndexes();
    emit nodeRemoved(id);
    emitGraphChanged();
    return true;
}

bool VisionFlow::hasConnection(const QString &id) const
{
    return m_connectionIndex.contains(id);
}

ConnectionModel VisionFlow::connection(const QString &id) const
{
    const int idx = m_connectionIndex.value(id, -1);
    return idx >= 0 ? m_connections.at(idx) : ConnectionModel{};
}

QString VisionFlow::addConnection(ConnectionModel connection)
{
    if (!hasNode(connection.sourceNodeId) || !hasNode(connection.targetNodeId))
        return {};
    if (connection.sourceNodeId == connection.targetNodeId)
        return {};
    if (connection.id.isEmpty())
        connection.id = Selt::IdGenerator::create(QStringLiteral("conn"));
    if (m_connectionIndex.contains(connection.id))
        return {};
    m_connections.append(connection);
    m_connectionIndex.insert(connection.id, m_connections.size() - 1);
    emit connectionAdded(connection);
    emitGraphChanged();
    return connection.id;
}

bool VisionFlow::removeConnection(const QString &id)
{
    const int idx = m_connectionIndex.value(id, -1);
    if (idx < 0)
        return false;
    m_connections.removeAt(idx);
    rebuildIndexes();
    emit connectionRemoved(id);
    emitGraphChanged();
    return true;
}

void VisionFlow::clear()
{
    m_nodes.clear();
    m_connections.clear();
    m_nodeIndex.clear();
    m_connectionIndex.clear();
    emit graphReset();
    emitGraphChanged();
}

bool VisionFlow::replaceGraph(const QVector<NodeModel> &nodes,
                              const QVector<ConnectionModel> &connections,
                              QString *error)
{
    QSet<QString> nodeIds;
    for (const NodeModel &node : nodes) {
        if (node.id.isEmpty()) {
            if (error)
                *error = QStringLiteral("节点 ID 不能为空");
            return false;
        }
        if (nodeIds.contains(node.id)) {
            if (error)
                *error = QStringLiteral("存在重复节点 ID: %1").arg(node.id);
            return false;
        }
        nodeIds.insert(node.id);
    }

    QSet<QString> connIds;
    for (const ConnectionModel &conn : connections) {
        if (conn.id.isEmpty()) {
            if (error)
                *error = QStringLiteral("连线 ID 不能为空");
            return false;
        }
        if (connIds.contains(conn.id)) {
            if (error)
                *error = QStringLiteral("存在重复连线 ID: %1").arg(conn.id);
            return false;
        }
        if (!nodeIds.contains(conn.sourceNodeId) || !nodeIds.contains(conn.targetNodeId)) {
            if (error)
                *error = QStringLiteral("连线引用了不存在的节点: %1").arg(conn.id);
            return false;
        }
        connIds.insert(conn.id);
    }

    QVector<NodeModel> upgradedNodes = nodes;
    QVector<ConnectionModel> upgradedConnections = connections;
    VisionNodeRegistry::upgradeGraph(upgradedNodes, upgradedConnections);

    m_nodes = upgradedNodes;
    m_connections = upgradedConnections;
    rebuildIndexes();
    emit graphReset();
    emitGraphChanged();
    return true;
}

void VisionFlow::rebuildIndexes()
{
    m_nodeIndex.clear();
    m_connectionIndex.clear();
    for (int i = 0; i < m_nodes.size(); ++i)
        m_nodeIndex.insert(m_nodes.at(i).id, i);
    for (int i = 0; i < m_connections.size(); ++i)
        m_connectionIndex.insert(m_connections.at(i).id, i);
}

void VisionFlow::emitGraphChanged()
{
    emit graphChanged();
}

void VisionFlow::setFlowVariables(const Selt::ProjectVariableStore &vars)
{
    m_flowVariables = vars;
    emit variablesChanged();
}

Selt::ProjectVariableStore &VisionFlow::groupVariables(const QString &groupId)
{
    return m_groupVariables[groupId];
}

const Selt::ProjectVariableStore *VisionFlow::groupVariablesOrNull(const QString &groupId) const
{
    auto it = m_groupVariables.constFind(groupId);
    if (it == m_groupVariables.cend())
        return nullptr;
    return &(*it);
}

void VisionFlow::setGroupVariables(const QString &groupId, const Selt::ProjectVariableStore &vars)
{
    if (groupId.isEmpty())
        return;
    m_groupVariables.insert(groupId, vars);
    emit variablesChanged();
}

void VisionFlow::clearGroupVariables()
{
    m_groupVariables.clear();
    emit variablesChanged();
}

QJsonObject VisionFlow::variablesToJson() const
{
    QJsonObject groups;
    for (auto it = m_groupVariables.cbegin(); it != m_groupVariables.cend(); ++it)
        groups.insert(it.key(), it.value().toJson());
    return QJsonObject{{QStringLiteral("flowVariables"), m_flowVariables.toJson()},
                       {QStringLiteral("groupVariables"), groups}};
}

bool VisionFlow::variablesFromJson(const QJsonObject &obj, QString *error)
{
    Selt::ProjectVariableStore flowVars;
    if (obj.contains(QStringLiteral("flowVariables"))) {
        if (!flowVars.fromJson(obj.value(QStringLiteral("flowVariables")).toArray(), error))
            return false;
    }
    QHash<QString, Selt::ProjectVariableStore> groups;
    const QJsonObject groupsObj = obj.value(QStringLiteral("groupVariables")).toObject();
    for (auto it = groupsObj.begin(); it != groupsObj.end(); ++it) {
        Selt::ProjectVariableStore store;
        if (!store.fromJson(it.value().toArray(), error))
            return false;
        groups.insert(it.key(), store);
    }
    m_flowVariables = flowVars;
    m_groupVariables = groups;
    emit variablesChanged();
    return true;
}
