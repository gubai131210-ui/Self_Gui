#include "imagepreviewwidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QWheelEvent>
#include <QVBoxLayout>

ImageCanvasWidget::ImageCanvasWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(240, 180);
    setStyleSheet(QStringLiteral("background:#1e1e1e;"));
    setMouseTracking(true);
}

void ImageCanvasWidget::setImage(const QImage &image)
{
    m_image = image;
    update();
}

void ImageCanvasWidget::setOverlays(const OverlayList &overlays)
{
    m_overlays = overlays;
    update();
}

void ImageCanvasWidget::setRoi(const RoiRect &roi)
{
    m_roi = roi;
    update();
}

void ImageCanvasWidget::setRoiEditEnabled(bool enabled)
{
    m_roiEditEnabled = enabled;
}

void ImageCanvasWidget::setScale(qreal scale)
{
    m_scale = scale;
    emit viewChanged();
    update();
}

void ImageCanvasWidget::setOffset(const QPointF &offset)
{
    m_offset = offset;
    emit viewChanged();
    update();
}

void ImageCanvasWidget::fitToView()
{
    if (m_image.isNull() || width() < 10 || height() < 10)
        return;
    const qreal sx = static_cast<qreal>(width()) / m_image.width();
    const qreal sy = static_cast<qreal>(height()) / m_image.height();
    m_scale = qMax(0.05, qMin(sx, sy) * 0.95);
    m_offset = QPointF(0, 0);
    emit viewChanged();
    update();
}

void ImageCanvasWidget::resetView()
{
    m_scale = 1.0;
    m_offset = QPointF(0, 0);
    emit viewChanged();
    update();
}

void ImageCanvasWidget::zoomBy(qreal factor)
{
    m_scale = qBound(0.05, m_scale * factor, 8.0);
    emit viewChanged();
    update();
}

QRectF ImageCanvasWidget::imageDisplayRect() const
{
    if (m_image.isNull())
        return {};
    const qreal w = m_image.width() * m_scale;
    const qreal h = m_image.height() * m_scale;
    return QRectF((width() - w) * 0.5 + m_offset.x(),
                  (height() - h) * 0.5 + m_offset.y(),
                  w, h);
}

QPointF ImageCanvasWidget::widgetToImage(const QPoint &pos) const
{
    const QRectF disp = imageDisplayRect();
    if (disp.isEmpty())
        return {};
    return QPointF((pos.x() - disp.x()) / m_scale, (pos.y() - disp.y()) / m_scale);
}

QPointF ImageCanvasWidget::imageToWidget(const QPointF &pos) const
{
    const QRectF disp = imageDisplayRect();
    return QPointF(disp.x() + pos.x() * m_scale, disp.y() + pos.y() * m_scale);
}

void ImageCanvasWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor(30, 30, 30));
    if (m_image.isNull()) {
        painter.setPen(QColor(160, 160, 160));
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("暂无图像\nCtrl+拖拽可绘制ROI"));
        return;
    }

    const QRectF disp = imageDisplayRect();
    painter.drawImage(disp, m_image);
    painter.setRenderHint(QPainter::Antialiasing, true);

    for (const OverlayItem &item : m_overlays) {
        painter.setPen(QPen(item.color, item.penWidth));
        painter.setBrush(Qt::NoBrush);
        if (item.type == OverlayItemType::Rectangle) {
            painter.drawRect(QRectF(imageToWidget(item.rect.topLeft()),
                                    imageToWidget(item.rect.bottomRight())));
        } else if (item.type == OverlayItemType::Text) {
            painter.drawText(imageToWidget(item.rect.topLeft()) + QPointF(0, -4), item.text);
        } else if (item.type == OverlayItemType::Line && item.points.size() >= 2) {
            painter.drawLine(imageToWidget(item.points.at(0)), imageToWidget(item.points.at(1)));
        } else if (item.type == OverlayItemType::Contour && item.points.size() >= 2) {
            QPolygonF poly;
            for (const QPointF &p : item.points)
                poly << imageToWidget(p);
            painter.drawPolyline(poly);
        }
    }

    if (m_roi.enabled && m_roi.rect.isValid()) {
        painter.setPen(QPen(QColor(0, 180, 255), 2, Qt::DashLine));
        painter.drawRect(QRectF(imageToWidget(m_roi.rect.topLeft()),
                                imageToWidget(m_roi.rect.bottomRight())));
    }
}

void ImageCanvasWidget::wheelEvent(QWheelEvent *event)
{
    if (m_image.isNull())
        return;
    zoomBy(event->angleDelta().y() > 0 ? 1.25 : 0.8);
    event->accept();
}

void ImageCanvasWidget::mousePressEvent(QMouseEvent *event)
{
    if (m_image.isNull())
        return;

    if (m_roiEditEnabled && event->button() == Qt::LeftButton && (event->modifiers() & Qt::ControlModifier)) {
        m_drawingRoi = true;
        m_roiStart = widgetToImage(event->pos());
        m_roi.enabled = true;
        m_roi.rect = QRectF(m_roiStart, QSizeF(1, 1));
        update();
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton || event->button() == Qt::MiddleButton) {
        m_panning = true;
        m_lastMousePos = event->pos();
        event->accept();
    }
}

void ImageCanvasWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_drawingRoi) {
        m_roi.rect = QRectF(m_roiStart, widgetToImage(event->pos())).normalized();
        update();
        event->accept();
        return;
    }
    if (m_panning) {
        m_offset += (event->pos() - m_lastMousePos);
        m_lastMousePos = event->pos();
        emit viewChanged();
        update();
        event->accept();
    }
}

void ImageCanvasWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_drawingRoi && event->button() == Qt::LeftButton) {
        m_drawingRoi = false;
        if (m_roi.rect.width() < 2 || m_roi.rect.height() < 2)
            m_roi = RoiRect{};
        else
            m_roi.enabled = true;
        emit roiChanged(m_roi);
        update();
        event->accept();
        return;
    }
    if (m_panning) {
        m_panning = false;
        event->accept();
    }
}

ImagePreviewWidget::ImagePreviewWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    auto *toolbar = new QHBoxLayout;
    m_titleLabel = new QLabel(QStringLiteral("图像预览"), this);
    m_fitBtn = new QPushButton(QStringLiteral("适应"), this);
    m_resetBtn = new QPushButton(QStringLiteral("1:1"), this);
    m_clearRoiBtn = new QPushButton(QStringLiteral("清除ROI"), this);
    toolbar->addWidget(m_titleLabel, 1);
    toolbar->addWidget(m_fitBtn);
    toolbar->addWidget(m_resetBtn);
    toolbar->addWidget(m_clearRoiBtn);
    layout->addLayout(toolbar);

    m_canvas = new ImageCanvasWidget(this);
    layout->addWidget(m_canvas, 1);

    connect(m_fitBtn, &QPushButton::clicked, m_canvas, &ImageCanvasWidget::fitToView);
    connect(m_resetBtn, &QPushButton::clicked, m_canvas, &ImageCanvasWidget::resetView);
    connect(m_clearRoiBtn, &QPushButton::clicked, this, [this]() {
        m_canvas->setRoi(RoiRect{});
        emit roiChanged(RoiRect{});
    });
    connect(m_canvas, &ImageCanvasWidget::roiChanged, this, &ImagePreviewWidget::roiChanged);
    connect(m_canvas, &ImageCanvasWidget::viewChanged, this, &ImagePreviewWidget::updateTitle);
}

void ImagePreviewWidget::setImage(const QImage &image, const QString &title)
{
    if (!title.isEmpty())
        m_title = title;
    m_canvas->setImage(image);
    if (!image.isNull())
        m_canvas->fitToView();
    updateTitle();
}

void ImagePreviewWidget::setOverlays(const OverlayList &overlays)
{
    m_canvas->setOverlays(overlays);
}

void ImagePreviewWidget::setRoi(const RoiRect &roi)
{
    m_canvas->setRoi(roi);
}

void ImagePreviewWidget::clearImage()
{
    m_title = QStringLiteral("图像预览");
    m_canvas->setImage(QImage());
    m_canvas->setOverlays({});
    m_canvas->setRoi(RoiRect{});
    updateTitle();
}

void ImagePreviewWidget::setRoiEditEnabled(bool enabled)
{
    m_canvas->setRoiEditEnabled(enabled);
    m_clearRoiBtn->setEnabled(enabled);
}

RoiRect ImagePreviewWidget::currentRoi() const
{
    return m_canvas->roi();
}

QImage ImagePreviewWidget::currentImage() const
{
    return m_canvas->image();
}

void ImagePreviewWidget::updateTitle()
{
    const QString zoom = m_canvas->image().isNull()
        ? QString()
        : QStringLiteral("  [%1%]").arg(qRound(m_canvas->scale() * 100));
    m_titleLabel->setText((m_title.isEmpty() ? QStringLiteral("图像预览") : m_title) + zoom);
}
