#include "nodestyle.h"

#include <QJsonValue>

static QString colorToString(const QColor &c)
{
    return c.name(QColor::HexArgb);
}

static QColor colorFromString(const QString &s, const QColor &fallback)
{
    QColor c(s);
    return c.isValid() ? c : fallback;
}

QJsonObject NodeStyle::toJson() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("fillColor"), colorToString(fillColor));
    obj.insert(QStringLiteral("borderColor"), colorToString(borderColor));
    obj.insert(QStringLiteral("borderWidth"), borderWidth);
    obj.insert(QStringLiteral("textColor"), colorToString(textColor));
    obj.insert(QStringLiteral("fontSize"), fontSize);
    obj.insert(QStringLiteral("cornerRadius"), cornerRadius);
    return obj;
}

NodeStyle NodeStyle::fromJson(const QJsonObject &obj)
{
    NodeStyle style;
    style.fillColor = colorFromString(obj.value(QStringLiteral("fillColor")).toString(), style.fillColor);
    style.borderColor = colorFromString(obj.value(QStringLiteral("borderColor")).toString(), style.borderColor);
    style.borderWidth = obj.value(QStringLiteral("borderWidth")).toDouble(style.borderWidth);
    style.textColor = colorFromString(obj.value(QStringLiteral("textColor")).toString(), style.textColor);
    style.fontSize = obj.value(QStringLiteral("fontSize")).toInt(style.fontSize);
    style.cornerRadius = obj.value(QStringLiteral("cornerRadius")).toDouble(style.cornerRadius);
    return style;
}
