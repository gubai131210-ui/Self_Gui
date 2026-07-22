#ifndef CALIBRATIONMODEL_H
#define CALIBRATIONMODEL_H

#include "vision/model/measurementresult.h"

#include <QJsonObject>
#include <QPointF>
#include <QString>

/// Simple calibration model for pixel-to-physical conversion.
/// Supports unit-per-pixel scale, origin shift and rotation.
struct CalibrationModel
{
    QString calibrationId;
    QString unit{QStringLiteral("px")};
    /// Physical unit per pixel (e.g. mm per pixel).
    double unitPerPixel{1.0};
    QPointF originPx;
    double rotationRad{0.0};
    bool valid{false};

    QPointF transformPixelToPhysical(const QPointF &p) const;
    double transformLength(double pxLength) const;
    /// Apply scale to length-based fields; decision remains caller's responsibility.
    MeasurementResult applyTo(const MeasurementResult &pixelResult) const;

    QJsonObject toJson() const;
    static CalibrationModel fromJson(const QJsonObject &obj);
};

#endif // CALIBRATIONMODEL_H
