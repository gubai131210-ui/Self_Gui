#ifndef EXTENDEDROI_H
#define EXTENDEDROI_H

#include "vision/model/roi.h"

#include <QJsonArray>
#include <QPointF>
#include <QSize>
#include <QtGlobal>
#include <QVector>

namespace Selt {

enum class RoiShapeType {
    Rectangle,
    RotatedRect,
    Circle,
    Ellipse,
    Polygon
};

struct ExtendedRoi
{
    QString id;
    RoiShapeType shape{RoiShapeType::Rectangle};
    QRectF rect;
    QPointF center;
    double width{0};
    double height{0};
    double angleDeg{0};
    double radius{0};
    QVector<QPointF> polygon;
    bool enabled{false};
    bool locked{false};
    bool visible{true};
    int zOrder{0};

    bool isValid() const;
    void normalize();
    void clampToImage(const QSize &imageSize);
    QRectF boundingRect() const;

    RoiRect toLegacyRect() const;
    static ExtendedRoi fromLegacyRect(const RoiRect &roi);

    QJsonObject toJson() const;
    static ExtendedRoi fromJson(const QJsonObject &obj);
};

inline bool ExtendedRoi::isValid() const
{
    if (!enabled)
        return false;
    switch (shape) {
    case RoiShapeType::Circle:
        return radius > 0.0;
    case RoiShapeType::Polygon:
        return polygon.size() >= 3;
    case RoiShapeType::RotatedRect:
    case RoiShapeType::Ellipse:
        return width > 0.0 && height > 0.0;
    case RoiShapeType::Rectangle:
    default:
        return rect.width() > 0.0 && rect.height() > 0.0;
    }
}

inline void ExtendedRoi::normalize()
{
    if (shape == RoiShapeType::Rectangle)
        rect = rect.normalized();
    if (width < 0)
        width = -width;
    if (height < 0)
        height = -height;
    if (radius < 0)
        radius = -radius;
}

inline void ExtendedRoi::clampToImage(const QSize &imageSize)
{
    if (imageSize.width() <= 0 || imageSize.height() <= 0)
        return;
    const QRectF bounds(0, 0, imageSize.width(), imageSize.height());
    if (shape == RoiShapeType::Rectangle) {
        rect = rect.intersected(bounds);
        return;
    }
    if (shape == RoiShapeType::Circle) {
        center.setX(qBound(0.0, center.x(), double(imageSize.width())));
        center.setY(qBound(0.0, center.y(), double(imageSize.height())));
        radius = qMin(radius, qMin(center.x(), qMin(center.y(),
                                                    qMin(imageSize.width() - center.x(),
                                                         imageSize.height() - center.y()))));
        return;
    }
    // Generic: clamp axis-aligned bounding box.
    QRectF box = boundingRect().intersected(bounds);
    if (shape == RoiShapeType::RotatedRect || shape == RoiShapeType::Ellipse) {
        center = box.center();
        width = box.width();
        height = box.height();
    }
}

inline QRectF ExtendedRoi::boundingRect() const
{
    switch (shape) {
    case RoiShapeType::Circle:
        return QRectF(center.x() - radius, center.y() - radius, radius * 2.0, radius * 2.0);
    case RoiShapeType::Polygon: {
        if (polygon.isEmpty())
            return {};
        QRectF box(polygon.first(), QSizeF(0, 0));
        for (const QPointF &p : polygon)
            box = box.united(QRectF(p, QSizeF(0, 0)));
        return box;
    }
    case RoiShapeType::RotatedRect:
    case RoiShapeType::Ellipse:
        return QRectF(center.x() - width * 0.5, center.y() - height * 0.5, width, height);
    case RoiShapeType::Rectangle:
    default:
        return rect;
    }
}

inline RoiRect ExtendedRoi::toLegacyRect() const
{
    RoiRect r;
    r.id = id;
    r.enabled = enabled;
    r.locked = locked;
    r.rect = boundingRect();
    return r;
}

inline ExtendedRoi ExtendedRoi::fromLegacyRect(const RoiRect &roi)
{
    ExtendedRoi r;
    r.id = roi.id;
    r.shape = RoiShapeType::Rectangle;
    r.rect = roi.rect.normalized();
    r.center = r.rect.center();
    r.width = r.rect.width();
    r.height = r.rect.height();
    r.enabled = roi.enabled;
    r.locked = roi.locked;
    return r;
}

inline QJsonObject ExtendedRoi::toJson() const
{
    QJsonObject obj{
        {QStringLiteral("id"), id},
        {QStringLiteral("shape"), static_cast<int>(shape)},
        {QStringLiteral("x"), rect.x()},
        {QStringLiteral("y"), rect.y()},
        {QStringLiteral("width"), rect.width()},
        {QStringLiteral("height"), rect.height()},
        {QStringLiteral("cx"), center.x()},
        {QStringLiteral("cy"), center.y()},
        {QStringLiteral("w"), width},
        {QStringLiteral("h"), height},
        {QStringLiteral("angle"), angleDeg},
        {QStringLiteral("radius"), radius},
        {QStringLiteral("enabled"), enabled},
        {QStringLiteral("locked"), locked},
        {QStringLiteral("visible"), visible},
        {QStringLiteral("zOrder"), zOrder}};
    QJsonArray pts;
    for (const QPointF &p : polygon)
        pts.append(QJsonObject{{QStringLiteral("x"), p.x()}, {QStringLiteral("y"), p.y()}});
    obj.insert(QStringLiteral("polygon"), pts);
    return obj;
}

inline ExtendedRoi ExtendedRoi::fromJson(const QJsonObject &obj)
{
    ExtendedRoi r;
    r.id = obj.value(QStringLiteral("id")).toString();
    r.shape = static_cast<RoiShapeType>(obj.value(QStringLiteral("shape")).toInt());
    r.rect = QRectF(obj.value(QStringLiteral("x")).toDouble(),
                    obj.value(QStringLiteral("y")).toDouble(),
                    obj.value(QStringLiteral("width")).toDouble(),
                    obj.value(QStringLiteral("height")).toDouble());
    r.center = QPointF(obj.value(QStringLiteral("cx")).toDouble(),
                       obj.value(QStringLiteral("cy")).toDouble());
    r.width = obj.value(QStringLiteral("w")).toDouble();
    r.height = obj.value(QStringLiteral("h")).toDouble();
    r.angleDeg = obj.value(QStringLiteral("angle")).toDouble();
    r.radius = obj.value(QStringLiteral("radius")).toDouble();
    r.enabled = obj.value(QStringLiteral("enabled")).toBool();
    r.locked = obj.value(QStringLiteral("locked")).toBool();
    r.visible = obj.value(QStringLiteral("visible")).toBool(true);
    r.zOrder = obj.value(QStringLiteral("zOrder")).toInt();
    const QJsonArray pts = obj.value(QStringLiteral("polygon")).toArray();
    for (const QJsonValue &v : pts) {
        const QJsonObject p = v.toObject();
        r.polygon.append(QPointF(p.value(QStringLiteral("x")).toDouble(),
                                 p.value(QStringLiteral("y")).toDouble()));
    }
    return r;
}

} // namespace Selt

#endif
