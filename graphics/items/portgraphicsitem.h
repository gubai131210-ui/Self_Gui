#ifndef PORTGRAPHICSITEM_H
#define PORTGRAPHICSITEM_H

#include "core/model/portmodel.h"

#include <QGraphicsObject>
#include <QColor>
#include <QString>

class NodeGraphicsItem;

class PortGraphicsItem : public QGraphicsObject
{
public:
    enum { Type = UserType + 1 };
    int type() const override { return Type; }

    static constexpr qreal CapsuleHalfWidth = 11.0;
    static constexpr qreal CapsuleHalfHeight = 7.0;

    PortGraphicsItem(const PortModel &port, NodeGraphicsItem *parent);

    QString portId() const { return m_port.id; }
    PortModel port() const { return m_port; }
    void setPort(const PortModel &port);
    void updatePosition(const QSizeF &nodeSize);
    NodeGraphicsItem *ownerNode() const;
    QPointF connectionAnchor() const;

    void setCompatibilityHighlight(bool compatible, bool incompatibleDimmed);
    void clearCompatibilityHighlight();
    void setConnected(bool connected);

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;

private:
    void refreshToolTip();
    QColor effectiveColor() const;

    PortModel m_port;
    QColor m_baseColor;
    bool m_compatHighlight{false};
    bool m_incompatDimmed{false};
    bool m_hovered{false};
    bool m_connected{false};
};

#endif // PORTGRAPHICSITEM_H
