#ifndef NODEGRAPHICSITEM_H
#define NODEGRAPHICSITEM_H

#include "core/model/nodemodel.h"

#include <QGraphicsObject>
#include <QHash>

class PortGraphicsItem;
class CanvasScene;

enum class NodeRunVisualStatus {
    Idle,
    Running,
    Success,
    Failed,
    Disabled
};

class NodeGraphicsItem : public QGraphicsObject
{
    Q_OBJECT

public:
    enum { Type = UserType + 2 };
    int type() const override { return Type; }

    explicit NodeGraphicsItem(const NodeModel &model, QGraphicsItem *parent = nullptr);

    QString nodeId() const { return m_model.id; }
    NodeModel model() const { return m_model; }
    void setModel(const NodeModel &model);
    void setRunStatus(NodeRunVisualStatus status);
    NodeRunVisualStatus runStatus() const { return m_runStatus; }
    PortGraphicsItem *portItem(const QString &portId) const;
    QPointF portScenePos(const QString &portId) const;

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

signals:
    void moveFinished(const QString &nodeId, const QPointF &oldPos, const QPointF &newPos);

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
    void rebuildPorts();
    QPainterPath shapePath() const;
    CanvasScene *canvasScene() const;

    NodeModel m_model;
    QHash<QString, PortGraphicsItem *> m_ports;
    QPointF m_pressScenePos;
    bool m_moving{false};
    NodeRunVisualStatus m_runStatus{NodeRunVisualStatus::Idle};
};

#endif // NODEGRAPHICSITEM_H
