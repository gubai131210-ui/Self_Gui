#ifndef REGIONDATA_H
#define REGIONDATA_H

#include "vision/model/visionimage.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QPointF>
#include <QRectF>
#include <QStringList>
#include <QVariant>
#include <QVariantList>
#include <QVector>

struct RegionStats
{
    int label{0};
    QPointF centroid;
    double area{0.0};
    double perimeter{0.0};
    double circularity{0.0};
    double elongation{0.0}; // major/minor principal inertia (>=1)
    double aspectRatio{1.0};
    double extent{0.0};
    double solidity{0.0};
    double width{0.0};
    double height{0.0};
    QRectF boundingRect;

    QJsonObject toJson() const
    {
        return QJsonObject{
            {QStringLiteral("label"), label},
            {QStringLiteral("cx"), centroid.x()},
            {QStringLiteral("cy"), centroid.y()},
            {QStringLiteral("area"), area},
            {QStringLiteral("perimeter"), perimeter},
            {QStringLiteral("circularity"), circularity},
            {QStringLiteral("elongation"), elongation},
            {QStringLiteral("aspectRatio"), aspectRatio},
            {QStringLiteral("extent"), extent},
            {QStringLiteral("solidity"), solidity},
            {QStringLiteral("width"), width},
            {QStringLiteral("height"), height},
            {QStringLiteral("bx"), boundingRect.x()},
            {QStringLiteral("by"), boundingRect.y()},
            {QStringLiteral("bw"), boundingRect.width()},
            {QStringLiteral("bh"), boundingRect.height()},
        };
    }

    static RegionStats fromJson(const QJsonObject &obj)
    {
        RegionStats s;
        s.label = obj.value(QStringLiteral("label")).toInt();
        s.centroid = QPointF(obj.value(QStringLiteral("cx")).toDouble(),
                             obj.value(QStringLiteral("cy")).toDouble());
        s.area = obj.value(QStringLiteral("area")).toDouble();
        s.perimeter = obj.value(QStringLiteral("perimeter")).toDouble();
        s.circularity = obj.value(QStringLiteral("circularity")).toDouble();
        s.elongation = obj.value(QStringLiteral("elongation")).toDouble(1.0);
        s.aspectRatio = obj.value(QStringLiteral("aspectRatio")).toDouble(1.0);
        s.extent = obj.value(QStringLiteral("extent")).toDouble();
        s.solidity = obj.value(QStringLiteral("solidity")).toDouble();
        s.width = obj.value(QStringLiteral("width")).toDouble();
        s.height = obj.value(QStringLiteral("height")).toDouble();
        s.boundingRect = QRectF(obj.value(QStringLiteral("bx")).toDouble(),
                                obj.value(QStringLiteral("by")).toDouble(),
                                obj.value(QStringLiteral("bw")).toDouble(),
                                obj.value(QStringLiteral("bh")).toDouble());
        return s;
    }
};

struct RegionData
{
    VisionImage labelImage;
    QVector<RegionStats> regions;
    int labelCount{0};

    QJsonObject toJson() const
    {
        QJsonArray arr;
        for (const RegionStats &s : regions)
            arr.append(s.toJson());
        return QJsonObject{
            {QStringLiteral("labelCount"), labelCount},
            {QStringLiteral("regions"), arr},
            {QStringLiteral("hasLabelImage"), !labelImage.isEmpty()},
            {QStringLiteral("labelWidth"), labelImage.isEmpty() ? 0 : labelImage.width()},
            {QStringLiteral("labelHeight"), labelImage.isEmpty() ? 0 : labelImage.height()},
        };
    }

    static RegionData fromJson(const QJsonObject &obj)
    {
        RegionData d;
        d.labelCount = obj.value(QStringLiteral("labelCount")).toInt();
        const QJsonArray arr = obj.value(QStringLiteral("regions")).toArray();
        for (const QJsonValue &v : arr)
            d.regions.append(RegionStats::fromJson(v.toObject()));
        // Pixel label map is runtime-only; JSON restores metadata without mat data.
        return d;
    }
};

struct TableData
{
    QStringList columns;
    QVector<QVariantList> rows;

    QJsonObject toJson() const
    {
        QJsonArray cols;
        for (const QString &c : columns)
            cols.append(c);
        QJsonArray rowArr;
        for (const QVariantList &row : rows) {
            QJsonArray cells;
            for (const QVariant &cell : row)
                cells.append(QJsonValue::fromVariant(cell));
            rowArr.append(cells);
        }
        return QJsonObject{
            {QStringLiteral("columns"), cols},
            {QStringLiteral("rows"), rowArr},
        };
    }

    static TableData fromJson(const QJsonObject &obj)
    {
        TableData t;
        const QJsonArray cols = obj.value(QStringLiteral("columns")).toArray();
        for (const QJsonValue &v : cols)
            t.columns.append(v.toString());
        const QJsonArray rows = obj.value(QStringLiteral("rows")).toArray();
        for (const QJsonValue &rv : rows) {
            QVariantList row;
            for (const QJsonValue &cell : rv.toArray())
                row.append(cell.toVariant());
            t.rows.append(row);
        }
        return t;
    }
};

#endif // REGIONDATA_H
