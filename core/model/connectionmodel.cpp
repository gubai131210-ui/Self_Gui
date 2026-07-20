#include "connectionmodel.h"

static QString lineStyleToString(ConnectionLineStyle style)
{
    return style == ConnectionLineStyle::Straight
        ? QStringLiteral("straight")
        : QStringLiteral("orthogonal");
}

static ConnectionLineStyle lineStyleFromString(const QString &s)
{
    if (s == QLatin1String("straight"))
        return ConnectionLineStyle::Straight;
    return ConnectionLineStyle::Orthogonal;
}

QJsonObject ConnectionModel::toJson() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("id"), id);
    obj.insert(QStringLiteral("sourceNodeId"), sourceNodeId);
    obj.insert(QStringLiteral("sourcePortId"), sourcePortId);
    obj.insert(QStringLiteral("targetNodeId"), targetNodeId);
    obj.insert(QStringLiteral("targetPortId"), targetPortId);
    obj.insert(QStringLiteral("label"), label);
    obj.insert(QStringLiteral("lineStyle"), lineStyleToString(lineStyle));
    obj.insert(QStringLiteral("showArrow"), showArrow);
    return obj;
}

ConnectionModel ConnectionModel::fromJson(const QJsonObject &obj)
{
    ConnectionModel c;
    c.id = obj.value(QStringLiteral("id")).toString();
    c.sourceNodeId = obj.value(QStringLiteral("sourceNodeId")).toString();
    c.sourcePortId = obj.value(QStringLiteral("sourcePortId")).toString();
    c.targetNodeId = obj.value(QStringLiteral("targetNodeId")).toString();
    c.targetPortId = obj.value(QStringLiteral("targetPortId")).toString();
    c.label = obj.value(QStringLiteral("label")).toString();
    c.lineStyle = lineStyleFromString(obj.value(QStringLiteral("lineStyle")).toString());
    c.showArrow = obj.value(QStringLiteral("showArrow")).toBool(true);
    return c;
}
