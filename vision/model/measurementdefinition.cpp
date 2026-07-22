#include "measurementdefinition.h"

QString MeasurementDefinition::evaluateDecision(double value) const
{
    if (!hasTolerance)
        return QStringLiteral("unknown");
    if (value >= lower && value <= upper)
        return QStringLiteral("ok");
    return QStringLiteral("ng");
}

QJsonObject MeasurementDefinition::toJson() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("definitionId"), definitionId);
    obj.insert(QStringLiteral("measurementType"), measurementType);
    obj.insert(QStringLiteral("unit"), unit);
    obj.insert(QStringLiteral("nominal"), nominal);
    obj.insert(QStringLiteral("lower"), lower);
    obj.insert(QStringLiteral("upper"), upper);
    obj.insert(QStringLiteral("hasTolerance"), hasTolerance);
    return obj;
}

MeasurementDefinition MeasurementDefinition::fromJson(const QJsonObject &obj)
{
    MeasurementDefinition d;
    d.definitionId = obj.value(QStringLiteral("definitionId")).toString();
    d.measurementType = obj.value(QStringLiteral("measurementType")).toString();
    d.unit = obj.value(QStringLiteral("unit")).toString(QStringLiteral("px"));
    d.nominal = obj.value(QStringLiteral("nominal")).toDouble(0.0);
    d.lower = obj.value(QStringLiteral("lower")).toDouble(0.0);
    d.upper = obj.value(QStringLiteral("upper")).toDouble(0.0);
    d.hasTolerance = obj.value(QStringLiteral("hasTolerance")).toBool(false);
    return d;
}

