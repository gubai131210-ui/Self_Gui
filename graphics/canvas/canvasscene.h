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
    void bringSelectionForward(bool toFront);
    void sendSelectionBackward(bool toBack);
    void groupSelection();
    void ungroupSelection();

signals:
    void selectionNodeChanged(const QString &nodeId);
    void statusMessage(const QString &message);

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

    Document *m_document{nullptr};
    QUndoStack *m_undoStack{nullptr};
    QHash<QString, NodeGraphicsItem *> m_nodes;
    QHash<QString, ConnectionGraphicsItem *> m_connections;
    PortGraphicsItem *m_sourcePort{nullptr};
    QGraphicsPathItem *m_tempConnection{nullptr};
    QVector<NodeModel> m_clipboardNodes;
    QVector<ConnectionModel> m_clipboardConnections;
};

#endif // CANVASSCENE_H
