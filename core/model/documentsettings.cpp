#include "documentsettings.h"

QJsonObject DocumentSettings::toJson() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("gridSize"), gridSize);
    obj.insert(QStringLiteral("snapToGrid"), snapToGrid);
    obj.insert(QStringLiteral("showGrid"), showGrid);
    obj.insert(QStringLiteral("backgroundColor"), backgroundColor.name(QColor::HexArgb));
    return obj;
}

DocumentSettings DocumentSettings::fromJson(const QJsonObject &obj)
{
    DocumentSettings s;
    s.gridSize = obj.value(QStringLiteral("gridSize")).toInt(20);
    s.snapToGrid = obj.value(QStringLiteral("snapToGrid")).toBool(true);
    s.showGrid = obj.value(QStringLiteral("showGrid")).toBool(true);
    const QColor bg(obj.value(QStringLiteral("backgroundColor")).toString());
    if (bg.isValid())
        s.backgroundColor = bg;
    return s;
}
