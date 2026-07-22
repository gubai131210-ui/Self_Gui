#ifndef CANVASSCENE_H
#define CANVASSCENE_H

#include "core/model/document.h"

#include <QGraphicsScene>
#include <QHash>
#include <QUndoStack>

class NodeGraphicsItem;
class ConnectionGraphicsItem;
class PortGraphicsItem;

class CanvasScene : public QGraphicsScene
{
    Q_OBJECT

public:
    explicit CanvasScene(Document *document, QUndoStack *undoStack, QObject *parent = nullptr);

    Document *document() const { return m_document; }
    QUndoStack *undoStack() const { return m_undoStack; }
    void setUndoStack(QUndoStack *undoStack);

    NodeGraphicsItem *nodeItem(const QString &id) const;
    ConnectionGraphicsItem *connectionItem(const QString &id) const;

    void rebuildFromDocument();
    void createNodeAt(const QString &type, const QPointF &scenePos);
    void deleteSelection();
    void selectAll();
    QStringList selectedNodeIds() const;
    QStringList selectedConnectionIds() const;

    void beginConnection(PortGraphicsItem *port);
    void updateTempConnection(const QPointF &scenePos);
    void finishConnection(PortGraphicsItem *targetPort);
    void cancelConnection();
    bool isConnecting() const { return m_sourcePort != nullptr; }
    PortGraphicsItem *sourcePort() const { return m_sourcePort; }

    void notifyNodeMoved(const QString &nodeId, const QPointF &oldPos, const QPointF &newPos);
    void updateConnectionsForNode(const QString &nodeId);

    void copySelection();
    void pasteClipboard(const QPointF &anchor);
    void alignSelection(Qt::Alignment alignment);
    void distributeSelection(Qt::Orientation orientation);
    void autoLayoutSelection();
    void bringSelectionForward(bool toFront);
    void sendSelectionBackward(bool toBack);
    void groupSelection();
    void ungroupSelection();
    void toggleBreakpointOnSelection();
    void setValidationWarning(const QString &nodeId, bool warning);

signals:
    void selectionNodeChanged(const QString &nodeId);
    void nodeDoubleClicked(const QString &nodeId);
    void runNodeRequested(const QString &nodeId);
    void runFromNodeRequested(const QString &nodeId);
    void inspectUpstreamRequested(const QString &nodeId);
    void statusMessage(const QString &message);
    /// Empty string clears toolbox type filter after connection drag ends.
    void connectionDataTypeFilterRequested(const QString &dataTypeId);
    /// 任意入口（双击工具箱 / 拖放 / 右键菜单）成功创建节点后发出，供刷新「最近使用」。
    void nodeTypeCreated(const QString &type);

protected:
    void drawBackground(QPainter *painter, const QRectF &rect) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

private slots:
    void onNodeAdded(const NodeModel &node);
    void onNodeRemoved(const QString &id);
    void onNodeUpdated(const NodeModel &node);
    void onConnectionAdded(const ConnectionModel &connection);
    void onConnectionRemoved(const QString &id);
    void onConnectionUpdated(const ConnectionModel &connection);
    void onDocumentReset();
    void onSelectionChanged();

private:
    void connectDocument();
    QPointF snapPoint(const QPointF &pos) const;
    PortGraphicsItem *findCompatiblePortNear(const QPointF &scenePos, qreal radius) const;
    void refreshConnectionHighlights(const QPointF &scenePos);
    void clearConnectionHighlights();
    QString occupiedConnectionId(const QString &targetNodeId, const QString &targetPortId) const;

    Document *m_document{nullptr};
    QUndoStack *m_undoStack{nullptr};
    QHash<QString, NodeGraphicsItem *> m_nodes;
    QHash<QString, ConnectionGraphicsItem *> m_connections;
    PortGraphicsItem *m_sourcePort{nullptr};
    QGraphicsPathItem *m_tempConnection{nullptr};
    QVector<NodeModel> m_clipboardNodes;
    QVector<ConnectionModel> m_clipboardConnections;
    QHash<QString, bool> m_validationWarnings;
};

#endif // CANVASSCENE_H
