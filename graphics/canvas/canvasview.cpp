#include "canvasview.h"

#include "canvasscene.h"
#include "core/registry/nodefactory.h"

#include <QCursor>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QScrollBar>
#include <QWheelEvent>

static const char *kNodeTypeMime = "application/x-selt-node-type";

CanvasView::CanvasView(CanvasScene *scene, QWidget *parent)
    : QGraphicsView(scene, parent)
{
    setRenderHint(QPainter::Antialiasing, true);
    setDragMode(QGraphicsView::RubberBandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    setAcceptDrops(true);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
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
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
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

void CanvasView::drawForeground(QPainter *painter, const QRectF &rect)
{
    Q_UNUSED(painter);
    Q_UNUSED(rect);
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
