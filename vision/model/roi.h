#ifndef ROI_H
#define ROI_H

#include <QJsonObject>
#include <QRectF>
#include <QString>

struct RoiRect
{
    QString id;
    QRectF rect;
    bool enabled{false};
    bool locked{false};

    bool isValid() const
    {
        return enabled && rect.width() > 0.0 && rect.height() > 0.0;
    }

    QJsonObject toJson() const
    {
        return QJsonObject{
            {QStringLiteral("id"), id},
            {QStringLiteral("x"), rect.x()},
            {QStringLiteral("y"), rect.y()},
            {QStringLiteral("width"), rect.width()},
            {QStringLiteral("height"), rect.height()},
            {QStringLiteral("enabled"), enabled},
            {QStringLiteral("locked"), locked}};
    }

    static RoiRect fromJson(const QJsonObject &obj)
    {
        RoiRect roi;
        roi.id = obj.value(QStringLiteral("id")).toString();
        roi.rect = QRectF(obj.value(QStringLiteral("x")).toDouble(),
                          obj.value(QStringLiteral("y")).toDouble(),
                          obj.value(QStringLiteral("width")).toDouble(),
                          obj.value(QStringLiteral("height")).toDouble());
        roi.enabled = obj.value(QStringLiteral("enabled")).toBool(false);
        roi.locked = obj.value(QStringLiteral("locked")).toBool(false);
        return roi;
    }
};

#endif // ROI_H
