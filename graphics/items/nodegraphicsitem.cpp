#include "nodegraphicsitem.h"

#include "portgraphicsitem.h"
#include "graphics/canvas/canvasscene.h"

#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

NodeGraphicsItem::NodeGraphicsItem(const NodeModel &model, QGraphicsItem *parent)
    : QGraphicsObject(parent)
    , m_model(model)
{
    setFlags(ItemIsSelectable | ItemIsMovable | ItemSendsGeometryChanges);
    setAcceptHoverEvents(true);
    setPos(m_model.position);
    setZValue(m_model.zValue);
    setFlag(ItemIsMovable, !m_model.locked);
    rebuildPorts();
}

void NodeGraphicsItem::setModel(const NodeModel &model)
{
    const bool posChanged = m_model.position != model.position;
    m_model = model;
    if (posChanged)
        setPos(m_model.position);
    setZValue(m_model.zValue);
    setFlag(ItemIsMovable, !m_model.locked);
    rebuildPorts();
    prepareGeometryChange();
    update();
}

void NodeGraphicsItem::setRunStatus(NodeRunVisualStatus status)
{
    if (m_runStatus == status)
        return;
    m_runStatus = status;
    update();
}

PortGraphicsItem *NodeGraphicsItem::portItem(const QString &portId) const
{
    return m_ports.value(portId, nullptr);
}

QPointF NodeGraphicsItem::portScenePos(const QString &portId) const
{
    if (auto *port = portItem(portId))
        return port->scenePos();
    return sceneBoundingRect().center();
}

QRectF NodeGraphicsItem::boundingRect() const
{
    const qreal margin = 4.0 + m_model.style.borderWidth;
    return QRectF(-margin, -margin,
                  m_model.size.width() + margin * 2,
                  m_model.size.height() + margin * 2);
}

QPainterPath NodeGraphicsItem::shape() const
{
    return shapePath();
}

void NodeGraphicsItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(widget);
    painter->setRenderHint(QPainter::Antialiasing, true);

    QPen pen(m_model.style.borderColor, m_model.style.borderWidth);
    switch (m_runStatus) {
    case NodeRunVisualStatus::Running:
        pen = QPen(QColor(30, 144, 255), m_model.style.borderWidth + 1.0);
        break;
    case NodeRunVisualStatus::Success:
        pen = QPen(QColor(46, 180, 80), m_model.style.borderWidth + 1.0);
        break;
    case NodeRunVisualStatus::Failed:
        pen = QPen(QColor(220, 50, 50), m_model.style.borderWidth + 1.5);
        break;
    case NodeRunVisualStatus::Disabled:
        pen = QPen(QColor(140, 140, 140), m_model.style.borderWidth, Qt::DotLine);
        break;
    case NodeRunVisualStatus::Idle:
    default:
        break;
    }
    if (option->state & QStyle::State_Selected)
        pen = QPen(QColor(255, 140, 0), m_model.style.borderWidth + 1.0, Qt::DashLine);

    painter->setPen(pen);
    painter->setBrush(QBrush(m_model.style.fillColor));
    painter->drawPath(shapePath());

    QFont font = painter->font();
    font.setPointSize(m_model.style.fontSize);
    painter->setFont(font);
    painter->setPen(m_model.style.textColor);
    painter->drawText(QRectF(8, 8, m_model.size.width() - 16, m_model.size.height() - 16),
                      Qt::AlignCenter | Qt::TextWordWrap,
                      m_model.text);

    if (m_runStatus != NodeRunVisualStatus::Idle) {
        QColor badge(120, 120, 120);
        QString badgeText = QStringLiteral("Idle");
        switch (m_runStatus) {
        case NodeRunVisualStatus::Running:
            badge = QColor(30, 144, 255);
            badgeText = QStringLiteral("Run");
            break;
        case NodeRunVisualStatus::Success:
            badge = QColor(46, 180, 80);
            badgeText = QStringLiteral("OK");
            break;
        case NodeRunVisualStatus::Failed:
            badge = QColor(220, 50, 50);
            badgeText = QStringLiteral("ERR");
            break;
        case NodeRunVisualStatus::Disabled:
            badge = QColor(140, 140, 140);
            badgeText = QStringLiteral("OFF");
            break;
        default:
            break;
        }
        const QRectF badgeRect(m_model.size.width() - 34, 4, 30, 14);
        painter->setPen(Qt::NoPen);
        painter->setBrush(badge);
        painter->drawRoundedRect(badgeRect, 3, 3);
        QFont badgeFont = font;
        badgeFont.setPointSize(7);
        badgeFont.setBold(true);
        painter->setFont(badgeFont);
        painter->setPen(Qt::white);
        painter->drawText(badgeRect, Qt::AlignCenter, badgeText);
    }
}

QVariant NodeGraphicsItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionChange && scene()) {
        QPointF newPos = value.toPointF();
        if (auto *cs = canvasScene()) {
            if (cs->document() && cs->document()->settings().snapToGrid) {
                const int grid = qMax(1, cs->document()->settings().gridSize);
                newPos.setX(qRound(newPos.x() / grid) * grid);
                newPos.setY(qRound(newPos.y() / grid) * grid);
            }
        }
        return newPos;
    }

    if (change == ItemPositionHasChanged && scene()) {
        if (auto *cs = canvasScene())
            cs->updateConnectionsForNode(m_model.id);
    }

    return QGraphicsObject::itemChange(change, value);
}

void NodeGraphicsItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    m_pressScenePos = pos();
    m_moving = true;
    QGraphicsObject::mousePressEvent(event);
}

void NodeGraphicsItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsObject::mouseReleaseEvent(event);
    if (m_moving && pos() != m_pressScenePos) {
        emit moveFinished(m_model.id, m_pressScenePos, pos());
        if (auto *cs = canvasScene())
            cs->notifyNodeMoved(m_model.id, m_pressScenePos, pos());
    }
    m_moving = false;
}

void NodeGraphicsItem::rebuildPorts()
{
    for (auto *port : m_ports)
        delete port;
    m_ports.clear();

    for (const PortModel &port : m_model.ports) {
        auto *item = new PortGraphicsItem(port, this);
        m_ports.insert(port.id, item);
    }
}

QPainterPath NodeGraphicsItem::shapePath() const
{
    QPainterPath path;
    const QRectF rect(0, 0, m_model.size.width(), m_model.size.height());
    if (m_model.type == QLatin1String("ellipse")) {
        path.addEllipse(rect);
    } else if (m_model.type == QLatin1String("diamond")) {
        path.moveTo(rect.center().x(), rect.top());
        path.lineTo(rect.right(), rect.center().y());
        path.lineTo(rect.center().x(), rect.bottom());
        path.lineTo(rect.left(), rect.center().y());
        path.closeSubpath();
    } else if (m_model.type == QLatin1String("roundedRectangle") || m_model.type == QLatin1String("text")) {
        path.addRoundedRect(rect, m_model.style.cornerRadius, m_model.style.cornerRadius);
    } else {
        path.addRect(rect);
    }
    return path;
}

CanvasScene *NodeGraphicsItem::canvasScene() const
{
    return qobject_cast<CanvasScene *>(scene());
}
