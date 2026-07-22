#ifndef CONTOURDATA_H
#define CONTOURDATA_H

#include "vision/model/regiondata.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QPointF>
#include <QVector>

struct ContourData
{
    QVector<QPointF> points;

    QJsonObject toJson() const
    {
        QJsonArray arr;
        for (const QPointF &p : points) {
            arr.append(QJsonObject{{QStringLiteral("x"), p.x()},
                                   {QStringLiteral("y"), p.y()}});
        }
        return QJsonObject{{QStringLiteral("points"), arr}};
    }

    static ContourData fromJson(const QJsonObject &obj)
    {
        ContourData c;
        const QJsonArray arr = obj.value(QStringLiteral("points")).toArray();
        for (const QJsonValue &v : arr) {
            const QJsonObject p = v.toObject();
            c.points.append(QPointF(p.value(QStringLiteral("x")).toDouble(),
                                    p.value(QStringLiteral("y")).toDouble()));
        }
        return c;
    }
};

struct ContourList
{
    QVector<ContourData> contours;

    QJsonObject toJson() const
    {
        QJsonArray arr;
        for (const ContourData &c : contours)
            arr.append(c.toJson());
        return QJsonObject{{QStringLiteral("contours"), arr}};
    }

    static ContourList fromJson(const QJsonObject &obj)
    {
        ContourList list;
        const QJsonArray arr = obj.value(QStringLiteral("contours")).toArray();
        for (const QJsonValue &v : arr)
            list.contours.append(ContourData::fromJson(v.toObject()));
        return list;
    }
};

#endif // CONTOURDATA_H
