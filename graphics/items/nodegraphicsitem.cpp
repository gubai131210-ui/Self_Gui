#include "nodegraphicsitem.h"

#include "portgraphicsitem.h"
#include "core/model/document.h"
#include "graphics/canvas/canvasscene.h"
#include "ui/theme/iconprovider.h"
#include "ui/theme/uistyle.h"
#include "vision/model/modulevisualstyle.h"
#include "vision/model/portexposure.h"
#include "vision/model/subflowsupport.h"
#include "vision/registry/visionnodeids.h"
#include "vision/registry/visionnoderegistry.h"

#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QIcon>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QVector>

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
    refreshToolTip();
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
    refreshToolTip();
    prepareGeometryChange();
    update();
}

void NodeGraphicsItem::setRunStatus(NodeRunVisualStatus status)
{
    if (m_runStatus == status)
        return;
    m_runStatus = status;
    refreshToolTip();
    update();
}

void NodeGraphicsItem::setRunSummary(const QString &summary, qint64 elapsedMs)
{
    m_runSummary = summary;
    m_elapsedMs = elapsedMs;
    refreshToolTip();
    update();
}

void NodeGraphicsItem::clearRunSummary()
{
    m_runSummary.clear();
    m_elapsedMs = -1;
    refreshToolTip();
    update();
}

PortGraphicsItem *NodeGraphicsItem::portItem(const QString &portId) const
{
    return m_ports.value(portId, nullptr);
}

QPointF NodeGraphicsItem::portScenePos(const QString &portId) const
{
    if (auto *port = portItem(portId))
        return port->connectionAnchor();
    // Hidden but still referenced (should be rare after sync): approximate edge.
    for (const PortModel &port : m_model.ports) {
        if (port.id != portId)
            continue;
        return mapToScene(QPointF(port.relativeX * m_model.size.width(),
                                  port.relativeY * m_model.size.height()));
    }
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

QRectF NodeGraphicsItem::foldButtonRect() const
{
    return QRectF(m_model.size.width() - 20, 5, 14, 14);
}

void NodeGraphicsItem::refreshToolTip()
{
    const ModuleDescriptor descriptor = VisionNodeRegistry::descriptor(m_model.type);
    QString tip = Selt::helpTextOrDescription(descriptor.helpText, descriptor.description);
    if (tip.isEmpty())
        tip = m_model.text;
    if (!m_runSummary.isEmpty())
        tip += QStringLiteral("\n") + m_runSummary;
    if (m_elapsedMs >= 0)
        tip += QStringLiteral("\n耗时 %1 ms").arg(m_elapsedMs);
    if (m_runStatus == NodeRunVisualStatus::Failed && !m_runSummary.isEmpty())
        tip += QStringLiteral("\n(详见运行监视器)");
    setToolTip(tip);
}

void NodeGraphicsItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(widget);
    painter->setRenderHint(QPainter::Antialiasing, true);

    QPen pen(m_model.style.borderColor, m_model.style.borderWidth);
    switch (m_runStatus) {
    case NodeRunVisualStatus::Running:
        pen = QPen(Selt::UiStyle::runningBlue(), m_model.style.borderWidth + 1.0);
        break;
    case NodeRunVisualStatus::Success:
        pen = QPen(Selt::UiStyle::successGreen(), m_model.style.borderWidth + 1.0);
        break;
    case NodeRunVisualStatus::Failed:
        pen = QPen(Selt::UiStyle::failRed(), m_model.style.borderWidth + 1.5);
        break;
    case NodeRunVisualStatus::Disabled:
        pen = QPen(QColor(140, 140, 140), m_model.style.borderWidth, Qt::DotLine);
        break;
    case NodeRunVisualStatus::Warning:
        pen = QPen(QColor(243, 156, 18), m_model.style.borderWidth + 1.2);
        break;
    case NodeRunVisualStatus::Idle:
    default:
        break;
    }
    if (option->state & QStyle::State_Selected)
        pen = QPen(Selt::UiStyle::accentOrange(), m_model.style.borderWidth + 1.2);

    painter->setPen(pen);
    painter->setBrush(QBrush(m_model.style.fillColor));
    painter->drawPath(shapePath());

    const ModuleDescriptor descriptor = VisionNodeRegistry::descriptor(m_model.type);
    const QColor accent = descriptor.accentColor.isValid()
        ? descriptor.accentColor
        : Selt::accentColorForCategory(descriptor.category);
    const QRectF headerRect(0, 0, m_model.size.width(), 27);
    painter->save();
    painter->setPen(Qt::NoPen);
    painter->setBrush(accent);
    painter->drawRoundedRect(headerRect, m_model.style.cornerRadius, m_model.style.cornerRadius);
    painter->drawRect(QRectF(0, 12, m_model.size.width(), 15));
    painter->restore();

    const QIcon moduleIcon = Selt::IconProvider::visionIcon(
        descriptor.iconKey.isEmpty() ? descriptor.category : descriptor.iconKey, 18);
    if (!moduleIcon.isNull())
        moduleIcon.paint(painter, QRectF(6, 5, 18, 18).toRect());

    QFont font = painter->font();
    font.setPointSize(m_model.style.fontSize);
    font.setBold(true);
    painter->setFont(font);
    painter->setPen(Qt::white);
    painter->drawText(QRectF(30, 4, m_model.size.width() - 72, 20),
                      Qt::AlignVCenter | Qt::TextSingleLine,
                      painter->fontMetrics().elidedText(m_model.text, Qt::ElideRight,
                                                        qMax(1, int(m_model.size.width() - 76))));

    // Fold chevron
    const QRectF foldRect = foldButtonRect();
    painter->setPen(QPen(Qt::white, 1.5));
    const QPointF c = foldRect.center();
    if (m_model.collapsed) {
        painter->drawLine(c + QPointF(-3, -1), c + QPointF(0, 2));
        painter->drawLine(c + QPointF(0, 2), c + QPointF(3, -1));
    } else {
        painter->drawLine(c + QPointF(-3, 1), c + QPointF(0, -2));
        painter->drawLine(c + QPointF(0, -2), c + QPointF(3, 1));
    }

    if (!m_model.collapsed) {
        font.setBold(false);
        font.setPointSize(qMax(8, m_model.style.fontSize - 2));
        painter->setFont(font);
        painter->setPen(m_model.style.textColor);
        // 端口名称贴在端口旁，便于区分多输出；仅绘制画布暴露端口。
        const QFontMetrics fm(font);
        for (auto it = m_ports.cbegin(); it != m_ports.cend(); ++it) {
            const PortModel &port = it.value()->port();
            const QString label = port.name.isEmpty() ? port.id : port.name;
            if (label.isEmpty())
                continue;
            const qreal y = port.relativeY * m_model.size.height() - fm.height() / 2.0;
            if (port.direction == PortDirection::Input
                || (port.direction == PortDirection::Both && port.relativeX < 0.5)) {
                painter->drawText(QRectF(14, y, m_model.size.width() * 0.55, fm.height()),
                                  Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                                  fm.elidedText(label, Qt::ElideRight,
                                                int(m_model.size.width() * 0.55)));
            } else {
                painter->drawText(QRectF(m_model.size.width() * 0.35, y,
                                         m_model.size.width() * 0.55 - 8, fm.height()),
                                  Qt::AlignRight | Qt::AlignVCenter | Qt::TextSingleLine,
                                  fm.elidedText(label, Qt::ElideRight,
                                                int(m_model.size.width() * 0.55 - 8)));
            }
        }
        if (!m_runSummary.isEmpty() || m_elapsedMs >= 0) {
            QString footer = m_runSummary;
            if (footer.isEmpty() && m_elapsedMs >= 0)
                footer = QStringLiteral("%1 ms").arg(m_elapsedMs);
            painter->drawText(QRectF(8, m_model.size.height() - 18,
                                     m_model.size.width() - 16, 14),
                              Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                              fm.elidedText(footer, Qt::ElideRight,
                                            int(m_model.size.width() - 16)));
        }
    }

    if (m_runStatus != NodeRunVisualStatus::Idle) {
        QColor badge(120, 120, 120);
        QString badgeText = QStringLiteral("Idle");
        switch (m_runStatus) {
        case NodeRunVisualStatus::Running:
            badge = Selt::UiStyle::runningBlue();
            badgeText = QStringLiteral("Run");
            break;
        case NodeRunVisualStatus::Success:
            badge = Selt::UiStyle::successGreen();
            badgeText = QStringLiteral("OK");
            break;
        case NodeRunVisualStatus::Failed:
            badge = Selt::UiStyle::failRed();
            badgeText = QStringLiteral("ERR");
            break;
        case NodeRunVisualStatus::Disabled:
            badge = QColor(140, 140, 140);
            badgeText = QStringLiteral("OFF");
            break;
        case NodeRunVisualStatus::Warning:
            badge = QColor(243, 156, 18);
            badgeText = QStringLiteral("WARN");
            break;
        default:
            break;
        }
        const QRectF badgeRect(m_model.size.width() - 54, 4, 28, 14);
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

    if (m_model.breakpoint) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(231, 76, 60));
        painter->drawEllipse(QPointF(12, m_model.size.height() - 12), 5, 5);
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
    if (event->button() == Qt::LeftButton && foldButtonRect().contains(event->pos())) {
        m_model.collapsed = !m_model.collapsed;
        emit collapseToggled(m_model.id, m_model.collapsed);
        update();
        event->accept();
        return;
    }
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

void NodeGraphicsItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    // 仅左键双击打开属性；右键双击会与右键菜单叠加，易导致卡退。
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    emit doubleClicked(m_model.id);
    event->accept();
}

void NodeGraphicsItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    refreshToolTip();
    QGraphicsObject::hoverEnterEvent(event);
}

void NodeGraphicsItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    if (foldButtonRect().contains(event->pos()))
        setCursor(Qt::PointingHandCursor);
    else
        setCursor(Qt::ArrowCursor);
    QGraphicsObject::hoverMoveEvent(event);
}

void NodeGraphicsItem::rebuildPorts()
{
    for (auto *port : m_ports)
        delete port;
    m_ports.clear();

    QVector<ConnectionModel> connections;
    QHash<QString, bool> connected;
    if (auto *cs = canvasScene()) {
        if (Document *doc = cs->document()) {
            connections = doc->connections();
            for (const ConnectionModel &conn : connections) {
                if (conn.sourceNodeId == m_model.id)
                    connected.insert(conn.sourcePortId, true);
                if (conn.targetNodeId == m_model.id)
                    connected.insert(conn.targetPortId, true);
            }
        }
    }

    ModuleDescriptor descriptor = VisionNodeRegistry::descriptor(m_model.type);
    if (m_model.type == VisionNodeIds::subflow()) {
        descriptor.ports = Selt::modulePortsFromSubflowInterface(
            Selt::subflowInterfaceFromParameters(m_model.parameters));
        descriptor.hasExposureHints = true;
    }
    NodeModel layoutNode = m_model;
    if (!descriptor.typeId.isEmpty())
        Selt::layoutExposedPorts(descriptor, layoutNode, connections);

    const QStringList exposed = descriptor.typeId.isEmpty()
        ? [&] {
              QStringList ids;
              for (const PortModel &p : m_model.ports)
                  ids.append(p.id);
              return ids;
          }()
        : Selt::resolveExposedPortIds(m_model, descriptor, connections);

    for (const PortModel &port : layoutNode.ports) {
        if (!exposed.contains(port.id))
            continue;
        auto *item = new PortGraphicsItem(port, this);
        item->setConnected(connected.value(port.id, false));
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
        path.addRoundedRect(rect, m_model.style.cornerRadius, m_model.style.cornerRadius);
    }
    return path;
}

CanvasScene *NodeGraphicsItem::canvasScene() const
{
    return qobject_cast<CanvasScene *>(scene());
}
