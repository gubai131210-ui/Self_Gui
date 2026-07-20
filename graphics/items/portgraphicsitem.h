#ifndef PORTGRAPHICSITEM_H
#define PORTGRAPHICSITEM_H

#include "core/model/portmodel.h"

#include <QGraphicsEllipseItem>
#include <QString>

class NodeGraphicsItem;

class PortGraphicsItem : public QGraphicsEllipseItem
{
public:
    enum { Type = UserType + 1 };
    int type() const override { return Type; }

    static constexpr qreal Radius = 5.0;

    PortGraphicsItem(const PortModel &port, NodeGraphicsItem *parent);

    QString portId() const { return m_port.id; }
    PortModel port() const { return m_port; }
    void setPort(const PortModel &port);
    void updatePosition(const QSizeF &nodeSize);
    NodeGraphicsItem *ownerNode() const;

protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;

private:
    PortModel m_port;
};

#endif // PORTGRAPHICSITEM_H
