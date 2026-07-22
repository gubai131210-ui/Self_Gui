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
#include <QPair>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSizePolicy>
#include <QStringList>
#include <QStyle>
#include <QWheelEvent>
#include <QtMath>
#include <QVBoxLayout>

namespace {
constexpr qreal kHandleSize = 8.0;

double distanceToSegment(const QPointF &point, const QPointF &start, const QPointF &end)
{
    const QPointF direction = end - start;
    const double lengthSquared = QPointF::dotProduct(direction, direction);
    if (lengthSquared <= 1e-12)
        return QLineF(point, start).length();
    const double projection = QPointF::dotProduct(point - start, direction) / lengthSquared;
    const double t = qBound(0.0, projection, 1.0);
    return QLineF(point, start + direction * t).length();
}

QString imageFormatName(QImage::Format format)
{
    switch (format) {
    case QImage::Format_Grayscale8:
        return QStringLiteral("Gray8");
    case QImage::Format_Grayscale16:
        return QStringLiteral("Gray16");
    case QImage::Format_RGB32:
    case QImage::Format_ARGB32:
    case QImage::Format_ARGB32_Premultiplied:
        return QStringLiteral("RGB32");
    case QImage::Format_RGB888:
        return QStringLiteral("RGB888");
    case QImage::Format_Indexed8:
        return QStringLiteral("Indexed8");
    default:
        return QStringLiteral("F%1").arg(int(format));
    }
}
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
                         QStringLiteral("暂无图像\n拖拽绘制ROI\n多边形：Ctrl+点击加点，双击结束"));
        return;
    }

    const QRectF disp = imageDisplayRect();
    // Zoom-in: nearest-neighbor keeps edges crisp; zoom-out: smooth reduces aliasing.
    const Qt::TransformationMode mode =
        (m_scale >= 1.0) ? Qt::FastTransformation : Qt::SmoothTransformation;
    const QImage drawn = m_image.scaled(disp.size().toSize(), Qt::IgnoreAspectRatio, mode);
    painter.drawImage(disp.topLeft(), drawn);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

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
            if (poly.size() >= 3)
                painter.drawPolygon(poly);
            else
                painter.drawPolyline(poly);
        }
    }

    drawInteractiveGeometry(painter);
    if (!m_hideRoiOverlay) {
        drawRoiShape(painter);
        drawHandles(painter);
    }
}

void ImageCanvasWidget::setRoiOverlayHidden(bool hidden)
{
    if (m_hideRoiOverlay == hidden)
        return;
    m_hideRoiOverlay = hidden;
    update();
}

void ImageCanvasWidget::setInteractiveGeometry(const Selt::InteractiveGeometrySpec &spec,
                                               const QJsonObject &parameters)
{
    m_geometrySpec = spec;
    m_geometryParameters = parameters;
    m_geometryHandle = -1;
    update();
}

void ImageCanvasWidget::clearInteractiveGeometry()
{
    m_geometrySpec = {};
    m_geometryParameters = {};
    m_geometryHandle = -1;
    update();
}

void ImageCanvasWidget::drawInteractiveGeometry(QPainter &painter) const
{
    if (!m_geometrySpec.editable
        || m_geometrySpec.kind == Selt::GeometryKind::None
        || m_geometrySpec.kind == Selt::GeometryKind::NotApplicable
        || m_geometrySpec.kind == Selt::GeometryKind::Roi
        || m_geometrySpec.kind == Selt::GeometryKind::TemplateRoi)
        return;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QColor geometryColor(40, 190, 255);
    const QColor handleColor(255, 210, 80);
    painter.setPen(QPen(geometryColor, 2.0));
    painter.setBrush(Qt::NoBrush);

    auto point = [this](double x, double y) { return imageToWidget(QPointF(x, y)); };
    auto drawHandle = [&painter](const QPointF &pos, const QColor &color) {
        painter.setPen(QPen(color, 1.5));
        painter.setBrush(color);
        painter.drawEllipse(pos, 5.0, 5.0);
        painter.setBrush(Qt::NoBrush);
    };

    if (m_geometrySpec.kind == Selt::GeometryKind::Point) {
        const QString xKey = m_geometryParameters.contains(QStringLiteral("refX"))
            ? QStringLiteral("refX")
            : QStringLiteral("x");
        const QString yKey = m_geometryParameters.contains(QStringLiteral("refY"))
            ? QStringLiteral("refY")
            : QStringLiteral("y");
        if (m_geometryParameters.contains(xKey) && m_geometryParameters.contains(yKey)) {
            const QPointF p = point(m_geometryParameters.value(xKey).toDouble(),
                                    m_geometryParameters.value(yKey).toDouble());
            painter.drawLine(p + QPointF(-8, 0), p + QPointF(8, 0));
            painter.drawLine(p + QPointF(0, -8), p + QPointF(0, 8));
            drawHandle(p, handleColor);
        }
    } else if (m_geometrySpec.kind == Selt::GeometryKind::TwoPoint) {
        const bool useRef = m_geometryParameters.contains(QStringLiteral("refX"));
        const QString ax = useRef ? QStringLiteral("refX") : QStringLiteral("x1");
        const QString ay = useRef ? QStringLiteral("refY") : QStringLiteral("y1");
        const QString bx = useRef ? QStringLiteral("x") : QStringLiteral("x2");
        const QString by = useRef ? QStringLiteral("y") : QStringLiteral("y2");
        if (m_geometryParameters.contains(ax) && m_geometryParameters.contains(ay)) {
            const QPointF p0 = point(m_geometryParameters.value(ax).toDouble(),
                                     m_geometryParameters.value(ay).toDouble());
            drawHandle(p0, handleColor);
            if (m_geometryParameters.contains(bx) && m_geometryParameters.contains(by)) {
                const QPointF p1 = point(m_geometryParameters.value(bx).toDouble(),
                                         m_geometryParameters.value(by).toDouble());
                painter.drawLine(p0, p1);
                drawHandle(p1, handleColor);
            }
        }
    } else if (m_geometrySpec.kind == Selt::GeometryKind::TwoLineSegment) {
        auto drawSeg = [&](const QString &x0, const QString &y0,
                           const QString &x1, const QString &y1) {
            if (!m_geometryParameters.contains(x0) || !m_geometryParameters.contains(y0)
                || !m_geometryParameters.contains(x1) || !m_geometryParameters.contains(y1))
                return;
            const QPointF p0 = point(m_geometryParameters.value(x0).toDouble(),
                                     m_geometryParameters.value(y0).toDouble());
            const QPointF p1 = point(m_geometryParameters.value(x1).toDouble(),
                                     m_geometryParameters.value(y1).toDouble());
            painter.drawLine(p0, p1);
            drawHandle(p0, handleColor);
            drawHandle(p1, handleColor);
        };
        drawSeg(QStringLiteral("x1"), QStringLiteral("y1"),
                QStringLiteral("x2"), QStringLiteral("y2"));
        drawSeg(QStringLiteral("x3"), QStringLiteral("y3"),
                QStringLiteral("x4"), QStringLiteral("y4"));
    } else if (m_geometrySpec.kind == Selt::GeometryKind::ContourPick) {
        if (m_geometryParameters.contains(QStringLiteral("pickX"))
            && m_geometryParameters.contains(QStringLiteral("pickY"))) {
            const QPointF p = point(m_geometryParameters.value(QStringLiteral("pickX")).toDouble(),
                                    m_geometryParameters.value(QStringLiteral("pickY")).toDouble());
            painter.setPen(QPen(QColor(255, 120, 40), 2.0));
            painter.drawEllipse(p, 10.0, 10.0);
            drawHandle(p, handleColor);
        }
    } else if (m_geometrySpec.kind == Selt::GeometryKind::LineSegment) {
        const double x0 = m_geometryParameters.value(QStringLiteral("x0")).toDouble();
        const double y0 = m_geometryParameters.value(QStringLiteral("y0")).toDouble();
        const double x1 = m_geometryParameters.value(QStringLiteral("x1")).toDouble();
        const double y1 = m_geometryParameters.value(QStringLiteral("y1")).toDouble();
        const QPointF p0 = point(x0, y0);
        const QPointF p1 = point(x1, y1);
        painter.drawLine(p0, p1);
        drawHandle(p0, handleColor);
        drawHandle(p1, handleColor);

        // Preview caliper search strips along the expected segment.
        const int numCalipers = qMax(2, m_geometryParameters.value(QStringLiteral("numCalipers")).toInt(8));
        double searchLen = m_geometryParameters.value(QStringLiteral("searchLength")).toDouble();
        if (!(searchLen > 1.0))
            searchLen = 60.0;
        double projLen = m_geometryParameters.value(QStringLiteral("projectionLength")).toDouble();
        if (!(projLen > 0.5))
            projLen = 10.0;
        const QPointF dir = QPointF(x1 - x0, y1 - y0);
        const double len = std::hypot(dir.x(), dir.y());
        if (len > 1e-6) {
            const QPointF axis = dir / len;
            const QPointF normal(-axis.y(), axis.x());
            painter.setPen(QPen(QColor(255, 200, 80, 180), 1.0));
            for (int i = 0; i < numCalipers; ++i) {
                const double t = (i + 0.5) / double(numCalipers);
                const QPointF c(x0 + t * dir.x(), y0 + t * dir.y());
                const QVector<QPointF> corners = {
                    c - normal * (searchLen * 0.5) - axis * (projLen * 0.5),
                    c + normal * (searchLen * 0.5) - axis * (projLen * 0.5),
                    c + normal * (searchLen * 0.5) + axis * (projLen * 0.5),
                    c - normal * (searchLen * 0.5) + axis * (projLen * 0.5)};
                QPolygonF poly;
                for (const QPointF &corner : corners)
                    poly.append(imageToWidget(corner));
                painter.drawPolygon(poly);
            }
        }
    } else if (m_geometrySpec.kind == Selt::GeometryKind::Circle) {
        const double cx = m_geometryParameters.value(QStringLiteral("cx")).toDouble();
        const double cy = m_geometryParameters.value(QStringLiteral("cy")).toDouble();
        const double radius = m_geometryParameters.value(QStringLiteral("radius")).toDouble();
        const QPointF center = point(cx, cy);
        painter.drawEllipse(center, radius * m_scale, radius * m_scale);
        drawHandle(center, handleColor);
        drawHandle(imageToWidget(QPointF(cx + radius, cy)), handleColor);

        const int numCalipers = qMax(3, m_geometryParameters.value(QStringLiteral("numCalipers")).toInt(12));
        double searchLen = m_geometryParameters.value(QStringLiteral("searchLength")).toDouble();
        if (!(searchLen > 1.0))
            searchLen = qMax(12.0, radius * 0.45);
        double projLen = m_geometryParameters.value(QStringLiteral("projectionLength")).toDouble();
        if (!(projLen > 0.5))
            projLen = qBound(3.0, radius * 0.08, 24.0);
        const bool inward = m_geometryParameters.value(QStringLiteral("searchInward")).toBool(true);
        painter.setPen(QPen(QColor(255, 200, 80, 180), 1.0));
        for (int i = 0; i < numCalipers; ++i) {
            const double angDeg = (360.0 * i) / double(numCalipers);
            const double angRad = qDegreesToRadians(angDeg);
            const QPointF c(cx + radius * std::cos(angRad), cy + radius * std::sin(angRad));
            const double searchAng = inward ? (angDeg + 180.0) : angDeg;
            const double sRad = qDegreesToRadians(searchAng);
            const QPointF axis(std::cos(sRad), std::sin(sRad));
            const QPointF normal(-axis.y(), axis.x());
            const QVector<QPointF> corners = {
                c - axis * (searchLen * 0.5) - normal * (projLen * 0.5),
                c + axis * (searchLen * 0.5) - normal * (projLen * 0.5),
                c + axis * (searchLen * 0.5) + normal * (projLen * 0.5),
                c - axis * (searchLen * 0.5) + normal * (projLen * 0.5)};
            QPolygonF poly;
            for (const QPointF &corner : corners)
                poly.append(imageToWidget(corner));
            painter.drawPolygon(poly);
        }
    } else if (m_geometrySpec.kind == Selt::GeometryKind::CaliperStrip) {
        const double cx = m_geometryParameters.value(QStringLiteral("cx")).toDouble();
        const double cy = m_geometryParameters.value(QStringLiteral("cy")).toDouble();
        const double angle = qDegreesToRadians(
            m_geometryParameters.value(QStringLiteral("angle")).toDouble());
        const double length = qMax(2.0, m_geometryParameters.value(QStringLiteral("length")).toDouble());
        const double width = qMax(1.0, m_geometryParameters.value(QStringLiteral("width")).toDouble());
        const QPointF center(cx, cy);
        const QPointF axis(std::cos(angle), std::sin(angle));
        const QPointF normal(-axis.y(), axis.x());
        const QVector<QPointF> corners = {
            center - axis * (length * 0.5) - normal * (width * 0.5),
            center + axis * (length * 0.5) - normal * (width * 0.5),
            center + axis * (length * 0.5) + normal * (width * 0.5),
            center - axis * (length * 0.5) + normal * (width * 0.5)};
        QPolygonF polygon;
        for (const QPointF &corner : corners)
            polygon.append(imageToWidget(corner));
        painter.drawPolygon(polygon);
        drawHandle(imageToWidget(center), handleColor);
        drawHandle(imageToWidget(center + axis * (length * 0.5)), handleColor);
        drawHandle(imageToWidget(center + normal * (width * 0.5)), handleColor);
    }
    painter.restore();
}

bool ImageCanvasWidget::hitInteractiveGeometry(const QPoint &pos)
{
    if (!m_geometrySpec.editable || m_image.isNull()
        || m_geometrySpec.kind == Selt::GeometryKind::Roi
        || m_geometrySpec.kind == Selt::GeometryKind::TemplateRoi
        || m_geometrySpec.kind == Selt::GeometryKind::NotApplicable)
        return false;
    const QPointF imagePos = widgetToImage(pos);
    const double threshold = 12.0 / qMax(0.05, m_scale);
    auto near = [threshold](const QPointF &a, const QPointF &b) {
        return QLineF(a, b).length() <= threshold;
    };

    if (m_geometrySpec.kind == Selt::GeometryKind::Point) {
        const QString xKey = m_geometryParameters.contains(QStringLiteral("refX"))
            ? QStringLiteral("refX")
            : QStringLiteral("x");
        const QString yKey = m_geometryParameters.contains(QStringLiteral("refY"))
            ? QStringLiteral("refY")
            : QStringLiteral("y");
        if (m_geometryParameters.contains(xKey) && m_geometryParameters.contains(yKey)
            && near(imagePos, QPointF(m_geometryParameters.value(xKey).toDouble(),
                                      m_geometryParameters.value(yKey).toDouble()))) {
            m_geometryHandle = 1;
            return true;
        }
    } else if (m_geometrySpec.kind == Selt::GeometryKind::TwoPoint) {
        const bool useRef = m_geometryParameters.contains(QStringLiteral("refX"));
        const QString ax = useRef ? QStringLiteral("refX") : QStringLiteral("x1");
        const QString ay = useRef ? QStringLiteral("refY") : QStringLiteral("y1");
        const QString bx = useRef ? QStringLiteral("x") : QStringLiteral("x2");
        const QString by = useRef ? QStringLiteral("y") : QStringLiteral("y2");
        if (m_geometryParameters.contains(ax) && m_geometryParameters.contains(ay)
            && near(imagePos, QPointF(m_geometryParameters.value(ax).toDouble(),
                                      m_geometryParameters.value(ay).toDouble()))) {
            m_geometryHandle = 1;
            return true;
        }
        if (m_geometryParameters.contains(bx) && m_geometryParameters.contains(by)
            && near(imagePos, QPointF(m_geometryParameters.value(bx).toDouble(),
                                      m_geometryParameters.value(by).toDouble()))) {
            m_geometryHandle = 2;
            return true;
        }
    } else if (m_geometrySpec.kind == Selt::GeometryKind::TwoLineSegment) {
        const QVector<QPair<QString, QString>> pts = {
            {QStringLiteral("x1"), QStringLiteral("y1")},
            {QStringLiteral("x2"), QStringLiteral("y2")},
            {QStringLiteral("x3"), QStringLiteral("y3")},
            {QStringLiteral("x4"), QStringLiteral("y4")}};
        for (int i = 0; i < pts.size(); ++i) {
            if (!m_geometryParameters.contains(pts[i].first)
                || !m_geometryParameters.contains(pts[i].second))
                continue;
            if (near(imagePos, QPointF(m_geometryParameters.value(pts[i].first).toDouble(),
                                       m_geometryParameters.value(pts[i].second).toDouble()))) {
                m_geometryHandle = i + 1;
                return true;
            }
        }
        if (m_geometryParameters.contains(QStringLiteral("x1"))
            && m_geometryParameters.contains(QStringLiteral("y1"))
            && m_geometryParameters.contains(QStringLiteral("x2"))
            && m_geometryParameters.contains(QStringLiteral("y2"))
            && distanceToSegment(imagePos,
                                 QPointF(m_geometryParameters.value(QStringLiteral("x1")).toDouble(),
                                         m_geometryParameters.value(QStringLiteral("y1")).toDouble()),
                                 QPointF(m_geometryParameters.value(QStringLiteral("x2")).toDouble(),
                                         m_geometryParameters.value(QStringLiteral("y2")).toDouble()))
                <= threshold) {
            m_geometryHandle = 10;
            return true;
        }
        if (m_geometryParameters.contains(QStringLiteral("x3"))
            && m_geometryParameters.contains(QStringLiteral("y3"))
            && m_geometryParameters.contains(QStringLiteral("x4"))
            && m_geometryParameters.contains(QStringLiteral("y4"))
            && distanceToSegment(imagePos,
                                 QPointF(m_geometryParameters.value(QStringLiteral("x3")).toDouble(),
                                         m_geometryParameters.value(QStringLiteral("y3")).toDouble()),
                                 QPointF(m_geometryParameters.value(QStringLiteral("x4")).toDouble(),
                                         m_geometryParameters.value(QStringLiteral("y4")).toDouble()))
                <= threshold) {
            m_geometryHandle = 11;
            return true;
        }
    } else if (m_geometrySpec.kind == Selt::GeometryKind::ContourPick) {
        if (m_geometryParameters.contains(QStringLiteral("pickX"))
            && m_geometryParameters.contains(QStringLiteral("pickY"))
            && near(imagePos, QPointF(m_geometryParameters.value(QStringLiteral("pickX")).toDouble(),
                                      m_geometryParameters.value(QStringLiteral("pickY")).toDouble()))) {
            m_geometryHandle = 1;
            return true;
        }
    } else if (m_geometrySpec.kind == Selt::GeometryKind::LineSegment) {
        const QPointF p0(m_geometryParameters.value(QStringLiteral("x0")).toDouble(),
                         m_geometryParameters.value(QStringLiteral("y0")).toDouble());
        const QPointF p1(m_geometryParameters.value(QStringLiteral("x1")).toDouble(),
                         m_geometryParameters.value(QStringLiteral("y1")).toDouble());
        if (near(imagePos, p0)) {
            m_geometryHandle = 1;
            return true;
        }
        if (near(imagePos, p1)) {
            m_geometryHandle = 2;
            return true;
        }
        if (distanceToSegment(imagePos, p0, p1) <= threshold) {
            m_geometryHandle = 0;
            return true;
        }
    } else if (m_geometrySpec.kind == Selt::GeometryKind::Circle) {
        const QPointF center(m_geometryParameters.value(QStringLiteral("cx")).toDouble(),
                             m_geometryParameters.value(QStringLiteral("cy")).toDouble());
        const double radius = m_geometryParameters.value(QStringLiteral("radius")).toDouble();
        if (near(imagePos, center)) {
            m_geometryHandle = 1;
            return true;
        }
        if (qAbs(QLineF(center, imagePos).length() - radius) <= threshold) {
            m_geometryHandle = 2;
            return true;
        }
        if (QLineF(center, imagePos).length() <= radius + threshold) {
            m_geometryHandle = 0;
            return true;
        }
    } else if (m_geometrySpec.kind == Selt::GeometryKind::CaliperStrip) {
        const QPointF center(m_geometryParameters.value(QStringLiteral("cx")).toDouble(),
                             m_geometryParameters.value(QStringLiteral("cy")).toDouble());
        const double angle = qDegreesToRadians(
            m_geometryParameters.value(QStringLiteral("angle")).toDouble());
        const double length = qMax(2.0, m_geometryParameters.value(QStringLiteral("length")).toDouble());
        const double width = qMax(1.0, m_geometryParameters.value(QStringLiteral("width")).toDouble());
        const QPointF axis(std::cos(angle), std::sin(angle));
        const QPointF normal(-axis.y(), axis.x());
        if (near(imagePos, center)) {
            m_geometryHandle = 0;
            return true;
        }
        if (near(imagePos, center + axis * (length * 0.5))) {
            m_geometryHandle = 1;
            return true;
        }
        if (near(imagePos, center + normal * (width * 0.5))) {
            m_geometryHandle = 2;
            return true;
        }
    }
    m_geometryHandle = -1;
    return false;
}

void ImageCanvasWidget::updateInteractiveGeometry(const QPoint &pos)
{
    if (m_geometryHandle < 0)
        return;
    const QPointF current = widgetToImage(pos);
    const QPointF delta = current - m_geometryStart;
    if (m_geometrySpec.kind == Selt::GeometryKind::Point) {
        const QString xKey = m_geometryParameters.contains(QStringLiteral("refX"))
            ? QStringLiteral("refX")
            : QStringLiteral("x");
        const QString yKey = m_geometryParameters.contains(QStringLiteral("refY"))
            ? QStringLiteral("refY")
            : QStringLiteral("y");
        m_geometryParameters.insert(xKey, current.x());
        m_geometryParameters.insert(yKey, current.y());
    } else if (m_geometrySpec.kind == Selt::GeometryKind::TwoPoint) {
        const bool useRef = m_geometryParameters.contains(QStringLiteral("refX"));
        const QString ax = useRef ? QStringLiteral("refX") : QStringLiteral("x1");
        const QString ay = useRef ? QStringLiteral("refY") : QStringLiteral("y1");
        const QString bx = useRef ? QStringLiteral("x") : QStringLiteral("x2");
        const QString by = useRef ? QStringLiteral("y") : QStringLiteral("y2");
        if (m_geometryHandle == 1) {
            m_geometryParameters.insert(ax, current.x());
            m_geometryParameters.insert(ay, current.y());
        } else if (m_geometryHandle == 2) {
            m_geometryParameters.insert(bx, current.x());
            m_geometryParameters.insert(by, current.y());
        }
    } else if (m_geometrySpec.kind == Selt::GeometryKind::TwoLineSegment) {
        if (m_geometryHandle >= 1 && m_geometryHandle <= 4) {
            const QVector<QPair<QString, QString>> pts = {
                {QStringLiteral("x1"), QStringLiteral("y1")},
                {QStringLiteral("x2"), QStringLiteral("y2")},
                {QStringLiteral("x3"), QStringLiteral("y3")},
                {QStringLiteral("x4"), QStringLiteral("y4")}};
            const auto &keys = pts[m_geometryHandle - 1];
            m_geometryParameters.insert(keys.first, current.x());
            m_geometryParameters.insert(keys.second, current.y());
        } else if (m_geometryHandle == 10 || m_geometryHandle == 11) {
            const QStringList keys = (m_geometryHandle == 10)
                ? QStringList{QStringLiteral("x1"), QStringLiteral("y1"),
                              QStringLiteral("x2"), QStringLiteral("y2")}
                : QStringList{QStringLiteral("x3"), QStringLiteral("y3"),
                              QStringLiteral("x4"), QStringLiteral("y4")};
            for (int i = 0; i < 4; i += 2) {
                m_geometryParameters.insert(keys[i],
                    m_geometryParameters.value(keys[i]).toDouble() + delta.x());
                m_geometryParameters.insert(keys[i + 1],
                    m_geometryParameters.value(keys[i + 1]).toDouble() + delta.y());
            }
        }
    } else if (m_geometrySpec.kind == Selt::GeometryKind::ContourPick) {
        m_geometryParameters.insert(QStringLiteral("pickX"), current.x());
        m_geometryParameters.insert(QStringLiteral("pickY"), current.y());
    } else if (m_geometrySpec.kind == Selt::GeometryKind::LineSegment) {
        double x0 = m_geometryParameters.value(QStringLiteral("x0")).toDouble();
        double y0 = m_geometryParameters.value(QStringLiteral("y0")).toDouble();
        double x1 = m_geometryParameters.value(QStringLiteral("x1")).toDouble();
        double y1 = m_geometryParameters.value(QStringLiteral("y1")).toDouble();
        if (m_geometryHandle == 1) {
            x0 = current.x();
            y0 = current.y();
        } else if (m_geometryHandle == 2) {
            x1 = current.x();
            y1 = current.y();
        } else {
            x0 += delta.x();
            y0 += delta.y();
            x1 += delta.x();
            y1 += delta.y();
        }
        m_geometryParameters.insert(QStringLiteral("x0"), x0);
        m_geometryParameters.insert(QStringLiteral("y0"), y0);
        m_geometryParameters.insert(QStringLiteral("x1"), x1);
        m_geometryParameters.insert(QStringLiteral("y1"), y1);
    } else if (m_geometrySpec.kind == Selt::GeometryKind::Circle) {
        double cx = m_geometryParameters.value(QStringLiteral("cx")).toDouble();
        double cy = m_geometryParameters.value(QStringLiteral("cy")).toDouble();
        double radius = m_geometryParameters.value(QStringLiteral("radius")).toDouble();
        if (m_geometryHandle == 1) {
            cx = current.x();
            cy = current.y();
        } else if (m_geometryHandle == 2) {
            radius = qMax(2.0, QLineF(QPointF(cx, cy), current).length());
        } else {
            cx += delta.x();
            cy += delta.y();
        }
        m_geometryParameters.insert(QStringLiteral("cx"), cx);
        m_geometryParameters.insert(QStringLiteral("cy"), cy);
        m_geometryParameters.insert(QStringLiteral("radius"), radius);
    } else if (m_geometrySpec.kind == Selt::GeometryKind::CaliperStrip) {
        double cx = m_geometryParameters.value(QStringLiteral("cx")).toDouble();
        double cy = m_geometryParameters.value(QStringLiteral("cy")).toDouble();
        const double angle = qDegreesToRadians(
            m_geometryParameters.value(QStringLiteral("angle")).toDouble());
        const QPointF axis(std::cos(angle), std::sin(angle));
        const QPointF normal(-axis.y(), axis.x());
        double length = qMax(2.0, m_geometryParameters.value(QStringLiteral("length")).toDouble());
        double width = qMax(1.0, m_geometryParameters.value(QStringLiteral("width")).toDouble());
        const QPointF center(cx, cy);
        if (m_geometryHandle == 0) {
            cx += delta.x();
            cy += delta.y();
        } else if (m_geometryHandle == 1) {
            length = qMax(2.0, 2.0 * QPointF::dotProduct(current - center, axis));
        } else if (m_geometryHandle == 2) {
            width = qMax(1.0, 2.0 * qAbs(QPointF::dotProduct(current - center, normal)));
        }
        m_geometryParameters.insert(QStringLiteral("cx"), cx);
        m_geometryParameters.insert(QStringLiteral("cy"), cy);
        m_geometryParameters.insert(QStringLiteral("length"), length);
        m_geometryParameters.insert(QStringLiteral("width"), width);
    }
    m_geometryStart = current;
    update();
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

    const bool shiftTeach = (event->modifiers() & Qt::ShiftModifier);
    const bool ctrlRoi = (event->modifiers() & Qt::ControlModifier);

    // Ctrl prefers ROI handles so geometry tools do not steal ROI edits.
    if (m_roiEditEnabled && event->button() == Qt::LeftButton && !m_roi.locked && ctrlRoi) {
        const RoiHandle handle = hitTestHandle(event->pos());
        if (handle != RoiHandle::None) {
            m_editingRoi = true;
            m_editingGeometry = false;
            m_geometryHandle = -1;
            m_activeHandle = handle;
            m_lastMousePos = event->pos();
            m_roiAtEditStart = m_roi.boundingRect();
            event->accept();
            return;
        }
    }

    if (m_geometrySpec.editable && event->button() == Qt::LeftButton
        && m_geometrySpec.kind != Selt::GeometryKind::None
        && m_geometrySpec.kind != Selt::GeometryKind::NotApplicable
        && m_geometrySpec.kind != Selt::GeometryKind::Roi
        && m_geometrySpec.kind != Selt::GeometryKind::TemplateRoi) {
        if (hitInteractiveGeometry(event->pos())) {
            m_editingGeometry = true;
            m_editingRoi = false;
            m_geometryStart = widgetToImage(event->pos());
            event->accept();
            return;
        }

        // Click-to-teach only when geometry is missing; Point/ContourPick also accept Shift.
        const QPointF imagePos = widgetToImage(event->pos());
        bool placed = false;
        auto missingKeys = [this](const QStringList &keys) {
            for (const QString &k : keys) {
                if (!m_geometryParameters.contains(k))
                    return true;
            }
            return false;
        };
        if (m_geometrySpec.kind == Selt::GeometryKind::Point
            && (shiftTeach
                || missingKeys({QStringLiteral("x"), QStringLiteral("y")})
                || missingKeys({QStringLiteral("refX"), QStringLiteral("refY")}))) {
            const bool useRef = m_geometrySpec.parameterKeys.contains(QStringLiteral("refX"));
            m_geometryParameters.insert(useRef ? QStringLiteral("refX") : QStringLiteral("x"),
                                        imagePos.x());
            m_geometryParameters.insert(useRef ? QStringLiteral("refY") : QStringLiteral("y"),
                                        imagePos.y());
            placed = true;
        } else if (m_geometrySpec.kind == Selt::GeometryKind::ContourPick
                   && (shiftTeach
                       || missingKeys({QStringLiteral("pickX"), QStringLiteral("pickY")}))) {
            m_geometryParameters.insert(QStringLiteral("pickX"), imagePos.x());
            m_geometryParameters.insert(QStringLiteral("pickY"), imagePos.y());
            placed = true;
        } else if (m_geometrySpec.kind == Selt::GeometryKind::TwoPoint) {
            const bool useRef = m_geometrySpec.parameterKeys.contains(QStringLiteral("refX"));
            const QString ax = useRef ? QStringLiteral("refX") : QStringLiteral("x1");
            const QString ay = useRef ? QStringLiteral("refY") : QStringLiteral("y1");
            const QString bx = useRef ? QStringLiteral("x") : QStringLiteral("x2");
            const QString by = useRef ? QStringLiteral("y") : QStringLiteral("y2");
            if (!m_geometryParameters.contains(ax) || !m_geometryParameters.contains(ay)) {
                m_geometryParameters.insert(ax, imagePos.x());
                m_geometryParameters.insert(ay, imagePos.y());
                placed = true;
            } else if (!m_geometryParameters.contains(bx) || !m_geometryParameters.contains(by)) {
                m_geometryParameters.insert(bx, imagePos.x());
                m_geometryParameters.insert(by, imagePos.y());
                placed = true;
            }
        } else if (m_geometrySpec.kind == Selt::GeometryKind::LineSegment
                   && (!m_geometryParameters.contains(QStringLiteral("x0"))
                       || !m_geometryParameters.contains(QStringLiteral("x1")))) {
            m_geometryParameters.insert(QStringLiteral("x0"), imagePos.x() - 40.0);
            m_geometryParameters.insert(QStringLiteral("y0"), imagePos.y());
            m_geometryParameters.insert(QStringLiteral("x1"), imagePos.x() + 40.0);
            m_geometryParameters.insert(QStringLiteral("y1"), imagePos.y());
            placed = true;
        } else if (m_geometrySpec.kind == Selt::GeometryKind::Circle
                   && (!m_geometryParameters.contains(QStringLiteral("cx"))
                       || !m_geometryParameters.contains(QStringLiteral("radius")))) {
            m_geometryParameters.insert(QStringLiteral("cx"), imagePos.x());
            m_geometryParameters.insert(QStringLiteral("cy"), imagePos.y());
            m_geometryParameters.insert(QStringLiteral("radius"), 40.0);
            placed = true;
        } else if (m_geometrySpec.kind == Selt::GeometryKind::CaliperStrip
                   && (!m_geometryParameters.contains(QStringLiteral("cx"))
                       || !m_geometryParameters.contains(QStringLiteral("cy")))) {
            m_geometryParameters.insert(QStringLiteral("cx"), imagePos.x());
            m_geometryParameters.insert(QStringLiteral("cy"), imagePos.y());
            if (!m_geometryParameters.contains(QStringLiteral("length")))
                m_geometryParameters.insert(QStringLiteral("length"), 80.0);
            if (!m_geometryParameters.contains(QStringLiteral("width")))
                m_geometryParameters.insert(QStringLiteral("width"), 8.0);
            if (!m_geometryParameters.contains(QStringLiteral("angle")))
                m_geometryParameters.insert(QStringLiteral("angle"), 0.0);
            placed = true;
        } else if (m_geometrySpec.kind == Selt::GeometryKind::TwoLineSegment
                   && (!m_geometryParameters.contains(QStringLiteral("x1"))
                       || !m_geometryParameters.contains(QStringLiteral("x2")))) {
            m_geometryParameters.insert(QStringLiteral("x1"), imagePos.x() - 40.0);
            m_geometryParameters.insert(QStringLiteral("y1"), imagePos.y());
            m_geometryParameters.insert(QStringLiteral("x2"), imagePos.x() + 40.0);
            m_geometryParameters.insert(QStringLiteral("y2"), imagePos.y());
            placed = true;
        }
        if (placed) {
            emit interactiveGeometryChanged(m_geometryParameters);
            update();
            event->accept();
            return;
        }
    }

    const bool directRoiDraw = m_roiEditEnabled
        && event->button() == Qt::LeftButton
        && ((event->modifiers() & Qt::ControlModifier) || !m_roi.isValid());
    if (directRoiDraw) {
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
        m_editingGeometry = false;
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
            m_editingGeometry = false;
            m_geometryHandle = -1;
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
    if (m_editingGeometry && m_geometryHandle >= 0
        && m_geometrySpec.kind != Selt::GeometryKind::None) {
        updateInteractiveGeometry(event->pos());
        event->accept();
        return;
    }

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
    if (m_editingGeometry && m_geometryHandle >= 0
        && m_geometrySpec.kind != Selt::GeometryKind::None
        && event->button() == Qt::LeftButton) {
        m_editingGeometry = false;
        m_geometryHandle = -1;
        emit interactiveGeometryChanged(m_geometryParameters);
        update();
        event->accept();
        return;
    }

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

    m_previewSourceCombo = new QComboBox(toolbarHost);
    m_previewSourceCombo->addItem(QStringLiteral("结果图"), 0);
    m_previewSourceCombo->setToolTip(QStringLiteral("切换：原图 / ROI / 预处理 / 结果 / 调试图"));
    m_previewSourceCombo->setMinimumContentsLength(5);

    toolbar->addWidget(m_titleLabel, 1);
    toolbar->addWidget(m_previewSourceCombo);
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
    // 不依赖主题是否加载成功：固定高对比底栏，避免与深色图像混叠。
    m_hudLabel->setStyleSheet(
        QStringLiteral(
            "QLabel#ImageHudLabel {"
            " color: %1;"
            " background-color: %2;"
            " padding: 4px 10px;"
            " border-top: 1px solid %3;"
            " font-family: Consolas, 'Courier New', monospace;"
            " font-size: 12px;"
            "}")
            .arg(Selt::UiStyle::textPrimary().name(),
                 Selt::UiStyle::panelBackground().name(),
                 Selt::UiStyle::border().name()));
    m_hudLabel->setMinimumHeight(24);
    m_hudLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_hudLabel);

    m_diagnosisLabel = new QLabel(QStringLiteral("诊断：-"), this);
    m_diagnosisLabel->setObjectName(QStringLiteral("ImageDiagnosisLabel"));
    m_diagnosisLabel->setWordWrap(true);
    m_diagnosisLabel->setStyleSheet(
        QStringLiteral("QLabel#ImageDiagnosisLabel { color: %1; background-color: %2;"
                       " border-top: 1px solid %3; padding: 3px 8px; font-size: 11px; }")
            .arg(Selt::UiStyle::textSecondary().name(),
                 Selt::UiStyle::panelAltBackground().name(),
                 Selt::UiStyle::border().name()));
    layout->addWidget(m_diagnosisLabel);

    // Keep cursor metadata physically outside the image canvas. This prevents
    // bright/dark image content from reducing HUD readability.
    layout->removeWidget(m_hudLabel);
    layout->removeWidget(m_diagnosisLabel);
    layout->insertWidget(1, m_hudLabel);
    layout->insertWidget(2, m_diagnosisLabel);
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
    connect(m_previewSourceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ImagePreviewWidget::onPreviewSourceChanged);
    connect(m_canvas, &ImageCanvasWidget::roiChanged, this, &ImagePreviewWidget::roiChanged);
    connect(m_canvas, &ImageCanvasWidget::extendedRoiChanged, this, &ImagePreviewWidget::extendedRoiChanged);
    connect(m_canvas, &ImageCanvasWidget::interactiveGeometryChanged,
            this, &ImagePreviewWidget::interactiveGeometryChanged);
    connect(m_canvas, &ImageCanvasWidget::viewChanged, this, &ImagePreviewWidget::updateTitle);
    connect(m_canvas, &ImageCanvasWidget::cursorInfoChanged, this,
            [this](const QPointF &pos, const QColor &pixel, bool valid) {
                updateHud(pos, pixel, valid);
            });
}

void ImagePreviewWidget::setInteractiveGeometry(const Selt::InteractiveGeometrySpec &spec,
                                                const QJsonObject &parameters)
{
    if (m_canvas)
        m_canvas->setInteractiveGeometry(spec, parameters);
}

void ImagePreviewWidget::clearInteractiveGeometry()
{
    if (m_canvas)
        m_canvas->clearInteractiveGeometry();
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
    QString roiText = QStringLiteral("ROI:-");
    if (m_canvas) {
        const RoiRect roi = m_canvas->roi();
        if (roi.enabled && roi.rect.isValid()) {
            roiText = QStringLiteral("ROI:%1,%2 %3x%4")
                          .arg(int(roi.rect.x()))
                          .arg(int(roi.rect.y()))
                          .arg(int(roi.rect.width()))
                          .arg(int(roi.rect.height()));
        }
    }
    if (!m_canvas || m_canvas->image().isNull()) {
        m_hudLabel->setText(QStringLiteral("X:- Y:- | RGB:- | %1% | -x- | %2 | fmt:-")
                                .arg(zoomPct)
                                .arg(roiText));
        return;
    }
    const QImage &image = m_canvas->image();
    m_hudLabel->setText(QStringLiteral("X:- Y:- | RGB:- | %1% | %2x%3 | %4 | fmt:%5")
                            .arg(zoomPct)
                            .arg(image.width())
                            .arg(image.height())
                            .arg(roiText)
                            .arg(imageFormatName(image.format())));
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
    QString roiText = QStringLiteral("ROI:-");
    const RoiRect roi = m_canvas->roi();
    if (roi.enabled && roi.rect.isValid()) {
        roiText = QStringLiteral("ROI:%1,%2 %3x%4")
                      .arg(int(roi.rect.x()))
                      .arg(int(roi.rect.y()))
                      .arg(int(roi.rect.width()))
                      .arg(int(roi.rect.height()));
    }
    const QString fmt = imageFormatName(image.format());
    const bool grayscale = image.isGrayscale()
        || image.format() == QImage::Format_Grayscale8
        || image.format() == QImage::Format_Grayscale16
        || (image.format() == QImage::Format_Indexed8 && image.colorCount() > 0);

    if (grayscale) {
        const int gray = qGray(pixel.rgb());
        m_hudLabel->setText(
            QStringLiteral("X:%1 Y:%2 | Gray:%3 (R:%4 G:%5 B:%6) | %7% | %8x%9 | %10 | fmt:%11")
                .arg(qRound(pos.x()), 4, 10, QLatin1Char('0'))
                .arg(qRound(pos.y()), 4, 10, QLatin1Char('0'))
                .arg(gray, 3, 10, QLatin1Char('0'))
                .arg(pixel.red(), 3, 10, QLatin1Char('0'))
                .arg(pixel.green(), 3, 10, QLatin1Char('0'))
                .arg(pixel.blue(), 3, 10, QLatin1Char('0'))
                .arg(zoomPct)
                .arg(image.width())
                .arg(image.height())
                .arg(roiText)
                .arg(fmt));
        return;
    }

    m_hudLabel->setText(
        QStringLiteral("X:%1 Y:%2 | R:%3 G:%4 B:%5 | %6% | %7x%8 | %9 | fmt:%10")
            .arg(qRound(pos.x()), 4, 10, QLatin1Char('0'))
            .arg(qRound(pos.y()), 4, 10, QLatin1Char('0'))
            .arg(pixel.red(), 3, 10, QLatin1Char('0'))
            .arg(pixel.green(), 3, 10, QLatin1Char('0'))
            .arg(pixel.blue(), 3, 10, QLatin1Char('0'))
            .arg(zoomPct)
            .arg(image.width())
            .arg(image.height())
            .arg(roiText)
            .arg(fmt));
}

void ImagePreviewWidget::onRoiToolChanged(int index)
{
    Q_UNUSED(index);
    const auto tool = static_cast<ImageCanvasWidget::RoiTool>(m_roiToolCombo->currentData().toInt());
    m_canvas->setRoiTool(tool);
}

void ImagePreviewWidget::onPreviewSourceChanged(int)
{
    applyDisplayedImage();
}

void ImagePreviewWidget::rebuildPreviewSourceCombo()
{
    if (!m_previewSourceCombo)
        return;
    const int previous = m_previewSourceCombo->currentData().toInt();
    m_previewSourceCombo->blockSignals(true);
    m_previewSourceCombo->clear();
    m_previewSourceCombo->addItem(QStringLiteral("结果图"), 0);
    if (!m_originalImage.isNull())
        m_previewSourceCombo->addItem(QStringLiteral("原图"), 2);
    if (!m_roiImage.isNull())
        m_previewSourceCombo->addItem(QStringLiteral("ROI"), 3);
    if (!m_preprocessImage.isNull() || !m_debugImage.isNull())
        m_previewSourceCombo->addItem(
            m_debugLabel.isEmpty() ? QStringLiteral("预处理/调试") : m_debugLabel, 1);
    int restore = m_previewSourceCombo->findData(previous);
    if (restore < 0)
        restore = 0;
    m_previewSourceCombo->setCurrentIndex(restore);
    m_previewSourceCombo->blockSignals(false);
}

void ImagePreviewWidget::applyDisplayedImage()
{
    if (!m_canvas)
        return;
    const int mode = m_previewSourceCombo ? m_previewSourceCombo->currentData().toInt() : 0;
    QImage image = m_resultImage;
    if (mode == 1) {
        image = !m_preprocessImage.isNull() ? m_preprocessImage : m_debugImage;
    } else if (mode == 2) {
        image = m_originalImage;
    } else if (mode == 3) {
        image = m_roiImage;
    }
    // ROI 裁剪视图下隐藏轮廓框，避免干扰查看。
    m_canvas->setRoiOverlayHidden(mode == 3 && !m_roiImage.isNull());
    if (!image.isNull())
        m_canvas->setImage(image);
    // 切换层级时默认适应窗口，避免上一层缩放导致误判尺寸。
    if (m_resetViewOnSourceChange && !image.isNull() && m_canvas->autoFitEnabled())
        m_canvas->fitToView();
    updateTitle();
    resetHud();
}

void ImagePreviewWidget::setDiagnosisText(const QString &text)
{
    if (!m_diagnosisLabel)
        return;
    m_diagnosisLabel->setText(text.isEmpty() ? QStringLiteral("诊断：-") : text);
}

void ImagePreviewWidget::setDebugImage(const QImage &image, const QString &label)
{
    m_debugImage = image;
    m_preprocessImage = image;
    m_debugLabel = label.isEmpty() ? QStringLiteral("调试图") : label;
    rebuildPreviewSourceCombo();
}

void ImagePreviewWidget::setPreviewLayers(const QImage &original,
                                          const QImage &roiCrop,
                                          const QImage &preprocess)
{
    m_originalImage = original;
    m_roiImage = roiCrop;
    if (!preprocess.isNull()) {
        m_preprocessImage = preprocess;
        m_debugImage = preprocess;
    }
    rebuildPreviewSourceCombo();
    // 有有效 ROI 裁剪时，优先切到 ROI 视图，只看框选内容。
    if (m_previewSourceCombo && !m_roiImage.isNull()) {
        const int roiIndex = m_previewSourceCombo->findData(3);
        if (roiIndex >= 0) {
            m_previewSourceCombo->blockSignals(true);
            m_previewSourceCombo->setCurrentIndex(roiIndex);
            m_previewSourceCombo->blockSignals(false);
        }
    }
    applyDisplayedImage();
}

void ImagePreviewWidget::setImage(const QImage &image, const QString &title)
{
    if (!title.isEmpty())
        m_title = title;
    m_resultImage = image;
    if (m_previewSourceCombo) {
        const int resultIndex = m_previewSourceCombo->findData(0);
        if (resultIndex >= 0)
            m_previewSourceCombo->setCurrentIndex(resultIndex);
    }
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
    m_resultImage = QImage();
    m_debugImage = QImage();
    m_originalImage = QImage();
    m_roiImage = QImage();
    m_preprocessImage = QImage();
    setDebugImage(QImage());
    setDiagnosisText(QString());
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
