#include "connectiongraphicsitem.h"

#include "nodegraphicsitem.h"
#include "graphics/canvas/canvasscene.h"

#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QtMath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

ConnectionGraphicsItem::ConnectionGraphicsItem(const ConnectionModel &model, QGraphicsItem *parent)
    : QGraphicsPathItem(parent)
    , m_model(model)
{
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setZValue(-1);
    setPen(QPen(QColor(70, 70, 90), 2.0));
}

void ConnectionGraphicsItem::setModel(const ConnectionModel &model)
{
    m_model = model;
    updatePath();
}

void ConnectionGraphicsItem::updatePath()
{
    auto *scene = canvasScene();
    if (!scene)
        return;

    NodeGraphicsItem *source = scene->nodeItem(m_model.sourceNodeId);
    NodeGraphicsItem *target = scene->nodeItem(m_model.targetNodeId);
    if (!source || !target)
        return;

    const QPointF start = source->portScenePos(m_model.sourcePortId);
    const QPointF end = target->portScenePos(m_model.targetPortId);
    setPath(buildPath(start, end));
    update();
}

QPainterPath ConnectionGraphicsItem::shape() const
{
    QPainterPathStroker stroker;
    stroker.setWidth(10.0);
    return stroker.createStroke(path());
}

void ConnectionGraphicsItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(widget);
    painter->setRenderHint(QPainter::Antialiasing, true);

    QPen pen(QColor(70, 70, 90), 2.0);
    if (option->state & QStyle::State_Selected)
        pen = QPen(QColor(255, 140, 0), 3.0, Qt::DashLine);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(path());

    if (m_model.showArrow && !path().isEmpty()) {
        const QPainterPath p = path();
        const qreal len = p.length();
        if (len > 1.0) {
            const QPointF tip = p.pointAtPercent(1.0);
            const QPointF before = p.pointAtPercent(qMax(0.0, 1.0 - 12.0 / len));
            const QLineF line(before, tip);
            const qreal angle = std::atan2(-line.dy(), line.dx());
            QPolygonF arrow;
            arrow << tip
                  << tip + QPointF(std::sin(angle + M_PI / 3) * 10, std::cos(angle + M_PI / 3) * 10)
                  << tip + QPointF(std::sin(angle + M_PI - M_PI / 3) * 10, std::cos(angle + M_PI - M_PI / 3) * 10);
            painter->setBrush(pen.color());
            painter->drawPolygon(arrow);
        }
    }

    if (!m_model.label.isEmpty()) {
        const QPointF mid = path().pointAtPercent(0.5);
        painter->setPen(QColor(40, 40, 40));
        painter->drawText(mid + QPointF(4, -4), m_model.label);
    }
}

CanvasScene *ConnectionGraphicsItem::canvasScene() const
{
    return qobject_cast<CanvasScene *>(scene());
}

QPainterPath ConnectionGraphicsItem::buildPath(const QPointF &start, const QPointF &end) const
{
    QPainterPath path;
    path.moveTo(start);
    if (m_model.lineStyle == ConnectionLineStyle::Straight) {
        path.lineTo(end);
    } else {
        const qreal midX = (start.x() + end.x()) / 2.0;
        path.lineTo(midX, start.y());
        path.lineTo(midX, end.y());
        path.lineTo(end);
    }
    return path;
}
