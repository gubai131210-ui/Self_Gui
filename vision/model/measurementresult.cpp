#include "measurementresult.h"

QJsonObject MeasurementResult::toJson() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("valid"), valid);
    obj.insert(QStringLiteral("unit"), unit);
    obj.insert(QStringLiteral("confidence"), confidence);
    obj.insert(QStringLiteral("measurementType"), measurementType);
    obj.insert(QStringLiteral("sourceNodeId"), sourceNodeId);
    obj.insert(QStringLiteral("decisionState"), decisionState);
    obj.insert(QStringLiteral("calibrationId"), calibrationId);
    obj.insert(QStringLiteral("toleranceId"), toleranceId);
    obj.insert(QStringLiteral("area"), area);
    obj.insert(QStringLiteral("perimeter"), perimeter);
    obj.insert(QStringLiteral("x"), boundingRect.x());
    obj.insert(QStringLiteral("y"), boundingRect.y());
    obj.insert(QStringLiteral("width"), boundingRect.width());
    obj.insert(QStringLiteral("height"), boundingRect.height());
    obj.insert(QStringLiteral("centerX"), center.x());
    obj.insert(QStringLiteral("centerY"), center.y());
    obj.insert(QStringLiteral("message"), message);
    return obj;
}

MeasurementResult MeasurementResult::fromJson(const QJsonObject &obj)
{
    MeasurementResult r;
    r.valid = obj.value(QStringLiteral("valid")).toBool(false);
    r.unit = obj.value(QStringLiteral("unit")).toString(QStringLiteral("px"));
    r.confidence = obj.value(QStringLiteral("confidence")).toDouble(1.0);
    r.measurementType = obj.value(QStringLiteral("measurementType")).toString();
    r.sourceNodeId = obj.value(QStringLiteral("sourceNodeId")).toString();
    r.decisionState = obj.value(QStringLiteral("decisionState")).toString(QStringLiteral("unknown"));
    r.calibrationId = obj.value(QStringLiteral("calibrationId")).toString();
    r.toleranceId = obj.value(QStringLiteral("toleranceId")).toString();
    r.area = obj.value(QStringLiteral("area")).toDouble(0.0);
    r.perimeter = obj.value(QStringLiteral("perimeter")).toDouble(0.0);
    r.boundingRect = QRectF(obj.value(QStringLiteral("x")).toDouble(),
                            obj.value(QStringLiteral("y")).toDouble(),
                            obj.value(QStringLiteral("width")).toDouble(),
                            obj.value(QStringLiteral("height")).toDouble());
    r.center = QPointF(obj.value(QStringLiteral("centerX")).toDouble(),
                       obj.value(QStringLiteral("centerY")).toDouble());
    r.width = r.boundingRect.width();
    r.height = r.boundingRect.height();
    r.message = obj.value(QStringLiteral("message")).toString();
    return r;
}
