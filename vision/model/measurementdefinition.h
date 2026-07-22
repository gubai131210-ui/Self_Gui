#ifndef MEASUREMENTDEFINITION_H
#define MEASUREMENTDEFINITION_H

#include <QJsonObject>
#include <QString>

/// Domain definition for a single measured quantity:
/// - unit and type (e.g. "length", "diameter", "rectangle.width")
/// - tolerance bands for OK/NG evaluation
struct MeasurementDefinition
{
    QString definitionId;
    QString measurementType; // e.g. "rectangle.width" / "diameter" / "distance"
    QString unit{QStringLiteral("px")};

    /// Nominal and tolerance band. When lower/upper are unset, evaluation is disabled.
    double nominal{0.0};
    double lower{0.0};
    double upper{0.0};
    bool hasTolerance{false};

    /// Evaluate state by value; returns "ok"/"ng"/"unknown".
    QString evaluateDecision(double value) const;

    QJsonObject toJson() const;
    static MeasurementDefinition fromJson(const QJsonObject &obj);
};

#endif // MEASUREMENTDEFINITION_H

