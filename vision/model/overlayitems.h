#ifndef OVERLAYITEMS_H
#define OVERLAYITEMS_H

#include <QColor>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector>

enum class OverlayItemType {
    Rectangle,
    Contour,
    Text,
    Line
};

struct OverlayItem
{
    OverlayItemType type{OverlayItemType::Rectangle};
    QRectF rect;
    QVector<QPointF> points;
    QString text;
    QColor color{0, 220, 80};
    qreal penWidth{2.0};

    QJsonObject toJson() const
    {
        QJsonObject obj;
        obj.insert(QStringLiteral("type"), static_cast<int>(type));
        obj.insert(QStringLiteral("x"), rect.x());
        obj.insert(QStringLiteral("y"), rect.y());
        obj.insert(QStringLiteral("width"), rect.width());
        obj.insert(QStringLiteral("height"), rect.height());
        obj.insert(QStringLiteral("text"), text);
        obj.insert(QStringLiteral("color"), color.name(QColor::HexArgb));
        obj.insert(QStringLiteral("penWidth"), penWidth);
        QJsonArray pts;
        for (const QPointF &p : points) {
            pts.append(QJsonObject{{QStringLiteral("x"), p.x()},
                                   {QStringLiteral("y"), p.y()}});
        }
        obj.insert(QStringLiteral("points"), pts);
        return obj;
    }

    static OverlayItem fromJson(const QJsonObject &obj)
    {
        OverlayItem item;
        item.type = static_cast<OverlayItemType>(obj.value(QStringLiteral("type")).toInt());
        item.rect = QRectF(obj.value(QStringLiteral("x")).toDouble(),
                           obj.value(QStringLiteral("y")).toDouble(),
                           obj.value(QStringLiteral("width")).toDouble(),
                           obj.value(QStringLiteral("height")).toDouble());
        item.text = obj.value(QStringLiteral("text")).toString();
        item.color = QColor(obj.value(QStringLiteral("color")).toString(QStringLiteral("#00DC50")));
        item.penWidth = obj.value(QStringLiteral("penWidth")).toDouble(2.0);
        const QJsonArray pts = obj.value(QStringLiteral("points")).toArray();
        for (const QJsonValue &v : pts) {
            const QJsonObject p = v.toObject();
            item.points.append(QPointF(p.value(QStringLiteral("x")).toDouble(),
                                       p.value(QStringLiteral("y")).toDouble()));
        }
        return item;
    }
};

using OverlayList = QVector<OverlayItem>;

#endif // OVERLAYITEMS_H
