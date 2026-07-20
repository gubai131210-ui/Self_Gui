#ifndef DOCUMENT_H
#define DOCUMENT_H

#include "connectionmodel.h"
#include "documentsettings.h"
#include "nodemodel.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUuid>
#include <QVector>

class Document : public QObject
{
    Q_OBJECT

public:
    explicit Document(QObject *parent = nullptr);

    QString title() const { return m_title; }
    void setTitle(const QString &title);

    DocumentSettings settings() const { return m_settings; }
    void setSettings(const DocumentSettings &settings);

    const QVector<NodeModel> &nodes() const { return m_nodes; }
    const QVector<ConnectionModel> &connections() const { return m_connections; }

    bool hasNode(const QString &id) const;
    bool hasConnection(const QString &id) const;
    NodeModel node(const QString &id) const;
    ConnectionModel connection(const QString &id) const;
    int nodeIndex(const QString &id) const;
    int connectionIndex(const QString &id) const;

    QString addNode(NodeModel node);
    bool removeNode(const QString &id);
    bool updateNode(const NodeModel &node);
    bool setNodePosition(const QString &id, const QPointF &pos);
    bool setNodeSize(const QString &id, const QSizeF &size);
    bool setNodeZValue(const QString &id, int z);

    QString addConnection(ConnectionModel connection);
    bool removeConnection(const QString &id);
    bool updateConnection(const ConnectionModel &connection);
    QVector<ConnectionModel> connectionsForNode(const QString &nodeId) const;
    QStringList removeConnectionsForNode(const QString &nodeId);

    void clear();
    void replaceAll(const QString &title,
                    const DocumentSettings &settings,
                    const QVector<NodeModel> &nodes,
                    const QVector<ConnectionModel> &connections);

    bool isModified() const { return m_modified; }
    void setModified(bool modified);
    void markClean();

    static QString createId(const QString &prefix = QStringLiteral("id"));

signals:
    void titleChanged(const QString &title);
    void settingsChanged(const DocumentSettings &settings);
    void nodeAdded(const NodeModel &node);
    void nodeRemoved(const QString &id);
    void nodeUpdated(const NodeModel &node);
    void connectionAdded(const ConnectionModel &connection);
    void connectionRemoved(const QString &id);
    void connectionUpdated(const ConnectionModel &connection);
    void documentReset();
    void modifiedChanged(bool modified);

private:
    void rebuildIndexes();
    void setModifiedInternal(bool modified);

    QString m_title{QStringLiteral("未命名文档")};
    DocumentSettings m_settings;
    QVector<NodeModel> m_nodes;
    QVector<ConnectionModel> m_connections;
    QHash<QString, int> m_nodeIndex;
    QHash<QString, int> m_connectionIndex;
    bool m_modified{false};
};

#endif // DOCUMENT_H
