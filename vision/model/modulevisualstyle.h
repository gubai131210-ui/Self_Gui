#ifndef MODULEVISUALSTYLE_H
#define MODULEVISUALSTYLE_H

#include "vision/data/datatype.h"

#include <QColor>
#include <QString>

namespace Selt {

/// Shared category / data-type colors for canvas, palette and ports.
/// Color is an aid only; text labels remain required for accessibility.
inline QColor accentColorForCategory(const QString &category)
{
    if (category == QStringLiteral("输入"))
        return QColor(64, 150, 230);
    if (category == QStringLiteral("预处理") || category == QStringLiteral("区域"))
        return QColor(40, 175, 160);
    if (category == QStringLiteral("定位"))
        return QColor(255, 150, 40);
    if (category == QStringLiteral("测量"))
        return QColor(240, 160, 50);
    if (category == QStringLiteral("输出") || category == QStringLiteral("判定"))
        return QColor(150, 100, 210);
    if (category == QStringLiteral("插件"))
        return QColor(120, 110, 190);
    if (category == QStringLiteral("逻辑") || category == QStringLiteral("数据"))
        return QColor(110, 140, 160);
    if (category == QStringLiteral("通讯"))
        return QColor(70, 170, 120);
    return QColor(200, 110, 20);
}

inline QColor fillColorForCategory(const QString &category)
{
    Q_UNUSED(category);
    return QColor(45, 48, 54);
}

inline QColor colorForDataType(DataTypeId type)
{
    switch (type) {
    case DataTypeId::Image:
        return QColor(64, 150, 230);
    case DataTypeId::Measurement:
    case DataTypeId::Real:
    case DataTypeId::Int:
    case DataTypeId::Bool:
        return QColor(255, 140, 0);
    case DataTypeId::Overlay:
        return QColor(170, 110, 220);
    case DataTypeId::Roi:
    case DataTypeId::Rect:
    case DataTypeId::Circle:
    case DataTypeId::Contour:
    case DataTypeId::Region:
        return QColor(56, 180, 90);
    case DataTypeId::Point2D:
        return QColor(40, 190, 170);
    case DataTypeId::Line:
        return QColor(90, 200, 120);
    case DataTypeId::String:
    case DataTypeId::Table:
    case DataTypeId::ByteArray:
        return QColor(140, 150, 165);
    case DataTypeId::None:
    default:
        return QColor(120, 130, 145);
    }
}

inline QString helpTextOrDescription(const QString &helpText, const QString &description)
{
    return helpText.isEmpty() ? description : helpText;
}

} // namespace Selt

#endif // MODULEVISUALSTYLE_H
