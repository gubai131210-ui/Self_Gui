#include "canvasscene.h"

#include "core/command/documentcommands.h"
#include "core/model/document.h"
#include "core/registry/nodefactory.h"
#include "graphics/items/connectiongraphicsitem.h"
#include "graphics/items/nodegraphicsitem.h"
#include "graphics/items/portgraphicsitem.h"

#include <QGraphicsPathItem>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
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
    NodeModel node = NodeFactory::create(type, snapPoint(scenePos));
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
}

void CanvasScene::updateTempConnection(const QPointF &scenePos)
{
    if (!m_sourcePort || !m_tempConnection)
        return;
    QPainterPath path;
    path.moveTo(m_sourcePort->scenePos());
    path.lineTo(scenePos);
    m_tempConnection->setPath(path);
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

    ConnectionModel connection;
    connection.id = Document::createId(QStringLiteral("conn"));
    connection.sourceNodeId = sourceNode->nodeId();
    connection.sourcePortId = m_sourcePort->portId();
    connection.targetNodeId = targetNode->nodeId();
    connection.targetPortId = targetPort->portId();
    connection.lineStyle = ConnectionLineStyle::Orthogonal;
    connection.showArrow = true;

    cancelConnection();
    m_undoStack->push(new AddConnectionCommand(m_document, connection));
}

void CanvasScene::cancelConnection()
{
    if (m_tempConnection) {
        removeItem(m_tempConnection);
        delete m_tempConnection;
        m_tempConnection = nullptr;
    }
    m_sourcePort = nullptr;
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
    QPen minorPen(QColor(220, 220, 230));
    minorPen.setWidthF(0);
    painter->setPen(minorPen);

    const int left = static_cast<int>(std::floor(rect.left() / grid)) * grid;
    const int top = static_cast<int>(std::floor(rect.top() / grid)) * grid;
    for (int x = left; x < rect.right(); x += grid)
        painter->drawLine(x, static_cast<int>(rect.top()), x, static_cast<int>(rect.bottom()));
    for (int y = top; y < rect.bottom(); y += grid)
        painter->drawLine(static_cast<int>(rect.left()), y, static_cast<int>(rect.right()), y);
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
        PortGraphicsItem *target = nullptr;
        for (QGraphicsItem *item : items(event->scenePos())) {
            target = qgraphicsitem_cast<PortGraphicsItem *>(item);
            if (target)
                break;
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
    QMenu *createMenu = menu.addMenu(QStringLiteral("创建节点"));
    for (const QString &type : NodeFactory::supportedTypes()) {
        createMenu->addAction(NodeFactory::displayName(type), this, [this, type, pos = event->scenePos()]() {
            createNodeAt(type, pos);
        });
    }
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
}

void CanvasScene::onConnectionRemoved(const QString &id)
{
    if (auto *item = m_connections.take(id)) {
        removeItem(item);
        delete item;
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
