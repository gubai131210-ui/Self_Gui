#include "portmodel.h"

static QString directionToString(PortDirection d)
{
    switch (d) {
    case PortDirection::Input:
        return QStringLiteral("input");
    case PortDirection::Output:
        return QStringLiteral("output");
    case PortDirection::Both:
    default:
        return QStringLiteral("both");
    }
}

static PortDirection directionFromString(const QString &s)
{
    if (s == QLatin1String("input"))
        return PortDirection::Input;
    if (s == QLatin1String("output"))
        return PortDirection::Output;
    return PortDirection::Both;
}

QJsonObject PortModel::toJson() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("id"), id);
    obj.insert(QStringLiteral("name"), name);
    obj.insert(QStringLiteral("direction"), directionToString(direction));
    if (!dataType.isEmpty())
        obj.insert(QStringLiteral("dataType"), dataType);
    obj.insert(QStringLiteral("relativeX"), relativeX);
    obj.insert(QStringLiteral("relativeY"), relativeY);
    return obj;
}

PortModel PortModel::fromJson(const QJsonObject &obj)
{
    PortModel port;
    port.id = obj.value(QStringLiteral("id")).toString();
    port.name = obj.value(QStringLiteral("name")).toString();
    port.direction = directionFromString(obj.value(QStringLiteral("direction")).toString());
    port.dataType = obj.value(QStringLiteral("dataType")).toString();
    port.relativeX = obj.value(QStringLiteral("relativeX")).toDouble(0.5);
    port.relativeY = obj.value(QStringLiteral("relativeY")).toDouble(0.5);
    return port;
}
