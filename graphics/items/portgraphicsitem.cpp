#include "portgraphicsitem.h"

#include "nodegraphicsitem.h"
#include "graphics/canvas/canvasscene.h"
#include "ui/theme/uistyle.h"
#include "vision/data/datatype.h"
#include "vision/model/modulevisualstyle.h"

#include <QCursor>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>

namespace {

QColor colorForPort(const PortModel &port)
{
    if (!port.dataType.isEmpty()) {
        bool ok = false;
        const Selt::DataTypeId type = Selt::dataTypeIdFromString(port.dataType, &ok);
        if (ok)
            return Selt::colorForDataType(type);
    }

    const QString semantic = (port.id + QLatin1Char(' ') + port.name).toLower();
    if (semantic.contains(QStringLiteral("overlay")) || semantic.contains(QStringLiteral("叠加")))
        return Selt::colorForDataType(Selt::DataTypeId::Overlay);
    if (semantic.contains(QStringLiteral("measure")) || semantic.contains(QStringLiteral("测量"))
        || semantic.contains(QStringLiteral("score")) || semantic.contains(QStringLiteral("得分")))
        return Selt::colorForDataType(Selt::DataTypeId::Measurement);
    if (semantic.contains(QStringLiteral("roi")) || semantic.contains(QStringLiteral("区域")))
        return Selt::colorForDataType(Selt::DataTypeId::Roi);
    if (semantic.contains(QStringLiteral("image")) || semantic.contains(QStringLiteral("图像"))
        || semantic.contains(QStringLiteral("输入")) || semantic.contains(QStringLiteral("输出")))
        return Selt::colorForDataType(Selt::DataTypeId::Image);
    return Selt::colorForDataType(Selt::DataTypeId::None);
}

QString shortPortLabel(const PortModel &port)
{
    const QString name = port.name.isEmpty() ? port.id : port.name;
    if (name.size() <= 4)
        return name;
    return name.left(3) + QLatin1Char('.');
}

} // namespace

PortGraphicsItem::PortGraphicsItem(const PortModel &port, NodeGraphicsItem *parent)
    : QGraphicsObject(parent)
    , m_port(port)
{
    setAcceptHoverEvents(true);
    m_baseColor = colorForPort(m_port);
    setZValue(10);
    setFlag(QGraphicsItem::ItemIsSelectable, false);
    setCursor(Qt::CrossCursor);
    refreshToolTip();
    updatePosition(parent->model().size);
}

void PortGraphicsItem::setPort(const PortModel &port)
{
    m_port = port;
    m_baseColor = colorForPort(m_port);
    refreshToolTip();
    if (auto *node = ownerNode())
        updatePosition(node->model().size);
    update();
}

void PortGraphicsItem::updatePosition(const QSizeF &nodeSize)
{
    setPos(m_port.relativeX * nodeSize.width(), m_port.relativeY * nodeSize.height());
}

NodeGraphicsItem *PortGraphicsItem::ownerNode() const
{
    return qgraphicsitem_cast<NodeGraphicsItem *>(parentItem());
}

QPointF PortGraphicsItem::connectionAnchor() const
{
    return scenePos();
}

void PortGraphicsItem::clearCompatibilityHighlight()
{
    m_compatHighlight = false;
    m_incompatDimmed = false;
    setOpacity(1.0);
    update();
}

void PortGraphicsItem::setCompatibilityHighlight(bool compatible, bool incompatibleDimmed)
{
    m_compatHighlight = compatible;
    m_incompatDimmed = incompatibleDimmed && !compatible;
    setOpacity(m_incompatDimmed ? 0.35 : 1.0);
    update();
}

void PortGraphicsItem::setConnected(bool connected)
{
    if (m_connected == connected)
        return;
    m_connected = connected;
    update();
}

void PortGraphicsItem::refreshToolTip()
{
    const QString name = m_port.name.isEmpty() ? m_port.id : m_port.name;
    QString typeText;
    if (!m_port.dataType.isEmpty()) {
        bool ok = false;
        const Selt::DataTypeId type = Selt::dataTypeIdFromString(m_port.dataType, &ok);
        if (ok)
            typeText = Selt::dataTypeIdDisplayName(type);
        else
            typeText = m_port.dataType;
    }
    const QString dirText = (m_port.direction == PortDirection::Input)
        ? QStringLiteral("输入")
        : (m_port.direction == PortDirection::Output ? QStringLiteral("输出")
                                                     : QStringLiteral("双向"));
    QString tip;
    if (typeText.isEmpty())
        tip = QStringLiteral("%1（%2）").arg(name, dirText);
    else
        tip = QStringLiteral("%1（%2 · %3）").arg(name, dirText, typeText);
    if (m_connected)
        tip += QStringLiteral("\n状态：已连接");
    setToolTip(tip);
}

QColor PortGraphicsItem::effectiveColor() const
{
    if (m_compatHighlight)
        return Selt::UiStyle::accentOrange();
    if (m_hovered)
        return m_baseColor.lighter(140);
    return m_baseColor;
}

QRectF PortGraphicsItem::boundingRect() const
{
    return QRectF(-CapsuleHalfWidth - 1, -CapsuleHalfHeight - 1,
                  CapsuleHalfWidth * 2 + 2, CapsuleHalfHeight * 2 + 2);
}

QPainterPath PortGraphicsItem::shape() const
{
    QPainterPath path;
    path.addRoundedRect(QRectF(-CapsuleHalfWidth, -CapsuleHalfHeight,
                               CapsuleHalfWidth * 2, CapsuleHalfHeight * 2),
                        4, 4);
    return path;
}

void PortGraphicsItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);
    painter->setRenderHint(QPainter::Antialiasing, true);
    const QColor fill = effectiveColor();
    painter->setBrush(fill);
    painter->setPen(QPen(fill.darker(135), m_connected ? 2.0 : 1.2));
    painter->drawRoundedRect(QRectF(-CapsuleHalfWidth, -CapsuleHalfHeight,
                                    CapsuleHalfWidth * 2, CapsuleHalfHeight * 2),
                             4, 4);

    QFont font = painter->font();
    font.setPointSize(7);
    font.setBold(true);
    painter->setFont(font);
    painter->setPen(Qt::white);
    painter->drawText(boundingRect().adjusted(1, 0, -1, 0), Qt::AlignCenter, shortPortLabel(m_port));
}

void PortGraphicsItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    m_hovered = true;
    update();
    QGraphicsObject::hoverEnterEvent(event);
}

void PortGraphicsItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    m_hovered = false;
    update();
    QGraphicsObject::hoverLeaveEvent(event);
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
    QGraphicsObject::mousePressEvent(event);
}
