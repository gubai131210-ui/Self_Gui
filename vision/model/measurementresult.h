#ifndef MEASUREMENTRESULT_H
#define MEASUREMENTRESULT_H

#include <QJsonObject>
#include <QPointF>
#include <QRectF>
#include <QString>

struct MeasurementResult
{
    bool valid{false};
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

#endif // MEASUREMENTRESULT_H
