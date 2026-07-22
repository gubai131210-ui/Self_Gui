#ifndef CONNECTIONGRAPHICSITEM_H
#define CONNECTIONGRAPHICSITEM_H

#include "core/model/connectionmodel.h"

#include <QGraphicsPathItem>

class CanvasScene;

class ConnectionGraphicsItem : public QGraphicsPathItem
{
public:
    enum { Type = UserType + 3 };
    int type() const override { return Type; }

    explicit ConnectionGraphicsItem(const ConnectionModel &model, QGraphicsItem *parent = nullptr);

    QString connectionId() const { return m_model.id; }
    ConnectionModel model() const { return m_model; }
    void setModel(const ConnectionModel &model);
    void updatePath();
    void setRelatedHighlight(bool related);

    QPainterPath shape() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    CanvasScene *canvasScene() const;
    QPainterPath buildPath(const QPointF &start, const QPointF &end) const;

    ConnectionModel m_model;
    bool m_relatedHighlight{false};
};

#endif // CONNECTIONGRAPHICSITEM_H
