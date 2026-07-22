#include "documentcommands.h"

AddNodeCommand::AddNodeCommand(Document *document, const NodeModel &node, QUndoCommand *parent)
    : QUndoCommand(QStringLiteral("添加节点"), parent)
    , m_document(document)
    , m_node(node)
{
}

void AddNodeCommand::undo()
{
    m_document->removeNode(m_node.id);
}

void AddNodeCommand::redo()
{
    if (m_first) {
        m_first = false;
        if (!m_document->hasNode(m_node.id))
            m_document->addNode(m_node);
        return;
    }
    m_document->addNode(m_node);
}

DeleteNodesCommand::DeleteNodesCommand(Document *document, const QStringList &nodeIds, QUndoCommand *parent)
    : QUndoCommand(QStringLiteral("删除节点"), parent)
    , m_document(document)
{
    for (const QString &id : nodeIds) {
        if (!m_document->hasNode(id))
            continue;
        m_nodes.append(m_document->node(id));
        for (const ConnectionModel &c : m_document->connectionsForNode(id)) {
            bool exists = false;
            for (const ConnectionModel &existing : m_connections) {
                if (existing.id == c.id) {
                    exists = true;
                    break;
                }
            }
            if (!exists)
                m_connections.append(c);
        }
    }
}

void DeleteNodesCommand::undo()
{
    for (const NodeModel &node : m_nodes)
        m_document->addNode(node);
    for (const ConnectionModel &c : m_connections)
        m_document->addConnection(c);
}

void DeleteNodesCommand::redo()
{
    for (const NodeModel &node : m_nodes)
        m_document->removeNode(node.id);
}

MoveNodesCommand::MoveNodesCommand(Document *document, const QVector<Item> &items, QUndoCommand *parent)
    : QUndoCommand(QStringLiteral("移动节点"), parent)
    , m_document(document)
    , m_items(items)
{
}

void MoveNodesCommand::undo()
{
    for (const Item &item : m_items)
        m_document->setNodePosition(item.id, item.oldPos);
}

void MoveNodesCommand::redo()
{
    for (const Item &item : m_items)
        m_document->setNodePosition(item.id, item.newPos);
}

ResizeNodeCommand::ResizeNodeCommand(Document *document,
                                     const QString &nodeId,
                                     const QSizeF &oldSize,
                                     const QSizeF &newSize,
                                     const QPointF &oldPos,
                                     const QPointF &newPos,
                                     QUndoCommand *parent)
    : QUndoCommand(QStringLiteral("调整节点大小"), parent)
    , m_document(document)
    , m_nodeId(nodeId)
    , m_oldSize(oldSize)
    , m_newSize(newSize)
    , m_oldPos(oldPos)
    , m_newPos(newPos)
{
}

void ResizeNodeCommand::undo()
{
    m_document->setNodeSize(m_nodeId, m_oldSize);
    m_document->setNodePosition(m_nodeId, m_oldPos);
}

void ResizeNodeCommand::redo()
{
    m_document->setNodeSize(m_nodeId, m_newSize);
    m_document->setNodePosition(m_nodeId, m_newPos);
}

AddConnectionCommand::AddConnectionCommand(Document *document, const ConnectionModel &connection, QUndoCommand *parent)
    : QUndoCommand(QStringLiteral("添加连接"), parent)
    , m_document(document)
    , m_connection(connection)
{
}

void AddConnectionCommand::undo()
{
    m_document->removeConnection(m_connection.id);
}

void AddConnectionCommand::redo()
{
    if (m_first) {
        m_first = false;
        if (!m_document->hasConnection(m_connection.id))
            m_document->addConnection(m_connection);
        return;
    }
    m_document->addConnection(m_connection);
}

DeleteConnectionsCommand::DeleteConnectionsCommand(Document *document, const QStringList &connectionIds, QUndoCommand *parent)
    : QUndoCommand(QStringLiteral("删除连接"), parent)
    , m_document(document)
{
    for (const QString &id : connectionIds) {
        if (m_document->hasConnection(id))
            m_connections.append(m_document->connection(id));
    }
}

void DeleteConnectionsCommand::undo()
{
    for (const ConnectionModel &c : m_connections)
        m_document->addConnection(c);
}

void DeleteConnectionsCommand::redo()
{
    for (const ConnectionModel &c : m_connections)
        m_document->removeConnection(c.id);
}

ChangeNodePropertyCommand::ChangeNodePropertyCommand(Document *document,
                                                     const NodeModel &oldNode,
                                                     const NodeModel &newNode,
                                                     QUndoCommand *parent)
    : QUndoCommand(QStringLiteral("修改节点属性"), parent)
    , m_document(document)
    , m_oldNode(oldNode)
    , m_newNode(newNode)
{
}

void ChangeNodePropertyCommand::undo()
{
    m_document->updateNode(m_oldNode);
}

void ChangeNodePropertyCommand::redo()
{
    m_document->updateNode(m_newNode);
}

ChangeNodePortExposureCommand::ChangeNodePortExposureCommand(Document *document,
                                                             const NodeModel &oldNode,
                                                             const NodeModel &newNode,
                                                             QUndoCommand *parent)
    : QUndoCommand(QStringLiteral("修改端口暴露"), parent)
    , m_document(document)
    , m_oldNode(oldNode)
    , m_newNode(newNode)
{
}

void ChangeNodePortExposureCommand::undo()
{
    m_document->updateNode(m_oldNode);
}

void ChangeNodePortExposureCommand::redo()
{
    m_document->updateNode(m_newNode);
}

PasteNodesCommand::PasteNodesCommand(Document *document,
                                     const QVector<NodeModel> &nodes,
                                     const QVector<ConnectionModel> &connections,
                                     QUndoCommand *parent)
    : QUndoCommand(QStringLiteral("粘贴"), parent)
    , m_document(document)
    , m_nodes(nodes)
    , m_connections(connections)
{
}

void PasteNodesCommand::undo()
{
    for (const NodeModel &node : m_nodes)
        m_document->removeNode(node.id);
}

void PasteNodesCommand::redo()
{
    if (m_first) {
        m_first = false;
        for (const NodeModel &node : m_nodes) {
            if (!m_document->hasNode(node.id))
                m_document->addNode(node);
        }
        for (const ConnectionModel &c : m_connections) {
            if (!m_document->hasConnection(c.id))
                m_document->addConnection(c);
        }
        return;
    }
    for (const NodeModel &node : m_nodes)
        m_document->addNode(node);
    for (const ConnectionModel &c : m_connections)
        m_document->addConnection(c);
}
