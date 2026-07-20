#include "portgraphicsitem.h"

#include "nodegraphicsitem.h"
#include "graphics/canvas/canvasscene.h"

#include <QBrush>
#include <QCursor>
#include <QGraphicsSceneMouseEvent>
#include <QPen>

PortGraphicsItem::PortGraphicsItem(const PortModel &port, NodeGraphicsItem *parent)
    : QGraphicsEllipseItem(-Radius, -Radius, Radius * 2, Radius * 2, parent)
    , m_port(port)
{
    setAcceptHoverEvents(true);
    setBrush(QBrush(QColor(255, 255, 255)));
    setPen(QPen(QColor(40, 90, 180), 1.5));
    setZValue(10);
    setFlag(QGraphicsItem::ItemIsSelectable, false);
    setCursor(Qt::CrossCursor);
    updatePosition(parent->model().size);
}

void PortGraphicsItem::setPort(const PortModel &port)
{
    m_port = port;
    if (auto *node = ownerNode())
        updatePosition(node->model().size);
}

void PortGraphicsItem::updatePosition(const QSizeF &nodeSize)
{
    setPos(m_port.relativeX * nodeSize.width(), m_port.relativeY * nodeSize.height());
}

NodeGraphicsItem *PortGraphicsItem::ownerNode() const
{
    return qgraphicsitem_cast<NodeGraphicsItem *>(parentItem());
}

void PortGraphicsItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    setBrush(QBrush(QColor(120, 180, 255)));
    QGraphicsEllipseItem::hoverEnterEvent(event);
}

void PortGraphicsItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    setBrush(QBrush(QColor(255, 255, 255)));
    QGraphicsEllipseItem::hoverLeaveEvent(event);
}

void PortGraphicsItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (auto *scene = qobject_cast<CanvasScene *>(this->scene())) {
            scene->beginConnection(this);
            event->accept();
            return;
        }
    }
    QGraphicsEllipseItem::mousePressEvent(event);
}
