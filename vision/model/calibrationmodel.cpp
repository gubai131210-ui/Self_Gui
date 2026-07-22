#include "calibrationmodel.h"

#include <QtMath>

QPointF CalibrationModel::transformPixelToPhysical(const QPointF &p) const
{
    if (!valid)
        return p;
    const QPointF shifted = p - originPx;
    const double c = qCos(rotationRad);
    const double s = qSin(rotationRad);
    const QPointF rotated(shifted.x() * c - shifted.y() * s,
                          shifted.x() * s + shifted.y() * c);
    return QPointF(rotated.x() * unitPerPixel, rotated.y() * unitPerPixel);
}

double CalibrationModel::transformLength(double pxLength) const
{
    if (!valid)
        return pxLength;
    return pxLength * unitPerPixel;
}

MeasurementResult CalibrationModel::applyTo(const MeasurementResult &pixelResult) const
{
    MeasurementResult out = pixelResult;
    if (!valid)
        return out;
    out.unit = unit;
    out.calibrationId = calibrationId;
    out.width = transformLength(pixelResult.width);
    out.height = transformLength(pixelResult.height);
    out.perimeter = transformLength(pixelResult.perimeter);
    out.area = pixelResult.area * unitPerPixel * unitPerPixel;
    out.center = transformPixelToPhysical(pixelResult.center);
    out.boundingRect = QRectF(transformPixelToPhysical(pixelResult.boundingRect.topLeft()),
                              QSizeF(out.width, out.height));
    return out;
}

QJsonObject CalibrationModel::toJson() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("calibrationId"), calibrationId);
    obj.insert(QStringLiteral("unit"), unit);
    obj.insert(QStringLiteral("unitPerPixel"), unitPerPixel);
    obj.insert(QStringLiteral("originX"), originPx.x());
    obj.insert(QStringLiteral("originY"), originPx.y());
    obj.insert(QStringLiteral("rotationRad"), rotationRad);
    obj.insert(QStringLiteral("valid"), valid);
    return obj;
}

CalibrationModel CalibrationModel::fromJson(const QJsonObject &obj)
{
    CalibrationModel m;
    m.calibrationId = obj.value(QStringLiteral("calibrationId")).toString();
    m.unit = obj.value(QStringLiteral("unit")).toString(QStringLiteral("px"));
    m.unitPerPixel = obj.value(QStringLiteral("unitPerPixel")).toDouble(1.0);
    m.originPx = QPointF(obj.value(QStringLiteral("originX")).toDouble(),
                         obj.value(QStringLiteral("originY")).toDouble());
    m.rotationRad = obj.value(QStringLiteral("rotationRad")).toDouble(0.0);
    m.valid = obj.value(QStringLiteral("valid")).toBool(false);
    return m;
}

