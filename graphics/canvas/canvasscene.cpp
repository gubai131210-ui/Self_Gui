#include "canvasscene.h"

#include "app/nodebuilder.h"
#include "core/command/documentcommands.h"
#include "core/model/document.h"
#if SELT_HAS_OPENCV
#include "vision/data/datatype.h"
#include "vision/registry/visionnoderegistry.h"
#include "vision/validation/graphvalidator.h"
#endif
#include "graphics/items/connectiongraphicsitem.h"
#include "graphics/items/nodegraphicsitem.h"
#include "graphics/items/portgraphicsitem.h"
#include "ui/theme/uistyle.h"

#include <QGraphicsPathItem>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QLineF>
#include <QMenu>
#include <QPainter>
#include <QSet>
#include <QUndoCommand>
#include <QtMath>
#include <algorithm>

CanvasScene::CanvasScene(Document *document, QUndoStack *undoStack, QObject *parent)
    : QGraphicsScene(parent)
    , m_document(document)
    , m_undoStack(undoStack)
{
    setSceneRect(-5000, -5000, 10000, 10000);
    connectDocument();
    connect(this, &QGraphicsScene::selectionChanged, this, &CanvasScene::onSelectionChanged);
    rebuildFromDocument();
}

void CanvasScene::setUndoStack(QUndoStack *undoStack)
{
    m_undoStack = undoStack;
}

NodeGraphicsItem *CanvasScene::nodeItem(const QString &id) const
{
    return m_nodes.value(id, nullptr);
}

ConnectionGraphicsItem *CanvasScene::connectionItem(const QString &id) const
{
    return m_connections.value(id, nullptr);
}

void CanvasScene::rebuildFromDocument()
{
    clear();
    m_nodes.clear();
    m_connections.clear();
    m_sourcePort = nullptr;
    m_tempConnection = nullptr;

    if (!m_document)
        return;

    for (const NodeModel &node : m_document->nodes())
        onNodeAdded(node);
    for (const ConnectionModel &c : m_document->connections())
        onConnectionAdded(c);
}

void CanvasScene::createNodeAt(const QString &type, const QPointF &scenePos)
{
    if (!m_document || !m_undoStack)
        return;
    NodeModel node = NodeBuilder::create(type, snapPoint(scenePos));
    m_undoStack->push(new AddNodeCommand(m_document, node));
}

void CanvasScene::deleteSelection()
{
    if (!m_document || !m_undoStack)
        return;

    const QStringList nodeIds = selectedNodeIds();
    const QStringList connectionIds = selectedConnectionIds();

    if (!nodeIds.isEmpty()) {
        m_undoStack->push(new DeleteNodesCommand(m_document, nodeIds));
        return;
    }
    if (!connectionIds.isEmpty())
        m_undoStack->push(new DeleteConnectionsCommand(m_document, connectionIds));
}

void CanvasScene::selectAll()
{
    for (auto *item : m_nodes)
        item->setSelected(true);
    for (auto *item : m_connections)
        item->setSelected(true);
}

QStringList CanvasScene::selectedNodeIds() const
{
    QStringList ids;
    for (QGraphicsItem *item : selectedItems()) {
        if (auto *node = qgraphicsitem_cast<NodeGraphicsItem *>(item))
            ids.append(node->nodeId());
    }
    return ids;
}

QStringList CanvasScene::selectedConnectionIds() const
{
    QStringList ids;
    for (QGraphicsItem *item : selectedItems()) {
        if (auto *conn = qgraphicsitem_cast<ConnectionGraphicsItem *>(item))
            ids.append(conn->connectionId());
    }
    return ids;
}

void CanvasScene::beginConnection(PortGraphicsItem *port)
{
    cancelConnection();
    m_sourcePort = port;
    m_tempConnection = addPath(QPainterPath(), QPen(QColor(100, 140, 220), 2, Qt::DashLine));
    m_tempConnection->setZValue(100);
    refreshConnectionHighlights(port ? port->scenePos() : QPointF());
#if SELT_HAS_OPENCV
    if (port) {
        QString typeHint = port->port().dataType;
        bool ok = false;
        const Selt::DataTypeId tid = Selt::dataTypeIdFromString(typeHint, &ok);
        if (ok)
            typeHint = Selt::dataTypeIdDisplayName(tid);
        if (!typeHint.isEmpty()) {
            emit statusMessage(
                QStringLiteral("连线中 · 端口类型 %1 — 工具箱已按该类型筛选")
                    .arg(typeHint));
        }
        emit connectionDataTypeFilterRequested(port->port().dataType);
    }
#endif
}

void CanvasScene::updateTempConnection(const QPointF &scenePos)
{
    if (!m_sourcePort || !m_tempConnection)
        return;
    QPointF end = scenePos;
    if (PortGraphicsItem *nearPort = findCompatiblePortNear(scenePos, 28.0))
        end = nearPort->scenePos();
    QPainterPath path;
    path.moveTo(m_sourcePort->scenePos());
    path.lineTo(end);
    m_tempConnection->setPath(path);

    bool validNear = false;
#if SELT_HAS_OPENCV
    if (PortGraphicsItem *nearPort = findCompatiblePortNear(scenePos, 28.0)) {
        QString err;
        NodeGraphicsItem *srcNode = m_sourcePort->ownerNode();
        NodeGraphicsItem *dstNode = nearPort->ownerNode();
        if (srcNode && dstNode) {
            validNear = Selt::GraphValidator::canConnect(
                *m_document, srcNode->nodeId(), m_sourcePort->portId(),
                dstNode->nodeId(), nearPort->portId(), &err, true);
            if (validNear) {
                emit statusMessage(QStringLiteral("可连接：%1.%2 → %3.%4")
                                       .arg(srcNode->model().text, m_sourcePort->portId(),
                                            dstNode->model().text, nearPort->portId()));
            } else if (!err.isEmpty()) {
                emit statusMessage(err);
            }
        }
    }
#endif
    m_tempConnection->setPen(QPen(validNear ? QColor(46, 204, 113) : QColor(100, 140, 220),
                                  2, Qt::DashLine));
    refreshConnectionHighlights(scenePos);
}

void CanvasScene::finishConnection(PortGraphicsItem *targetPort)
{
    if (!m_sourcePort || !targetPort || !m_document || !m_undoStack) {
        cancelConnection();
        return;
    }

    NodeGraphicsItem *sourceNode = m_sourcePort->ownerNode();
    NodeGraphicsItem *targetNode = targetPort->ownerNode();
    if (!sourceNode || !targetNode || sourceNode == targetNode) {
        cancelConnection();
        return;
    }

    const PortModel sourcePort = m_sourcePort->port();
    const PortModel targetPortModel = targetPort->port();
    const bool sourceIsOutput = sourcePort.direction == PortDirection::Output
        || sourcePort.direction == PortDirection::Both;
    const bool targetIsInput = targetPortModel.direction == PortDirection::Input
        || targetPortModel.direction == PortDirection::Both;
    if (!sourceIsOutput || !targetIsInput) {
        emit statusMessage(QStringLiteral("连线方向无效：需要 输出端口 → 输入端口"));
        cancelConnection();
        return;
    }

    ConnectionModel connection;
    connection.id = Document::createId(QStringLiteral("conn"));
    connection.sourceNodeId = sourceNode->nodeId();
    connection.sourcePortId = m_sourcePort->portId();
    connection.targetNodeId = targetNode->nodeId();
    connection.targetPortId = targetPort->portId();
    connection.lineStyle = ConnectionLineStyle::Orthogonal;
    connection.showArrow = true;

    const QString occupiedId = occupiedConnectionId(connection.targetNodeId, connection.targetPortId);
    cancelConnection();

#if SELT_HAS_OPENCV
    QString connectError;
    if (!Selt::GraphValidator::canConnect(*m_document,
                                          connection.sourceNodeId, connection.sourcePortId,
                                          connection.targetNodeId, connection.targetPortId,
                                          &connectError, true)) {
        emit statusMessage(connectError);
        return;
    }
#endif

    if (!occupiedId.isEmpty()) {
        auto *macro = new QUndoCommand(QStringLiteral("替换连线"));
        new DeleteConnectionsCommand(m_document, {occupiedId}, macro);
        new AddConnectionCommand(m_document, connection, macro);
        m_undoStack->push(macro);
        emit statusMessage(QStringLiteral("已替换原有输入连线"));
    } else {
        m_undoStack->push(new AddConnectionCommand(m_document, connection));
    }
}

void CanvasScene::cancelConnection()
{
    clearConnectionHighlights();
    if (m_tempConnection) {
        removeItem(m_tempConnection);
        delete m_tempConnection;
        m_tempConnection = nullptr;
    }
    m_sourcePort = nullptr;
    emit connectionDataTypeFilterRequested(QString());
}

void CanvasScene::notifyNodeMoved(const QString &nodeId, const QPointF &oldPos, const QPointF &newPos)
{
    if (!m_document || !m_undoStack || oldPos == newPos)
        return;

    // Collect multi-selection moves into one command when possible
    QVector<MoveNodesCommand::Item> items;
    const QStringList selected = selectedNodeIds();
    if (selected.contains(nodeId) && selected.size() > 1) {
        for (const QString &id : selected) {
            if (!m_document->hasNode(id))
                continue;
            auto *item = nodeItem(id);
            if (!item)
                continue;
            MoveNodesCommand::Item move;
            move.id = id;
            const NodeModel model = m_document->node(id);
            // Approximate old pos from delta of the primary node
            const QPointF delta = newPos - oldPos;
            move.newPos = item->pos();
            move.oldPos = move.newPos - delta;
            items.append(move);
        }
    } else {
        MoveNodesCommand::Item move;
        move.id = nodeId;
        move.oldPos = oldPos;
        move.newPos = newPos;
        items.append(move);
    }

    // Apply model update through command; suppress duplicate by temporarily blocking?
    // Command redo will set positions again - that's fine.
    m_undoStack->push(new MoveNodesCommand(m_document, items));
}

void CanvasScene::updateConnectionsForNode(const QString &nodeId)
{
    if (!m_document)
        return;
    for (const ConnectionModel &c : m_document->connectionsForNode(nodeId)) {
        if (auto *item = connectionItem(c.id))
            item->updatePath();
    }
}

void CanvasScene::copySelection()
{
    m_clipboardNodes.clear();
    m_clipboardConnections.clear();
    if (!m_document)
        return;

    const QStringList nodeIds = selectedNodeIds();
    QSet<QString> idSet(nodeIds.begin(), nodeIds.end());
    for (const QString &id : nodeIds)
        m_clipboardNodes.append(m_document->node(id));

    for (const ConnectionModel &c : m_document->connections()) {
        if (idSet.contains(c.sourceNodeId) && idSet.contains(c.targetNodeId))
            m_clipboardConnections.append(c);
    }
    emit statusMessage(QStringLiteral("已复制 %1 个节点").arg(m_clipboardNodes.size()));
}

void CanvasScene::pasteClipboard(const QPointF &anchor)
{
    if (!m_document || !m_undoStack || m_clipboardNodes.isEmpty())
        return;

    QPointF origin = m_clipboardNodes.first().position;
    QHash<QString, QString> idMap;
    QVector<NodeModel> newNodes;
    for (NodeModel node : m_clipboardNodes) {
        const QString oldId = node.id;
        node.id = Document::createId(QStringLiteral("node"));
        idMap.insert(oldId, node.id);
        node.position = anchor + (node.position - origin) + QPointF(20, 20);
        newNodes.append(node);
    }

    QVector<ConnectionModel> newConnections;
    for (ConnectionModel c : m_clipboardConnections) {
        c.id = Document::createId(QStringLiteral("conn"));
        c.sourceNodeId = idMap.value(c.sourceNodeId);
        c.targetNodeId = idMap.value(c.targetNodeId);
        if (!c.sourceNodeId.isEmpty() && !c.targetNodeId.isEmpty())
            newConnections.append(c);
    }

    m_undoStack->push(new PasteNodesCommand(m_document, newNodes, newConnections));
}

void CanvasScene::alignSelection(Qt::Alignment alignment)
{
    const QStringList ids = selectedNodeIds();
    if (ids.size() < 2 || !m_document || !m_undoStack)
        return;

    qreal ref = 0;
    bool first = true;
    for (const QString &id : ids) {
        const NodeModel n = m_document->node(id);
        qreal value = 0;
        if (alignment & Qt::AlignLeft)
            value = n.position.x();
        else if (alignment & Qt::AlignRight)
            value = n.position.x() + n.size.width();
        else if (alignment & Qt::AlignHCenter)
            value = n.position.x() + n.size.width() / 2.0;
        else if (alignment & Qt::AlignTop)
            value = n.position.y();
        else if (alignment & Qt::AlignBottom)
            value = n.position.y() + n.size.height();
        else if (alignment & Qt::AlignVCenter)
            value = n.position.y() + n.size.height() / 2.0;

        if (first) {
            ref = value;
            first = false;
        } else if (alignment & (Qt::AlignLeft | Qt::AlignTop)) {
            ref = qMin(ref, value);
        } else if (alignment & (Qt::AlignRight | Qt::AlignBottom)) {
            ref = qMax(ref, value);
        } else {
            ref = (ref + value) / 2.0; // rough average for centers
        }
    }

    // Recalculate ref properly for center alignments
    if (alignment & (Qt::AlignHCenter | Qt::AlignVCenter)) {
        qreal sum = 0;
        for (const QString &id : ids) {
            const NodeModel n = m_document->node(id);
            if (alignment & Qt::AlignHCenter)
                sum += n.position.x() + n.size.width() / 2.0;
            else
                sum += n.position.y() + n.size.height() / 2.0;
        }
        ref = sum / ids.size();
    }

    QVector<MoveNodesCommand::Item> items;
    for (const QString &id : ids) {
        NodeModel n = m_document->node(id);
        MoveNodesCommand::Item item;
        item.id = id;
        item.oldPos = n.position;
        item.newPos = n.position;
        if (alignment & Qt::AlignLeft)
            item.newPos.setX(ref);
        else if (alignment & Qt::AlignRight)
            item.newPos.setX(ref - n.size.width());
        else if (alignment & Qt::AlignHCenter)
            item.newPos.setX(ref - n.size.width() / 2.0);
        else if (alignment & Qt::AlignTop)
            item.newPos.setY(ref);
        else if (alignment & Qt::AlignBottom)
            item.newPos.setY(ref - n.size.height());
        else if (alignment & Qt::AlignVCenter)
            item.newPos.setY(ref - n.size.height() / 2.0);
        if (item.oldPos != item.newPos)
            items.append(item);
    }
    if (!items.isEmpty())
        m_undoStack->push(new MoveNodesCommand(m_document, items));
}

void CanvasScene::distributeSelection(Qt::Orientation orientation)
{
    QStringList ids = selectedNodeIds();
    if (ids.size() < 3 || !m_document || !m_undoStack)
        return;

    std::sort(ids.begin(), ids.end(), [this, orientation](const QString &a, const QString &b) {
        const NodeModel na = m_document->node(a);
        const NodeModel nb = m_document->node(b);
        return orientation == Qt::Horizontal ? na.position.x() < nb.position.x()
                                            : na.position.y() < nb.position.y();
    });

    const NodeModel first = m_document->node(ids.first());
    const NodeModel last = m_document->node(ids.last());
    const qreal start = orientation == Qt::Horizontal ? first.position.x() : first.position.y();
    const qreal end = orientation == Qt::Horizontal ? last.position.x() : last.position.y();
    const qreal step = (end - start) / (ids.size() - 1);

    QVector<MoveNodesCommand::Item> items;
    for (int i = 1; i < ids.size() - 1; ++i) {
        NodeModel n = m_document->node(ids.at(i));
        MoveNodesCommand::Item item;
        item.id = n.id;
        item.oldPos = n.position;
        item.newPos = n.position;
        if (orientation == Qt::Horizontal)
            item.newPos.setX(start + step * i);
        else
            item.newPos.setY(start + step * i);
        items.append(item);
    }
    if (!items.isEmpty())
        m_undoStack->push(new MoveNodesCommand(m_document, items));
}

void CanvasScene::bringSelectionForward(bool toFront)
{
    if (!m_document)
        return;
    int maxZ = 0;
    for (const NodeModel &n : m_document->nodes())
        maxZ = qMax(maxZ, n.zValue);

    for (const QString &id : selectedNodeIds()) {
        NodeModel n = m_document->node(id);
        n.zValue = toFront ? (maxZ + 1) : (n.zValue + 1);
        m_document->updateNode(n);
        maxZ = qMax(maxZ, n.zValue);
    }
}

void CanvasScene::sendSelectionBackward(bool toBack)
{
    if (!m_document)
        return;
    int minZ = 0;
    for (const NodeModel &n : m_document->nodes())
        minZ = qMin(minZ, n.zValue);

    for (const QString &id : selectedNodeIds()) {
        NodeModel n = m_document->node(id);
        n.zValue = toBack ? (minZ - 1) : (n.zValue - 1);
        m_document->updateNode(n);
        minZ = qMin(minZ, n.zValue);
    }
}

void CanvasScene::groupSelection()
{
    const QStringList ids = selectedNodeIds();
    if (ids.size() < 2 || !m_document || !m_undoStack)
        return;
    const QString groupId = Document::createId(QStringLiteral("group"));
    auto *macro = new QUndoCommand(QStringLiteral("组合"));
    for (const QString &id : ids) {
        NodeModel oldNode = m_document->node(id);
        NodeModel newNode = oldNode;
        newNode.groupId = groupId;
        new ChangeNodePropertyCommand(m_document, oldNode, newNode, macro);
    }
    m_undoStack->push(macro);
    emit statusMessage(QStringLiteral("已分组 %1 个节点").arg(ids.size()));
}

void CanvasScene::ungroupSelection()
{
    const QStringList ids = selectedNodeIds();
    if (ids.isEmpty() || !m_document || !m_undoStack)
        return;
    auto *macro = new QUndoCommand(QStringLiteral("取消组合"));
    bool any = false;
    for (const QString &id : ids) {
        NodeModel oldNode = m_document->node(id);
        if (oldNode.groupId.isEmpty())
            continue;
        NodeModel newNode = oldNode;
        newNode.groupId.clear();
        new ChangeNodePropertyCommand(m_document, oldNode, newNode, macro);
        any = true;
    }
    if (any)
        m_undoStack->push(macro);
    else
        delete macro;
}

void CanvasScene::autoLayoutSelection()
{
    if (!m_document || !m_undoStack)
        return;
    QStringList ids = selectedNodeIds();
    if (ids.size() < 2)
        ids = [&]() {
            QStringList all;
            for (const NodeModel &n : m_document->nodes())
                all.append(n.id);
            return all;
        }();
    if (ids.size() < 2)
        return;

    QHash<QString, int> indegree;
    QHash<QString, QStringList> outs;
    QSet<QString> idSet(ids.begin(), ids.end());
    for (const QString &id : ids)
        indegree[id] = 0;
    for (const ConnectionModel &c : m_document->connections()) {
        if (!idSet.contains(c.sourceNodeId) || !idSet.contains(c.targetNodeId))
            continue;
        outs[c.sourceNodeId].append(c.targetNodeId);
        indegree[c.targetNodeId] += 1;
    }

    QStringList queue;
    for (const QString &id : ids) {
        if (indegree.value(id) == 0)
            queue.append(id);
    }
    QStringList order;
    while (!queue.isEmpty()) {
        const QString id = queue.takeFirst();
        order.append(id);
        for (const QString &next : outs.value(id)) {
            indegree[next] -= 1;
            if (indegree[next] == 0)
                queue.append(next);
        }
    }
    for (const QString &id : ids) {
        if (!order.contains(id))
            order.append(id);
    }

    constexpr qreal kDx = 220.0;
    constexpr qreal kDy = 120.0;

    // Layered DAG layout: Kahn level assignment, left-to-right columns.
    QHash<QString, int> level;
    QHash<QString, int> remaining = indegree;
    QStringList frontier;
    for (const QString &id : ids) {
        if (indegree.value(id) == 0) {
            frontier.append(id);
            level[id] = 0;
        }
    }
    while (!frontier.isEmpty()) {
        const QString id = frontier.takeFirst();
        const int nextLevel = level.value(id) + 1;
        for (const QString &next : outs.value(id)) {
            level[next] = qMax(level.value(next, 0), nextLevel);
            remaining[next] -= 1;
            if (remaining[next] == 0)
                frontier.append(next);
        }
    }
    for (const QString &id : ids) {
        if (!level.contains(id))
            level[id] = 0;
    }

    QHash<int, QStringList> byLevel;
    int maxLevel = 0;
    for (const QString &id : order) {
        const int lv = level.value(id, 0);
        byLevel[lv].append(id);
        maxLevel = qMax(maxLevel, lv);
    }

    QVector<MoveNodesCommand::Item> moves;
    for (int lv = 0; lv <= maxLevel; ++lv) {
        const QStringList &col = byLevel.value(lv);
        const qreal startY = -0.5 * (col.size() - 1) * kDy;
        for (int i = 0; i < col.size(); ++i) {
            const QString &id = col.at(i);
            if (!m_document->hasNode(id))
                continue;
            MoveNodesCommand::Item item;
            item.id = id;
            item.oldPos = m_document->node(id).position;
            item.newPos = QPointF(lv * kDx, startY + i * kDy);
            if (item.oldPos != item.newPos)
                moves.append(item);
        }
    }
    if (!moves.isEmpty()) {
        m_undoStack->push(new MoveNodesCommand(m_document, moves));
        emit statusMessage(QStringLiteral("已按流程层级自动布局 %1 个节点").arg(moves.size()));
    }
}

void CanvasScene::toggleBreakpointOnSelection()
{
    const QStringList ids = selectedNodeIds();
    if (ids.isEmpty() || !m_document || !m_undoStack)
        return;
    auto *macro = new QUndoCommand(QStringLiteral("切换断点"));
    for (const QString &id : ids) {
        NodeModel oldNode = m_document->node(id);
        NodeModel newNode = oldNode;
        newNode.breakpoint = !oldNode.breakpoint;
        new ChangeNodePropertyCommand(m_document, oldNode, newNode, macro);
    }
    m_undoStack->push(macro);
}

void CanvasScene::setValidationWarning(const QString &nodeId, bool warning)
{
    m_validationWarnings.insert(nodeId, warning);
    if (NodeGraphicsItem *item = nodeItem(nodeId)) {
        if (warning && item->runStatus() == NodeRunVisualStatus::Idle)
            item->setRunStatus(NodeRunVisualStatus::Warning);
        else if (!warning && item->runStatus() == NodeRunVisualStatus::Warning)
            item->setRunStatus(NodeRunVisualStatus::Idle);
    }
}

PortGraphicsItem *CanvasScene::findCompatiblePortNear(const QPointF &scenePos, qreal radius) const
{
    if (!m_sourcePort || !m_document)
        return nullptr;
    NodeGraphicsItem *sourceNode = m_sourcePort->ownerNode();
    if (!sourceNode)
        return nullptr;

    PortGraphicsItem *best = nullptr;
    qreal bestDist = radius;
    for (auto it = m_nodes.constBegin(); it != m_nodes.constEnd(); ++it) {
        NodeGraphicsItem *node = it.value();
        if (!node || node == sourceNode)
            continue;
        for (const PortModel &port : node->model().ports) {
            PortGraphicsItem *portItem = node->portItem(port.id);
            if (!portItem)
                continue;
            const bool targetIsInput = port.direction == PortDirection::Input
                || port.direction == PortDirection::Both;
            if (!targetIsInput)
                continue;
            const qreal dist = QLineF(scenePos, portItem->scenePos()).length();
            if (dist > bestDist)
                continue;
#if SELT_HAS_OPENCV
            QString err;
            if (!Selt::GraphValidator::canConnect(*m_document,
                                                  sourceNode->nodeId(), m_sourcePort->portId(),
                                                  node->nodeId(), port.id, &err, true)) {
                continue;
            }
#endif
            bestDist = dist;
            best = portItem;
        }
    }
    return best;
}

void CanvasScene::refreshConnectionHighlights(const QPointF &scenePos)
{
    Q_UNUSED(scenePos);
    if (!m_sourcePort || !m_document) {
        clearConnectionHighlights();
        return;
    }
    NodeGraphicsItem *sourceNode = m_sourcePort->ownerNode();
    for (auto it = m_nodes.constBegin(); it != m_nodes.constEnd(); ++it) {
        NodeGraphicsItem *node = it.value();
        if (!node)
            continue;
        for (const PortModel &port : node->model().ports) {
            PortGraphicsItem *portItem = node->portItem(port.id);
            if (!portItem)
                continue;
            if (node == sourceNode) {
                portItem->clearCompatibilityHighlight();
                continue;
            }
            const bool targetIsInput = port.direction == PortDirection::Input
                || port.direction == PortDirection::Both;
            if (!targetIsInput) {
                portItem->setCompatibilityHighlight(false, true);
                continue;
            }
            bool ok = true;
#if SELT_HAS_OPENCV
            QString err;
            ok = Selt::GraphValidator::canConnect(*m_document,
                                                  sourceNode->nodeId(), m_sourcePort->portId(),
                                                  node->nodeId(), port.id, &err, true);
#else
            Q_UNUSED(sourceNode);
#endif
            portItem->setCompatibilityHighlight(ok, !ok);
        }
    }
}

void CanvasScene::clearConnectionHighlights()
{
    for (auto it = m_nodes.constBegin(); it != m_nodes.constEnd(); ++it) {
        NodeGraphicsItem *node = it.value();
        if (!node)
            continue;
        for (const PortModel &port : node->model().ports) {
            if (PortGraphicsItem *portItem = node->portItem(port.id))
                portItem->clearCompatibilityHighlight();
        }
    }
}

QString CanvasScene::occupiedConnectionId(const QString &targetNodeId, const QString &targetPortId) const
{
    if (!m_document)
        return {};
    for (const ConnectionModel &c : m_document->connections()) {
        if (c.targetNodeId == targetNodeId && c.targetPortId == targetPortId)
            return c.id;
    }
    return {};
}

void CanvasScene::drawBackground(QPainter *painter, const QRectF &rect)
{
    if (!m_document) {
        QGraphicsScene::drawBackground(painter, rect);
        return;
    }

    painter->fillRect(rect, m_document->settings().backgroundColor);
    if (!m_document->settings().showGrid)
        return;

    const int grid = qMax(1, m_document->settings().gridSize);
    const int majorEvery = 5;
    const QColor bg = m_document->settings().backgroundColor;
    const bool dark = bg.lightness() < 128;
    QPen minorPen(dark ? Selt::UiStyle::gridLine() : QColor(220, 220, 230));
    minorPen.setWidthF(0);
    QPen majorPen(dark ? Selt::UiStyle::gridMajorLine() : QColor(255, 140, 0, 40));
    majorPen.setWidthF(0);

    const int left = static_cast<int>(std::floor(rect.left() / grid)) * grid;
    const int top = static_cast<int>(std::floor(rect.top() / grid)) * grid;
    for (int x = left; x < rect.right(); x += grid) {
        const bool major = (x % (grid * majorEvery)) == 0;
        painter->setPen(major ? majorPen : minorPen);
        painter->drawLine(x, static_cast<int>(rect.top()), x, static_cast<int>(rect.bottom()));
    }
    for (int y = top; y < rect.bottom(); y += grid) {
        const bool major = (y % (grid * majorEvery)) == 0;
        painter->setPen(major ? majorPen : minorPen);
        painter->drawLine(static_cast<int>(rect.left()), y, static_cast<int>(rect.right()), y);
    }
}

void CanvasScene::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        deleteSelection();
        event->accept();
        return;
    }
    if (event->matches(QKeySequence::SelectAll)) {
        selectAll();
        event->accept();
        return;
    }
    if (event->matches(QKeySequence::Copy)) {
        copySelection();
        event->accept();
        return;
    }
    if (event->matches(QKeySequence::Paste)) {
        pasteClipboard(QPointF(0, 0));
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape) {
        cancelConnection();
        clearSelection();
        event->accept();
        return;
    }
    QGraphicsScene::keyPressEvent(event);
}

void CanvasScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (isConnecting())
        updateTempConnection(event->scenePos());
    QGraphicsScene::mouseMoveEvent(event);
}

void CanvasScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (isConnecting() && event->button() == Qt::LeftButton) {
        PortGraphicsItem *target = findCompatiblePortNear(event->scenePos(), 28.0);
        if (!target) {
            for (QGraphicsItem *item : items(event->scenePos())) {
                target = qgraphicsitem_cast<PortGraphicsItem *>(item);
                if (target)
                    break;
            }
        }
        if (target)
            finishConnection(target);
        else
            cancelConnection();
        event->accept();
        return;
    }
    QGraphicsScene::mouseReleaseEvent(event);
}

void CanvasScene::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    QMenu menu;
    NodeGraphicsItem *contextNode = nullptr;
    for (QGraphicsItem *item : items(event->scenePos())) {
        contextNode = qgraphicsitem_cast<NodeGraphicsItem *>(item);
        if (contextNode)
            break;
    }
    if (contextNode) {
        const QString nodeId = contextNode->nodeId();
        QMenu *runMenu = menu.addMenu(QStringLiteral("调试运行"));
        runMenu->addAction(QStringLiteral("运行此节点"), this,
                           [this, nodeId]() { emit runNodeRequested(nodeId); });
        runMenu->addAction(QStringLiteral("从此节点运行"), this,
                           [this, nodeId]() { emit runFromNodeRequested(nodeId); });
        menu.addAction(QStringLiteral("查看上游依赖"), this,
                       [this, nodeId]() { emit inspectUpstreamRequested(nodeId); });
        menu.addAction(QStringLiteral("切换断点"), this, [this, nodeId]() {
            clearSelection();
            if (NodeGraphicsItem *item = nodeItem(nodeId))
                item->setSelected(true);
            toggleBreakpointOnSelection();
        });
        menu.addSeparator();
    }
    QMenu *createMenu = menu.addMenu(QStringLiteral("创建视觉模块"));
#if SELT_HAS_OPENCV
    for (const QString &category : VisionNodeRegistry::categories()) {
        QMenu *catMenu = createMenu->addMenu(category);
        for (const QString &type : VisionNodeRegistry::typesInCategory(category)) {
            catMenu->addAction(NodeBuilder::displayName(type), this, [this, type, pos = event->scenePos()]() {
                createNodeAt(type, pos);
            });
        }
    }
#else
    createMenu->addAction(QStringLiteral("OpenCV 未启用"))->setEnabled(false);
#endif
    menu.addSeparator();
    menu.addAction(QStringLiteral("删除"), this, [this]() { deleteSelection(); });
    menu.addAction(QStringLiteral("复制"), this, [this]() { copySelection(); });
    menu.addAction(QStringLiteral("粘贴"), this, [this, pos = event->scenePos()]() { pasteClipboard(pos); });
    menu.exec(event->screenPos());
}

void CanvasScene::onNodeAdded(const NodeModel &node)
{
    if (m_nodes.contains(node.id))
        return;
    auto *item = new NodeGraphicsItem(node);
    addItem(item);
    connect(item, &NodeGraphicsItem::doubleClicked,
            this, &CanvasScene::nodeDoubleClicked);
    connect(item, &NodeGraphicsItem::collapseToggled, this, [this](const QString &nodeId, bool collapsed) {
        if (!m_document || !m_document->hasNode(nodeId))
            return;
        NodeModel model = m_document->node(nodeId);
        if (model.collapsed == collapsed)
            return;
        model.collapsed = collapsed;
        m_document->updateNode(model);
    });
    m_nodes.insert(node.id, item);
}

void CanvasScene::onNodeRemoved(const QString &id)
{
    if (auto *item = m_nodes.take(id)) {
        removeItem(item);
        delete item;
    }
}

void CanvasScene::onNodeUpdated(const NodeModel &node)
{
    if (auto *item = m_nodes.value(node.id)) {
        // Avoid feedback loop during drag: only sync if not the active mover with same visual pos
        item->setModel(node);
        updateConnectionsForNode(node.id);
    }
}

void CanvasScene::onConnectionAdded(const ConnectionModel &connection)
{
    if (m_connections.contains(connection.id))
        return;
    auto *item = new ConnectionGraphicsItem(connection);
    addItem(item);
    m_connections.insert(connection.id, item);
    item->updatePath();

    // Refresh port capsules so connected state / forced exposure updates immediately.
    if (auto *src = m_nodes.value(connection.sourceNodeId))
        src->setModel(m_document ? m_document->node(connection.sourceNodeId) : src->model());
    if (auto *dst = m_nodes.value(connection.targetNodeId))
        dst->setModel(m_document ? m_document->node(connection.targetNodeId) : dst->model());
    updateConnectionsForNode(connection.sourceNodeId);
    updateConnectionsForNode(connection.targetNodeId);
}

void CanvasScene::onConnectionRemoved(const QString &id)
{
    QString sourceNodeId;
    QString targetNodeId;
    if (auto *existing = m_connections.value(id)) {
        sourceNodeId = existing->model().sourceNodeId;
        targetNodeId = existing->model().targetNodeId;
        removeItem(existing);
        delete existing;
        m_connections.remove(id);
    }
    if (!sourceNodeId.isEmpty() && m_nodes.contains(sourceNodeId) && m_document
        && m_document->hasNode(sourceNodeId)) {
        m_nodes.value(sourceNodeId)->setModel(m_document->node(sourceNodeId));
        updateConnectionsForNode(sourceNodeId);
    }
    if (!targetNodeId.isEmpty() && m_nodes.contains(targetNodeId) && m_document
        && m_document->hasNode(targetNodeId)) {
        m_nodes.value(targetNodeId)->setModel(m_document->node(targetNodeId));
        updateConnectionsForNode(targetNodeId);
    }
}

void CanvasScene::onConnectionUpdated(const ConnectionModel &connection)
{
    if (auto *item = m_connections.value(connection.id)) {
        item->setModel(connection);
        item->updatePath();
    }
}

void CanvasScene::onDocumentReset()
{
    rebuildFromDocument();
}

void CanvasScene::onSelectionChanged()
{
    const QStringList ids = selectedNodeIds();
    QSet<QString> related;
    for (const QString &id : ids)
        related.insert(id);
    if (m_document) {
        for (const ConnectionModel &c : m_document->connections()) {
            if (related.contains(c.sourceNodeId) || related.contains(c.targetNodeId)) {
                related.insert(c.sourceNodeId);
                related.insert(c.targetNodeId);
            }
        }
    }
    for (auto it = m_connections.constBegin(); it != m_connections.constEnd(); ++it) {
        ConnectionGraphicsItem *item = it.value();
        if (!item)
            continue;
        const ConnectionModel model = item->model();
        const bool highlight = !ids.isEmpty()
            && (related.contains(model.sourceNodeId) && related.contains(model.targetNodeId))
            && (ids.contains(model.sourceNodeId) || ids.contains(model.targetNodeId));
        item->setRelatedHighlight(highlight);
    }
    emit selectionNodeChanged(ids.size() == 1 ? ids.first() : QString());
}

void CanvasScene::connectDocument()
{
    if (!m_document)
        return;
    connect(m_document, &Document::nodeAdded, this, &CanvasScene::onNodeAdded);
    connect(m_document, &Document::nodeRemoved, this, &CanvasScene::onNodeRemoved);
    connect(m_document, &Document::nodeUpdated, this, &CanvasScene::onNodeUpdated);
    connect(m_document, &Document::connectionAdded, this, &CanvasScene::onConnectionAdded);
    connect(m_document, &Document::connectionRemoved, this, &CanvasScene::onConnectionRemoved);
    connect(m_document, &Document::connectionUpdated, this, &CanvasScene::onConnectionUpdated);
    connect(m_document, &Document::documentReset, this, &CanvasScene::onDocumentReset);
    connect(m_document, &Document::settingsChanged, this, [this](const DocumentSettings &) {
        invalidate(sceneRect(), QGraphicsScene::BackgroundLayer);
    });
}

QPointF CanvasScene::snapPoint(const QPointF &pos) const
{
    if (!m_document || !m_document->settings().snapToGrid)
        return pos;
    const int grid = qMax(1, m_document->settings().gridSize);
    return QPointF(qRound(pos.x() / grid) * grid, qRound(pos.y() / grid) * grid);
}
