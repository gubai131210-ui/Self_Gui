#include "imagepreviewwidget.h"

#include "ui/theme/uistyle.h"

#include <cmath>
#include <QColor>
#include <QComboBox>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSizePolicy>
#include <QStyle>
#include <QWheelEvent>
#include <QtMath>
#include <QVBoxLayout>

namespace {
constexpr qreal kHandleSize = 8.0;
}

ImageCanvasWidget::ImageCanvasWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(200, 160);
    setStyleSheet(QStringLiteral("background:#1e1e1e;"));
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
}

void ImageCanvasWidget::setImage(const QImage &image)
{
    m_image = image;
    clampRoiToImage();
    update();
}

void ImageCanvasWidget::setOverlays(const OverlayList &overlays)
{
    m_overlays = overlays;
    update();
}

void ImageCanvasWidget::setRoi(const RoiRect &roi)
{
    m_roi = Selt::ExtendedRoi::fromLegacyRect(roi);
    clampRoiToImage();
    update();
}

void ImageCanvasWidget::setExtendedRoi(const Selt::ExtendedRoi &roi)
{
    m_roi = roi;
    m_roi.normalize();
    clampRoiToImage();
    update();
}

void ImageCanvasWidget::setRoiEditEnabled(bool enabled)
{
    m_roiEditEnabled = enabled;
}

void ImageCanvasWidget::setRoiTool(RoiTool tool)
{
    m_roiTool = tool;
    m_polygonDrawing = false;
}

void ImageCanvasWidget::setOverlayTypeFilter(const QSet<int> &visibleTypes)
{
    m_overlayTypeFilter = visibleTypes;
    update();
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
    m_autoFit = true;
    emit viewChanged();
    update();
}

void ImageCanvasWidget::resetView()
{
    m_scale = 1.0;
    m_offset = QPointF(0, 0);
    m_autoFit = false;
    emit viewChanged();
    update();
}

void ImageCanvasWidget::zoomBy(qreal factor)
{
    m_scale = qBound(0.05, m_scale * factor, 8.0);
    m_autoFit = false;
    emit viewChanged();
    update();
}

void ImageCanvasWidget::setAutoFitEnabled(bool enabled)
{
    m_autoFit = enabled;
    if (m_autoFit)
        fitToView();
}

void ImageCanvasWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_autoFit && !m_image.isNull())
        fitToView();
}

void ImageCanvasWidget::clearRoi()
{
    m_roi = Selt::ExtendedRoi{};
    m_polygonDrawing = false;
    emitRoiIfChanged();
    update();
}

Selt::RoiShapeType ImageCanvasWidget::shapeForTool(RoiTool tool) const
{
    switch (tool) {
    case RoiTool::RotatedRect:
        return Selt::RoiShapeType::RotatedRect;
    case RoiTool::Circle:
        return Selt::RoiShapeType::Circle;
    case RoiTool::Ellipse:
        return Selt::RoiShapeType::Ellipse;
    case RoiTool::Polygon:
        return Selt::RoiShapeType::Polygon;
    case RoiTool::Rectangle:
    default:
        return Selt::RoiShapeType::Rectangle;
    }
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

void ImageCanvasWidget::clampRoiToImage()
{
    if (m_image.isNull())
        return;
    m_roi.clampToImage(m_image.size());
}

void ImageCanvasWidget::applyBoundingBoxToShape(const QRectF &box)
{
    const QRectF normalized = box.normalized();
    m_roi.center = normalized.center();
    m_roi.width = normalized.width();
    m_roi.height = normalized.height();
    m_roi.rect = normalized;
    if (m_roi.shape == Selt::RoiShapeType::Circle)
        m_roi.radius = qMin(normalized.width(), normalized.height()) * 0.5;
}

ImageCanvasWidget::RoiHandle ImageCanvasWidget::hitTestHandle(const QPoint &pos) const
{
    if (!m_roi.enabled || !m_roi.visible)
        return RoiHandle::None;

    if (m_roi.shape == Selt::RoiShapeType::Polygon) {
        for (int i = 0; i < m_roi.polygon.size(); ++i) {
            const QPointF p = imageToWidget(m_roi.polygon.at(i));
            const QRectF handleRect(p.x() - kHandleSize * 0.5, p.y() - kHandleSize * 0.5,
                                    kHandleSize, kHandleSize);
            if (handleRect.contains(pos)) {
                const_cast<ImageCanvasWidget *>(this)->m_activePolygonIndex = i;
                return RoiHandle::PolygonVertex;
            }
        }
        QPolygonF poly;
        for (const QPointF &p : m_roi.polygon)
            poly << imageToWidget(p);
        if (poly.containsPoint(pos, Qt::OddEvenFill))
            return RoiHandle::Move;
        return RoiHandle::None;
    }

    if (m_roi.shape == Selt::RoiShapeType::Circle) {
        const QPointF center = imageToWidget(m_roi.center);
        const QPointF edge = imageToWidget(m_roi.center + QPointF(m_roi.radius, 0));
        const QRectF radiusHandle(edge.x() - kHandleSize * 0.5, edge.y() - kHandleSize * 0.5,
                                  kHandleSize, kHandleSize);
        if (radiusHandle.contains(pos))
            return RoiHandle::Radius;
        const qreal dist = QLineF(center, pos).length();
        if (dist <= m_roi.radius * m_scale + 4.0)
            return RoiHandle::Move;
        return RoiHandle::None;
    }

    const QRectF box(imageToWidget(m_roi.boundingRect().topLeft()),
                     imageToWidget(m_roi.boundingRect().bottomRight()));
    const QPointF points[] = {
        box.topLeft(), box.topRight(), box.bottomLeft(), box.bottomRight()};
    const RoiHandle handles[] = {
        RoiHandle::TopLeft, RoiHandle::TopRight, RoiHandle::BottomLeft, RoiHandle::BottomRight};
    for (int i = 0; i < 4; ++i) {
        const QRectF handleRect(points[i].x() - kHandleSize * 0.5,
                                points[i].y() - kHandleSize * 0.5,
                                kHandleSize, kHandleSize);
        if (handleRect.contains(pos))
            return handles[i];
    }
    if (box.adjusted(-2, -2, 2, 2).contains(pos))
        return RoiHandle::Move;
    return RoiHandle::None;
}

void ImageCanvasWidget::drawRoiShape(QPainter &painter) const
{
    if (!m_roi.enabled || !m_roi.visible)
        return;
    painter.setPen(QPen(m_roi.locked ? QColor(255, 160, 0) : QColor(0, 180, 255), 2,
                        m_roi.locked ? Qt::SolidLine : Qt::DashLine));
    painter.setBrush(QColor(0, 180, 255, 30));

    switch (m_roi.shape) {
    case Selt::RoiShapeType::Circle: {
        const QPointF c = imageToWidget(m_roi.center);
        const qreal r = m_roi.radius * m_scale;
        painter.drawEllipse(c, r, r);
        break;
    }
    case Selt::RoiShapeType::Ellipse: {
        const QRectF box(imageToWidget(m_roi.boundingRect().topLeft()),
                         imageToWidget(m_roi.boundingRect().bottomRight()));
        painter.drawEllipse(box);
        break;
    }
    case Selt::RoiShapeType::RotatedRect: {
        const QRectF box = m_roi.boundingRect();
        QTransform t;
        t.translate(imageToWidget(m_roi.center).x(), imageToWidget(m_roi.center).y());
        t.rotate(m_roi.angleDeg);
        t.scale(m_scale, m_scale);
        painter.save();
        painter.setTransform(t, true);
        painter.drawRect(QRectF(-box.width() * 0.5, -box.height() * 0.5, box.width(), box.height()));
        painter.restore();
        break;
    }
    case Selt::RoiShapeType::Polygon: {
        QPolygonF poly;
        for (const QPointF &p : m_roi.polygon)
            poly << imageToWidget(p);
        if (m_polygonDrawing && !poly.isEmpty())
            painter.drawPolyline(poly);
        else
            painter.drawPolygon(poly);
        break;
    }
    case Selt::RoiShapeType::Rectangle:
    default:
        painter.drawRect(QRectF(imageToWidget(m_roi.boundingRect().topLeft()),
                                imageToWidget(m_roi.boundingRect().bottomRight())));
        break;
    }
}

void ImageCanvasWidget::drawHandles(QPainter &painter) const
{
    if (!m_roi.enabled || m_roi.locked || !m_roiEditEnabled)
        return;
    painter.setBrush(QColor(0, 180, 255));
    painter.setPen(Qt::NoPen);

    if (m_roi.shape == Selt::RoiShapeType::Polygon) {
        for (const QPointF &ip : m_roi.polygon) {
            const QPointF p = imageToWidget(ip);
            painter.drawRect(QRectF(p.x() - kHandleSize * 0.5, p.y() - kHandleSize * 0.5,
                                    kHandleSize, kHandleSize));
        }
        return;
    }
    if (m_roi.shape == Selt::RoiShapeType::Circle) {
        const QPointF edge = imageToWidget(m_roi.center + QPointF(m_roi.radius, 0));
        painter.drawRect(QRectF(edge.x() - kHandleSize * 0.5, edge.y() - kHandleSize * 0.5,
                                kHandleSize, kHandleSize));
        return;
    }

    const QRectF box(imageToWidget(m_roi.boundingRect().topLeft()),
                     imageToWidget(m_roi.boundingRect().bottomRight()));
    const QPointF points[] = {
        box.topLeft(), box.topRight(), box.bottomLeft(), box.bottomRight()};
    for (const QPointF &p : points) {
        painter.drawRect(QRectF(p.x() - kHandleSize * 0.5, p.y() - kHandleSize * 0.5,
                                kHandleSize, kHandleSize));
    }
}

void ImageCanvasWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    if (m_checkerboard) {
        const int cell = 12;
        for (int y = 0; y < height(); y += cell) {
            for (int x = 0; x < width(); x += cell) {
                const bool dark = ((x / cell) + (y / cell)) % 2 == 0;
                painter.fillRect(x, y, cell, cell, dark ? QColor(40, 40, 40) : QColor(55, 55, 55));
            }
        }
    } else {
        painter.fillRect(rect(), QColor(30, 30, 30));
    }
    if (m_image.isNull()) {
        painter.setPen(QColor(160, 160, 160));
        painter.drawText(rect(), Qt::AlignCenter,
                         QStringLiteral("暂无图像\nCtrl+拖拽绘制ROI\n多边形：Ctrl+点击加点，双击结束"));
        return;
    }

    const QRectF disp = imageDisplayRect();
    painter.drawImage(disp, m_image);
    painter.setRenderHint(QPainter::Antialiasing, true);

    for (const OverlayItem &item : m_overlays) {
        if (!m_overlayTypeFilter.isEmpty()
            && !m_overlayTypeFilter.contains(static_cast<int>(item.type))) {
            continue;
        }
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

    drawRoiShape(painter);
    drawHandles(painter);
}

void ImageCanvasWidget::setCheckerboardBackground(bool enabled)
{
    m_checkerboard = enabled;
    update();
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
        if (m_roi.locked) {
            event->accept();
            return;
        }

        if (m_roiTool == RoiTool::Polygon) {
            const QPointF imagePos = widgetToImage(event->pos());
            if (!m_polygonDrawing) {
                m_roi = Selt::ExtendedRoi{};
                m_roi.shape = Selt::RoiShapeType::Polygon;
                m_roi.enabled = true;
                m_roi.visible = true;
                m_polygonDrawing = true;
            }
            m_roi.polygon.append(imagePos);
            update();
            event->accept();
            return;
        }

        m_drawingRoi = true;
        m_roiStart = widgetToImage(event->pos());
        m_roi = Selt::ExtendedRoi{};
        m_roi.shape = shapeForTool(m_roiTool);
        m_roi.enabled = true;
        m_roi.visible = true;
        m_roi.angleDeg = (m_roiTool == RoiTool::RotatedRect) ? 15.0 : 0.0;
        applyBoundingBoxToShape(QRectF(m_roiStart, QSizeF(1, 1)));
        update();
        event->accept();
        return;
    }

    if (m_roiEditEnabled && event->button() == Qt::LeftButton && !m_roi.locked) {
        const RoiHandle handle = hitTestHandle(event->pos());
        if (handle != RoiHandle::None) {
            m_editingRoi = true;
            m_activeHandle = handle;
            m_lastMousePos = event->pos();
            m_roiAtEditStart = m_roi.boundingRect();
            event->accept();
            return;
        }
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
        applyBoundingBoxToShape(QRectF(m_roiStart, widgetToImage(event->pos())));
        clampRoiToImage();
        update();
        event->accept();
        return;
    }

    if (m_editingRoi) {
        const QPointF cur = widgetToImage(event->pos());
        const QPointF prev = widgetToImage(m_lastMousePos);
        const QPointF delta = cur - prev;

        if (m_activeHandle == RoiHandle::PolygonVertex && m_activePolygonIndex >= 0
            && m_activePolygonIndex < m_roi.polygon.size()) {
            m_roi.polygon[m_activePolygonIndex] += delta;
        } else if (m_activeHandle == RoiHandle::Radius) {
            m_roi.radius = qMax(1.0, QLineF(m_roi.center, cur).length());
        } else if (m_activeHandle == RoiHandle::Move) {
            if (m_roi.shape == Selt::RoiShapeType::Polygon) {
                for (QPointF &p : m_roi.polygon)
                    p += delta;
            } else if (m_roi.shape == Selt::RoiShapeType::Circle) {
                m_roi.center += delta;
            } else {
                QRectF box = m_roi.boundingRect();
                box.translate(delta);
                applyBoundingBoxToShape(box);
            }
        } else {
            QRectF box = m_roi.boundingRect();
            switch (m_activeHandle) {
            case RoiHandle::TopLeft:
                box.setTopLeft(box.topLeft() + delta);
                break;
            case RoiHandle::TopRight:
                box.setTopRight(box.topRight() + delta);
                break;
            case RoiHandle::BottomLeft:
                box.setBottomLeft(box.bottomLeft() + delta);
                break;
            case RoiHandle::BottomRight:
                box.setBottomRight(box.bottomRight() + delta);
                break;
            default:
                break;
            }
            box = box.normalized();
            if (box.width() < 2)
                box.setWidth(2);
            if (box.height() < 2)
                box.setHeight(2);
            applyBoundingBoxToShape(box);
        }
        clampRoiToImage();
        m_lastMousePos = event->pos();
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
        return;
    }

    if (m_roiEditEnabled && !m_roi.locked) {
        switch (hitTestHandle(event->pos())) {
        case RoiHandle::Move:
            setCursor(Qt::SizeAllCursor);
            break;
        case RoiHandle::TopLeft:
        case RoiHandle::BottomRight:
            setCursor(Qt::SizeFDiagCursor);
            break;
        case RoiHandle::TopRight:
        case RoiHandle::BottomLeft:
            setCursor(Qt::SizeBDiagCursor);
            break;
        case RoiHandle::Radius:
        case RoiHandle::PolygonVertex:
            setCursor(Qt::PointingHandCursor);
            break;
        default:
            setCursor(Qt::ArrowCursor);
            break;
        }
    }

    if (!m_image.isNull()) {
        const QPointF imgPos = widgetToImage(event->pos());
        const int x = qBound(0, int(std::floor(imgPos.x())), m_image.width() - 1);
        const int y = qBound(0, int(std::floor(imgPos.y())), m_image.height() - 1);
        const bool inside = imgPos.x() >= 0 && imgPos.y() >= 0
            && imgPos.x() < m_image.width() && imgPos.y() < m_image.height();
        emit cursorInfoChanged(imgPos, inside ? m_image.pixelColor(x, y) : QColor(), inside);
    } else {
        emit cursorInfoChanged({}, {}, false);
    }
}

void ImageCanvasWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_drawingRoi && event->button() == Qt::LeftButton) {
        m_drawingRoi = false;
        m_roi.normalize();
        clampRoiToImage();
        if (!m_roi.isValid() || m_roi.boundingRect().width() < 2 || m_roi.boundingRect().height() < 2)
            m_roi = Selt::ExtendedRoi{};
        else
            m_roi.enabled = true;
        emitRoiIfChanged();
        update();
        event->accept();
        return;
    }
    if (m_editingRoi && event->button() == Qt::LeftButton) {
        m_editingRoi = false;
        m_activeHandle = RoiHandle::None;
        m_activePolygonIndex = -1;
        m_roi.normalize();
        clampRoiToImage();
        emitRoiIfChanged();
        update();
        event->accept();
        return;
    }
    if (m_panning) {
        m_panning = false;
        event->accept();
    }
}

void ImageCanvasWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (m_polygonDrawing && event->button() == Qt::LeftButton) {
        m_polygonDrawing = false;
        if (m_roi.polygon.size() >= 3) {
            m_roi.enabled = true;
            m_roi.normalize();
            clampRoiToImage();
            emitRoiIfChanged();
        } else {
            m_roi = Selt::ExtendedRoi{};
            emitRoiIfChanged();
        }
        update();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void ImageCanvasWidget::leaveEvent(QEvent *event)
{
    emit cursorInfoChanged({}, {}, false);
    QWidget::leaveEvent(event);
}

void ImageCanvasWidget::emitRoiIfChanged()
{
    emit roiChanged(m_roi.toLegacyRect());
    emit extendedRoiChanged(m_roi);
}

ImagePreviewWidget::ImagePreviewWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto *toolbarHost = new QWidget(this);
    toolbarHost->setObjectName(QStringLiteral("ImagePreviewToolbar"));
    auto *toolbar = new QHBoxLayout(toolbarHost);
    toolbar->setContentsMargins(0, 0, 0, 0);
    toolbar->setSpacing(Selt::UiStyle::compactSpacing);

    m_titleLabel = new QLabel(QStringLiteral("图像预览"), toolbarHost);
    m_titleLabel->setMinimumWidth(48);
    m_roiToolCombo = new QComboBox(toolbarHost);
    m_roiToolCombo->addItem(QStringLiteral("矩形"), static_cast<int>(ImageCanvasWidget::RoiTool::Rectangle));
    m_roiToolCombo->addItem(QStringLiteral("旋转矩形"), static_cast<int>(ImageCanvasWidget::RoiTool::RotatedRect));
    m_roiToolCombo->addItem(QStringLiteral("圆"), static_cast<int>(ImageCanvasWidget::RoiTool::Circle));
    m_roiToolCombo->addItem(QStringLiteral("椭圆"), static_cast<int>(ImageCanvasWidget::RoiTool::Ellipse));
    m_roiToolCombo->addItem(QStringLiteral("多边形"), static_cast<int>(ImageCanvasWidget::RoiTool::Polygon));
    m_roiToolCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_roiToolCombo->setMinimumContentsLength(4);

    m_fitBtn = new QPushButton(toolbarHost);
    m_resetBtn = new QPushButton(toolbarHost);
    m_zoomInBtn = new QPushButton(toolbarHost);
    m_zoomOutBtn = new QPushButton(toolbarHost);
    m_clearRoiBtn = new QPushButton(toolbarHost);
    m_teachTemplateBtn = new QPushButton(toolbarHost);
    m_bgToggleBtn = new QPushButton(toolbarHost);
    configureCompactButton(m_fitBtn, QStringLiteral("适应"), QStringLiteral("适应窗口"));
    configureCompactButton(m_resetBtn, QStringLiteral("1:1"), QStringLiteral("实际像素 1:1"));
    configureCompactButton(m_zoomInBtn, QStringLiteral("+"), QStringLiteral("放大"));
    configureCompactButton(m_zoomOutBtn, QStringLiteral("-"), QStringLiteral("缩小"));
    configureCompactButton(m_clearRoiBtn, QStringLiteral("ROI"), QStringLiteral("清除 ROI"));
    configureCompactButton(m_teachTemplateBtn, QStringLiteral("教示"), QStringLiteral("从当前 ROI 裁切模板并保存到项目资源"));
    configureCompactButton(m_bgToggleBtn, QStringLiteral("棋盘"), QStringLiteral("棋盘格背景"));
    m_bgToggleBtn->setCheckable(true);

    m_overlayFilter = new QComboBox(toolbarHost);
    m_overlayFilter->addItem(QStringLiteral("全部"), -1);
    m_overlayFilter->addItem(QStringLiteral("矩形"), static_cast<int>(OverlayItemType::Rectangle));
    m_overlayFilter->addItem(QStringLiteral("轮廓"), static_cast<int>(OverlayItemType::Contour));
    m_overlayFilter->addItem(QStringLiteral("文字"), static_cast<int>(OverlayItemType::Text));
    m_overlayFilter->addItem(QStringLiteral("直线"), static_cast<int>(OverlayItemType::Line));
    m_overlayFilter->setToolTip(QStringLiteral("Overlay 显示过滤（仅影响预览）"));
    m_overlayFilter->setMinimumContentsLength(3);

    toolbar->addWidget(m_titleLabel, 1);
    toolbar->addWidget(m_roiToolCombo);
    toolbar->addWidget(m_overlayFilter);
    toolbar->addWidget(m_fitBtn);
    toolbar->addWidget(m_resetBtn);
    toolbar->addWidget(m_zoomInBtn);
    toolbar->addWidget(m_zoomOutBtn);
    toolbar->addWidget(m_bgToggleBtn);
    toolbar->addWidget(m_clearRoiBtn);
    toolbar->addWidget(m_teachTemplateBtn);

    // Scroll container must reserve scrollbar height so buttons stay fully clickable
    // in narrow docks (fixed toolbar height previously clipped content).
    auto *toolbarScroll = new QScrollArea(this);
    toolbarScroll->setObjectName(QStringLiteral("ImagePreviewToolbarScroll"));
    toolbarScroll->setWidget(toolbarHost);
    toolbarScroll->setWidgetResizable(true);
    toolbarScroll->setFrameShape(QFrame::NoFrame);
    toolbarScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    toolbarScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    toolbarScroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    const int scrollBarExtent = style()->pixelMetric(QStyle::PM_ScrollBarExtent);
    const int toolbarContentH = qMax(Selt::UiStyle::imageToolbarHeight, 28);
    toolbarHost->setMinimumHeight(toolbarContentH);
    toolbarScroll->setMinimumHeight(toolbarContentH + scrollBarExtent + 2);
    toolbarScroll->setMaximumHeight(toolbarContentH + scrollBarExtent + 8);
    layout->addWidget(toolbarScroll);

    m_canvas = new ImageCanvasWidget(this);
    layout->addWidget(m_canvas, 1);

    m_hudLabel = new QLabel(QStringLiteral("X: -  Y: -  |  RGB: -"), this);
    m_hudLabel->setObjectName(QStringLiteral("ImageHudLabel"));
    layout->addWidget(m_hudLabel);
    resetHud();

    connect(m_fitBtn, &QPushButton::clicked, m_canvas, &ImageCanvasWidget::fitToView);
    connect(m_resetBtn, &QPushButton::clicked, m_canvas, &ImageCanvasWidget::resetView);
    connect(m_zoomInBtn, &QPushButton::clicked, this, [this]() { m_canvas->zoomBy(1.25); });
    connect(m_zoomOutBtn, &QPushButton::clicked, this, [this]() { m_canvas->zoomBy(0.8); });
    connect(m_bgToggleBtn, &QPushButton::toggled, m_canvas, &ImageCanvasWidget::setCheckerboardBackground);
    connect(m_clearRoiBtn, &QPushButton::clicked, this, [this]() {
        m_canvas->clearRoi();
    });
    connect(m_teachTemplateBtn, &QPushButton::clicked, this, &ImagePreviewWidget::teachTemplateRequested);
    connect(m_roiToolCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ImagePreviewWidget::onRoiToolChanged);
    connect(m_overlayFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { applyOverlayFilter(); });
    connect(m_canvas, &ImageCanvasWidget::roiChanged, this, &ImagePreviewWidget::roiChanged);
    connect(m_canvas, &ImageCanvasWidget::extendedRoiChanged, this, &ImagePreviewWidget::extendedRoiChanged);
    connect(m_canvas, &ImageCanvasWidget::viewChanged, this, &ImagePreviewWidget::updateTitle);
    connect(m_canvas, &ImageCanvasWidget::cursorInfoChanged, this,
            [this](const QPointF &pos, const QColor &pixel, bool valid) {
                updateHud(pos, pixel, valid);
            });
}

void ImagePreviewWidget::configureCompactButton(QPushButton *button, const QString &text, const QString &tip)
{
    if (!button)
        return;
    button->setText(text);
    button->setToolTip(tip);
    button->setObjectName(QStringLiteral("ImageToolbarButton"));
    button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    button->setMinimumWidth(28);
    button->setMaximumHeight(26);
}

void ImagePreviewWidget::resetHud()
{
    if (!m_hudLabel)
        return;
    const int zoomPct = m_canvas ? qRound(m_canvas->scale() * 100) : 100;
    if (!m_canvas || m_canvas->image().isNull()) {
        m_hudLabel->setText(QStringLiteral("X: -  Y: -  |  RGB: -  |  %1%  |  -x-").arg(zoomPct));
        return;
    }
    m_hudLabel->setText(QStringLiteral("X: -  Y: -  |  RGB: -  |  %1%  |  %2x%3")
                            .arg(zoomPct)
                            .arg(m_canvas->image().width())
                            .arg(m_canvas->image().height()));
}

void ImagePreviewWidget::updateHud(const QPointF &pos, const QColor &pixel, bool valid)
{
    if (!m_hudLabel || !m_canvas)
        return;
    if (!valid || m_canvas->image().isNull()) {
        resetHud();
        return;
    }

    const QImage &image = m_canvas->image();
    const int zoomPct = qRound(m_canvas->scale() * 100);
    const bool grayscale = image.isGrayscale()
        || image.format() == QImage::Format_Grayscale8
        || image.format() == QImage::Format_Grayscale16
        || (image.format() == QImage::Format_Indexed8 && image.colorCount() > 0);

    if (grayscale) {
        const int gray = qGray(pixel.rgb());
        m_hudLabel->setText(
            QStringLiteral("X: %1  Y: %2  |  Gray:%3  (R:%4 G:%5 B:%6)  |  %7%  |  %8x%9")
                .arg(qRound(pos.x()), 4, 10, QLatin1Char('0'))
                .arg(qRound(pos.y()), 4, 10, QLatin1Char('0'))
                .arg(gray, 3, 10, QLatin1Char('0'))
                .arg(pixel.red(), 3, 10, QLatin1Char('0'))
                .arg(pixel.green(), 3, 10, QLatin1Char('0'))
                .arg(pixel.blue(), 3, 10, QLatin1Char('0'))
                .arg(zoomPct)
                .arg(image.width())
                .arg(image.height()));
        return;
    }

    m_hudLabel->setText(
        QStringLiteral("X: %1  Y: %2  |  R:%3 G:%4 B:%5  |  %6%  |  %7x%8")
            .arg(qRound(pos.x()), 4, 10, QLatin1Char('0'))
            .arg(qRound(pos.y()), 4, 10, QLatin1Char('0'))
            .arg(pixel.red(), 3, 10, QLatin1Char('0'))
            .arg(pixel.green(), 3, 10, QLatin1Char('0'))
            .arg(pixel.blue(), 3, 10, QLatin1Char('0'))
            .arg(zoomPct)
            .arg(image.width())
            .arg(image.height()));
}

void ImagePreviewWidget::onRoiToolChanged(int index)
{
    Q_UNUSED(index);
    const auto tool = static_cast<ImageCanvasWidget::RoiTool>(m_roiToolCombo->currentData().toInt());
    m_canvas->setRoiTool(tool);
}

void ImagePreviewWidget::setImage(const QImage &image, const QString &title)
{
    if (!title.isEmpty())
        m_title = title;
    m_canvas->setImage(image);
    // Preserve user 1:1 / wheel zoom unless auto-fit mode is active.
    if (!image.isNull() && m_canvas->autoFitEnabled())
        m_canvas->fitToView();
    updateTitle();
    resetHud();
}

void ImagePreviewWidget::setOverlays(const OverlayList &overlays)
{
    m_allOverlays = overlays;
    applyOverlayFilter();
}

void ImagePreviewWidget::applyOverlayFilter()
{
    const int type = m_overlayFilter ? m_overlayFilter->currentData().toInt() : -1;
    if (type < 0) {
        m_canvas->setOverlayTypeFilter({});
        m_canvas->setOverlays(m_allOverlays);
        return;
    }
    OverlayList filtered;
    for (const OverlayItem &item : m_allOverlays) {
        if (static_cast<int>(item.type) == type)
            filtered.append(item);
    }
    m_canvas->setOverlayTypeFilter({type});
    m_canvas->setOverlays(filtered);
}

void ImagePreviewWidget::setRoi(const RoiRect &roi)
{
    m_canvas->setRoi(roi);
}

void ImagePreviewWidget::setExtendedRoi(const Selt::ExtendedRoi &roi)
{
    m_canvas->setExtendedRoi(roi);
}

void ImagePreviewWidget::clearImage()
{
    m_title = QStringLiteral("图像预览");
    m_allOverlays.clear();
    m_canvas->setImage(QImage());
    m_canvas->setOverlays({});
    m_canvas->clearRoi();
    updateTitle();
    resetHud();
}

void ImagePreviewWidget::setRoiEditEnabled(bool enabled)
{
    m_canvas->setRoiEditEnabled(enabled);
    m_clearRoiBtn->setEnabled(enabled);
    m_roiToolCombo->setEnabled(enabled);
    m_teachTemplateBtn->setEnabled(enabled);
}

RoiRect ImagePreviewWidget::currentRoi() const
{
    return m_canvas->roi();
}

Selt::ExtendedRoi ImagePreviewWidget::currentExtendedRoi() const
{
    return m_canvas->extendedRoi();
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
    if (m_canvas->image().isNull())
        resetHud();
}
