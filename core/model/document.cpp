#include "document.h"

Document::Document(QObject *parent)
    : QObject(parent)
{
}

void Document::setTitle(const QString &title)
{
    if (m_title == title)
        return;
    m_title = title;
    emit titleChanged(m_title);
    setModifiedInternal(true);
}

void Document::setSettings(const DocumentSettings &settings)
{
    m_settings = settings;
    emit settingsChanged(m_settings);
    setModifiedInternal(true);
}

bool Document::hasNode(const QString &id) const
{
    return m_nodeIndex.contains(id);
}

bool Document::hasConnection(const QString &id) const
{
    return m_connectionIndex.contains(id);
}

NodeModel Document::node(const QString &id) const
{
    const int idx = m_nodeIndex.value(id, -1);
    if (idx < 0)
        return {};
    return m_nodes.at(idx);
}

ConnectionModel Document::connection(const QString &id) const
{
    const int idx = m_connectionIndex.value(id, -1);
    if (idx < 0)
        return {};
    return m_connections.at(idx);
}

int Document::nodeIndex(const QString &id) const
{
    return m_nodeIndex.value(id, -1);
}

int Document::connectionIndex(const QString &id) const
{
    return m_connectionIndex.value(id, -1);
}

QString Document::addNode(NodeModel node)
{
    if (node.id.isEmpty())
        node.id = createId(QStringLiteral("node"));
    if (node.ports.isEmpty())
        node.ports = NodeModel::defaultPortsForType(node.type);

    if (m_nodeIndex.contains(node.id))
        return {};

    m_nodes.append(node);
    m_nodeIndex.insert(node.id, m_nodes.size() - 1);
    emit nodeAdded(node);
    setModifiedInternal(true);
    return node.id;
}

bool Document::removeNode(const QString &id)
{
    const int idx = m_nodeIndex.value(id, -1);
    if (idx < 0)
        return false;

    removeConnectionsForNode(id);
    m_nodes.removeAt(idx);
    rebuildIndexes();
    emit nodeRemoved(id);
    setModifiedInternal(true);
    return true;
}

bool Document::updateNode(const NodeModel &node)
{
    const int idx = m_nodeIndex.value(node.id, -1);
    if (idx < 0)
        return false;
    m_nodes[idx] = node;
    emit nodeUpdated(node);
    setModifiedInternal(true);
    return true;
}

bool Document::setNodePosition(const QString &id, const QPointF &pos)
{
    const int idx = m_nodeIndex.value(id, -1);
    if (idx < 0)
        return false;
    if (m_nodes[idx].position == pos)
        return true;
    m_nodes[idx].position = pos;
    emit nodeUpdated(m_nodes[idx]);
    setModifiedInternal(true);
    return true;
}

bool Document::setNodeSize(const QString &id, const QSizeF &size)
{
    const int idx = m_nodeIndex.value(id, -1);
    if (idx < 0)
        return false;
    if (m_nodes[idx].size == size)
        return true;
    m_nodes[idx].size = size;
    emit nodeUpdated(m_nodes[idx]);
    setModifiedInternal(true);
    return true;
}

bool Document::setNodeZValue(const QString &id, int z)
{
    const int idx = m_nodeIndex.value(id, -1);
    if (idx < 0)
        return false;
    if (m_nodes[idx].zValue == z)
        return true;
    m_nodes[idx].zValue = z;
    emit nodeUpdated(m_nodes[idx]);
    setModifiedInternal(true);
    return true;
}

QString Document::addConnection(ConnectionModel connection)
{
    if (connection.id.isEmpty())
        connection.id = createId(QStringLiteral("conn"));

    if (!hasNode(connection.sourceNodeId) || !hasNode(connection.targetNodeId))
        return {};
    if (connection.sourceNodeId == connection.targetNodeId
        && connection.sourcePortId == connection.targetPortId)
        return {};
    if (m_connectionIndex.contains(connection.id))
        return {};

    m_connections.append(connection);
    m_connectionIndex.insert(connection.id, m_connections.size() - 1);
    emit connectionAdded(connection);
    setModifiedInternal(true);
    return connection.id;
}

bool Document::removeConnection(const QString &id)
{
    const int idx = m_connectionIndex.value(id, -1);
    if (idx < 0)
        return false;
    m_connections.removeAt(idx);
    rebuildIndexes();
    emit connectionRemoved(id);
    setModifiedInternal(true);
    return true;
}

bool Document::updateConnection(const ConnectionModel &connection)
{
    const int idx = m_connectionIndex.value(connection.id, -1);
    if (idx < 0)
        return false;
    m_connections[idx] = connection;
    emit connectionUpdated(connection);
    setModifiedInternal(true);
    return true;
}

QVector<ConnectionModel> Document::connectionsForNode(const QString &nodeId) const
{
    QVector<ConnectionModel> result;
    for (const ConnectionModel &c : m_connections) {
        if (c.sourceNodeId == nodeId || c.targetNodeId == nodeId)
            result.append(c);
    }
    return result;
}

QStringList Document::removeConnectionsForNode(const QString &nodeId)
{
    QStringList removed;
    for (int i = m_connections.size() - 1; i >= 0; --i) {
        const ConnectionModel &c = m_connections.at(i);
        if (c.sourceNodeId == nodeId || c.targetNodeId == nodeId) {
            removed.prepend(c.id);
            const QString id = c.id;
            m_connections.removeAt(i);
            emit connectionRemoved(id);
        }
    }
    if (!removed.isEmpty()) {
        rebuildIndexes();
        setModifiedInternal(true);
    }
    return removed;
}

void Document::clear()
{
    m_title = QStringLiteral("未命名文档");
    m_settings = DocumentSettings{};
    m_nodes.clear();
    m_connections.clear();
    m_nodeIndex.clear();
    m_connectionIndex.clear();
    emit documentReset();
    setModifiedInternal(false);
}

void Document::replaceAll(const QString &title,
                          const DocumentSettings &settings,
                          const QVector<NodeModel> &nodes,
                          const QVector<ConnectionModel> &connections)
{
    m_title = title;
    m_settings = settings;
    m_nodes = nodes;
    m_connections = connections;
    rebuildIndexes();
    emit documentReset();
    setModifiedInternal(false);
}

void Document::setModified(bool modified)
{
    setModifiedInternal(modified);
}

void Document::markClean()
{
    setModifiedInternal(false);
}

QString Document::createId(const QString &prefix)
{
    return prefix + QLatin1Char('_')
        + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
}

void Document::rebuildIndexes()
{
    m_nodeIndex.clear();
    for (int i = 0; i < m_nodes.size(); ++i)
        m_nodeIndex.insert(m_nodes.at(i).id, i);

    m_connectionIndex.clear();
    for (int i = 0; i < m_connections.size(); ++i)
        m_connectionIndex.insert(m_connections.at(i).id, i);
}

void Document::setModifiedInternal(bool modified)
{
    if (m_modified == modified)
        return;
    m_modified = modified;
    emit modifiedChanged(m_modified);
}
