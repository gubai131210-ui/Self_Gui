#ifndef MEASUREMENTRESULT_H
#define MEASUREMENTRESULT_H

#include <QJsonObject>
#include <QPointF>
#include <QRectF>
#include <QString>

struct MeasurementResult
{
    bool valid{false};
    /// Unit for length-based fields (width/height) and derived area unit.
    /// Default keeps legacy behavior: pixels ("px").
    QString unit{QStringLiteral("px")};
    /// Confidence in [0,1]. For now algorithms use 1.0 unless a future model computes otherwise.
    double confidence{1.0};
    /// Logical measurement type (e.g. "rectangle", "circle").
    QString measurementType;
    /// Originating node id for traceability. Set by runtime execution.
    QString sourceNodeId;
    /// Decision state derived from tolerance evaluation (e.g. "unknown", "ok", "ng").
    QString decisionState{QStringLiteral("unknown")};
    /// Optional trace ids for calibration/tolerance used during this measurement.
    QString calibrationId;
    QString toleranceId;

    double area{0.0};
    double perimeter{0.0};
    QRectF boundingRect;
    QPointF center;
    double width{0.0};
    double height{0.0};
    double angle{0.0};
    QString message;

    QJsonObject toJson() const;
    static MeasurementResult fromJson(const QJsonObject &obj);
};

/// 标准化检测结果（与 MeasurementResult 兼容，便于 OK/NG 闭环）。
struct InspectionResult
{
    bool ok{false};
    QString state{QStringLiteral("unknown")}; // ok | ng | unknown
    double value{0.0};
    QString message;
    MeasurementResult measurement;

    QJsonObject toJson() const
    {
        return QJsonObject{
            {QStringLiteral("ok"), ok},
            {QStringLiteral("state"), state},
            {QStringLiteral("value"), value},
            {QStringLiteral("message"), message},
            {QStringLiteral("measurement"), measurement.toJson()},
        };
    }

    static InspectionResult fromDecision(bool isOk, double value, const QString &message = {})
    {
        InspectionResult r;
        r.ok = isOk;
        r.state = isOk ? QStringLiteral("ok") : QStringLiteral("ng");
        r.value = value;
        r.message = message;
        r.measurement.valid = true;
        r.measurement.decisionState = r.state;
        r.measurement.width = value;
        r.measurement.message = message;
        r.measurement.measurementType = QStringLiteral("inspection");
        return r;
    }
};

#endif // MEASUREMENTRESULT_H
