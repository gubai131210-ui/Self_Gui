#include "canvasview.h"

#include "canvasscene.h"
#include "core/registry/nodefactory.h"
#include "graphics/items/nodegraphicsitem.h"

#include <QCursor>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>

static const char *kNodeTypeMime = "application/x-selt-node-type";

CanvasView::CanvasView(CanvasScene *scene, QWidget *parent)
    : QGraphicsView(scene, parent)
{
    setRenderHint(QPainter::Antialiasing, true);
    setDragMode(QGraphicsView::RubberBandDrag);
    // SmartViewportUpdate leaves rubber-band edge remnants as black axis lines.
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setAcceptDrops(true);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setStyleSheet(QStringLiteral(
        "QGraphicsView {"
        "  background: transparent;"
        "}"
        "QRubberBand {"
        "  border: 1px solid #4a90d9;"
        "  background-color: rgba(74, 144, 217, 45);"
        "}"));
}

void CanvasView::zoomIn()
{
    setZoom(m_zoom * 1.15);
}

void CanvasView::zoomOut()
{
    setZoom(m_zoom / 1.15);
}

void CanvasView::resetZoom()
{
    setZoom(1.0);
}

void CanvasView::fitAll()
{
    if (!scene() || scene()->items().isEmpty()) {
        resetZoom();
        return;
    }
    fitInView(scene()->itemsBoundingRect().adjusted(-40, -40, 40, 40), Qt::KeepAspectRatio);
    m_zoom = transform().m11();
    emit zoomChanged(m_zoom);
}

qreal CanvasView::zoomFactor() const
{
    return m_zoom;
}

void CanvasView::setZoomFactor(qreal factor)
{
    setZoom(factor);
}

QPointF CanvasView::viewCenter() const
{
    return mapToScene(viewport()->rect().center());
}

void CanvasView::setViewCenter(const QPointF &center)
{
    centerOn(center);
}

int CanvasView::horizontalScroll() const
{
    return horizontalScrollBar()->value();
}

int CanvasView::verticalScroll() const
{
    return verticalScrollBar()->value();
}

void CanvasView::setScrollOffsets(int h, int v)
{
    horizontalScrollBar()->setValue(h);
    verticalScrollBar()->setValue(v);
}

void CanvasView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        if (event->angleDelta().y() > 0)
            zoomIn();
        else
            zoomOut();
        event->accept();
        return;
    }

    // Default: zoom with wheel for canvas feel
    if (event->angleDelta().y() > 0)
        zoomIn();
    else
        zoomOut();
    event->accept();
}

void CanvasView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton
        || (event->button() == Qt::LeftButton && m_spaceDown)) {
        m_panning = true;
        m_lastPanPos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

void CanvasView::mouseMoveEvent(QMouseEvent *event)
{
    emit mouseScenePosChanged(mapToScene(event->pos()));

    if (m_panning) {
        const QPoint delta = event->pos() - m_lastPanPos;
        m_lastPanPos = event->pos();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void CanvasView::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_panning && (event->button() == Qt::MiddleButton || event->button() == Qt::LeftButton)) {
        m_panning = false;
        setCursor(m_spaceDown ? Qt::OpenHandCursor : Qt::ArrowCursor);
        event->accept();
        viewport()->update();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
    // Flush any rubber-band paint leftovers after selection drag ends.
    viewport()->update();
}

void CanvasView::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_spaceDown = true;
        setCursor(Qt::OpenHandCursor);
        event->accept();
        return;
    }
    QGraphicsView::keyPressEvent(event);
}

void CanvasView::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_spaceDown = false;
        if (!m_panning)
            setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }
    QGraphicsView::keyReleaseEvent(event);
}

void CanvasView::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat(QString::fromLatin1(kNodeTypeMime)))
        event->acceptProposedAction();
}

void CanvasView::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasFormat(QString::fromLatin1(kNodeTypeMime)))
        event->acceptProposedAction();
}

void CanvasView::dropEvent(QDropEvent *event)
{
    if (!event->mimeData()->hasFormat(QString::fromLatin1(kNodeTypeMime)))
        return;
    const QString type = QString::fromUtf8(event->mimeData()->data(QString::fromLatin1(kNodeTypeMime)));
    if (auto *cs = canvasScene())
        cs->createNodeAt(type, mapToScene(event->position().toPoint()));
    event->acceptProposedAction();
}

void CanvasView::setMinimapVisible(bool visible)
{
    if (m_minimapVisible == visible)
        return;
    m_minimapVisible = visible;
    viewport()->update();
}

void CanvasView::drawForeground(QPainter *painter, const QRectF &rect)
{
    Q_UNUSED(rect);
    if (!m_minimapVisible || !scene() || scene()->items().isEmpty())
        return;

    const QRectF sceneBounds = scene()->itemsBoundingRect().adjusted(-40, -40, 40, 40);
    if (sceneBounds.isEmpty())
        return;

    constexpr int kPad = 8;
    constexpr int kSize = 140;
    const QRect mapRect(viewport()->width() - kSize - kPad, kPad, kSize, kSize);

    painter->save();
    painter->resetTransform();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(QPen(QColor(255, 140, 0, 160), 1));
    painter->setBrush(QColor(30, 34, 40, 200));
    painter->drawRoundedRect(mapRect, 4, 4);

    const qreal sx = mapRect.width() / sceneBounds.width();
    const qreal sy = mapRect.height() / sceneBounds.height();
    const qreal scale = qMin(sx, sy);
    const QPointF origin(mapRect.left() - sceneBounds.left() * scale,
                         mapRect.top() - sceneBounds.top() * scale);

    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(255, 140, 0, 180));
    for (QGraphicsItem *item : scene()->items()) {
        if (item->type() != NodeGraphicsItem::Type)
            continue;
        const QRectF br = item->sceneBoundingRect();
        QRectF mapped(origin.x() + br.left() * scale,
                      origin.y() + br.top() * scale,
                      qMax(2.0, br.width() * scale),
                      qMax(2.0, br.height() * scale));
        painter->drawRect(mapped);
    }

    const QRectF viewScene = mapToScene(viewport()->rect()).boundingRect();
    QRectF viewMapped(origin.x() + viewScene.left() * scale,
                      origin.y() + viewScene.top() * scale,
                      viewScene.width() * scale,
                      viewScene.height() * scale);
    painter->setBrush(Qt::NoBrush);
    painter->setPen(QPen(QColor(255, 170, 48), 1.5));
    painter->drawRect(viewMapped.intersected(mapRect));
    painter->restore();
}

void CanvasView::setZoom(qreal factor)
{
    factor = qBound(0.15, factor, 5.0);
    const qreal scale = factor / m_zoom;
    this->scale(scale, scale);
    m_zoom = factor;
    emit zoomChanged(m_zoom);
}

CanvasScene *CanvasView::canvasScene() const
{
    return qobject_cast<CanvasScene *>(scene());
}
