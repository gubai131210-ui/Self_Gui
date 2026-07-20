#ifndef DOCUMENTCOMMANDS_H
#define DOCUMENTCOMMANDS_H

#include "core/model/connectionmodel.h"
#include "core/model/document.h"
#include "core/model/nodemodel.h"

#include <QPointF>
#include <QSizeF>
#include <QUndoCommand>
#include <QVector>

class AddNodeCommand : public QUndoCommand
{
public:
    AddNodeCommand(Document *document, const NodeModel &node, QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;

private:
    Document *m_document;
    NodeModel m_node;
    bool m_first{true};
};

class DeleteNodesCommand : public QUndoCommand
{
public:
    DeleteNodesCommand(Document *document, const QStringList &nodeIds, QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;

private:
    Document *m_document;
    QVector<NodeModel> m_nodes;
    QVector<ConnectionModel> m_connections;
};

class MoveNodesCommand : public QUndoCommand
{
public:
    struct Item {
        QString id;
        QPointF oldPos;
        QPointF newPos;
    };

    MoveNodesCommand(Document *document, const QVector<Item> &items, QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;

private:
    Document *m_document;
    QVector<Item> m_items;
};

class ResizeNodeCommand : public QUndoCommand
{
public:
    ResizeNodeCommand(Document *document,
                      const QString &nodeId,
                      const QSizeF &oldSize,
                      const QSizeF &newSize,
                      const QPointF &oldPos,
                      const QPointF &newPos,
                      QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;

private:
    Document *m_document;
    QString m_nodeId;
    QSizeF m_oldSize;
    QSizeF m_newSize;
    QPointF m_oldPos;
    QPointF m_newPos;
};

class AddConnectionCommand : public QUndoCommand
{
public:
    AddConnectionCommand(Document *document, const ConnectionModel &connection, QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;

private:
    Document *m_document;
    ConnectionModel m_connection;
    bool m_first{true};
};

class DeleteConnectionsCommand : public QUndoCommand
{
public:
    DeleteConnectionsCommand(Document *document, const QStringList &connectionIds, QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;

private:
    Document *m_document;
    QVector<ConnectionModel> m_connections;
};

class ChangeNodePropertyCommand : public QUndoCommand
{
public:
    ChangeNodePropertyCommand(Document *document,
                              const NodeModel &oldNode,
                              const NodeModel &newNode,
                              QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;

private:
    Document *m_document;
    NodeModel m_oldNode;
    NodeModel m_newNode;
};

class PasteNodesCommand : public QUndoCommand
{
public:
    PasteNodesCommand(Document *document,
                      const QVector<NodeModel> &nodes,
                      const QVector<ConnectionModel> &connections,
                      QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;

private:
    Document *m_document;
    QVector<NodeModel> m_nodes;
    QVector<ConnectionModel> m_connections;
    bool m_first{true};
};

#endif // DOCUMENTCOMMANDS_H
