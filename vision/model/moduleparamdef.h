#ifndef MODULEPARAMDEF_H
#define MODULEPARAMDEF_H

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>

enum class ModuleParamType {
    String,
    FilePath,
    Int,
    Double,
    Bool,
    Enum,
    Color,
    RoiRect
};

struct ModuleParamOption
{
    QString value;
    QString label;
};

/// How an unset/empty parameter should be interpreted at runtime.
enum class ModuleParamAutoPolicy {
    None,       ///< Always use stored / default value.
    Auto,       ///< Unset/sentinel resolves via AutoParameterResolver.
    Unlimited   ///< 0 / negative means no upper bound (filters).
};

struct ModuleParamDef
{
    QString key;
    QString label;
    ModuleParamType type{ModuleParamType::String};
    QVariant defaultValue;
    QVariant minimum;
    QVariant maximum;
    int decimals{2};
    QStringList enumValues;
    QVector<ModuleParamOption> enumOptions;
    QString fileFilter;
    QString tooltip;
    /// Optional inspector group title (e.g. "采样", "阈值", "ROI").
    QString group;
    int displayOrder{0};
    /// When Auto, inspector may show "Auto/不限" and omit writing until user edits.
    ModuleParamAutoPolicy autoPolicy{ModuleParamAutoPolicy::None};
    /// Optional display placeholder for Auto/Unset (e.g. "Auto", "不限").
    QString autoPlaceholder;

    QJsonValue defaultJsonValue() const
    {
        switch (type) {
        case ModuleParamType::Bool:
            return defaultValue.toBool();
        case ModuleParamType::Int:
            return defaultValue.toInt();
        case ModuleParamType::Double:
            return defaultValue.toDouble();
        case ModuleParamType::RoiRect: {
            const QJsonObject roi = defaultValue.toJsonObject();
            return roi.isEmpty() ? QJsonObject{
                                       {QStringLiteral("x"), 0},
                                       {QStringLiteral("y"), 0},
                                       {QStringLiteral("width"), 0},
                                       {QStringLiteral("height"), 0},
                                       {QStringLiteral("enabled"), false}}
                                 : QJsonValue(roi);
        }
        case ModuleParamType::Enum:
        case ModuleParamType::String:
        case ModuleParamType::FilePath:
        case ModuleParamType::Color:
        default:
            return defaultValue.toString();
        }
    }

    bool validate(const QJsonValue &value, QString *error) const
    {
        switch (type) {
        case ModuleParamType::Int: {
            if (!value.isDouble() && !value.isString()) {
                if (error)
                    *error = QStringLiteral("%1 必须是整数").arg(label);
                return false;
            }
            const int v = value.toInt();
            if (minimum.isValid() && v < minimum.toInt()) {
                if (error)
                    *error = QStringLiteral("%1 不能小于 %2").arg(label).arg(minimum.toInt());
                return false;
            }
            if (maximum.isValid() && v > maximum.toInt()) {
                if (error)
                    *error = QStringLiteral("%1 不能大于 %2").arg(label).arg(maximum.toInt());
                return false;
            }
            return true;
        }
        case ModuleParamType::Double: {
            if (!value.isDouble() && !value.isString()) {
                if (error)
                    *error = QStringLiteral("%1 必须是数值").arg(label);
                return false;
            }
            const double v = value.toDouble();
            if (minimum.isValid() && v < minimum.toDouble()) {
                if (error)
                    *error = QStringLiteral("%1 不能小于 %2").arg(label).arg(minimum.toDouble());
                return false;
            }
            if (maximum.isValid() && v > maximum.toDouble()) {
                if (error)
                    *error = QStringLiteral("%1 不能大于 %2").arg(label).arg(maximum.toDouble());
                return false;
            }
            return true;
        }
        case ModuleParamType::Enum: {
            const QString text = value.toString();
            if (!enumValues.contains(text)) {
                if (error)
                    *error = QStringLiteral("%1 取值无效").arg(label);
                return false;
            }
            return true;
        }
        case ModuleParamType::FilePath:
        case ModuleParamType::String:
            return true;
        case ModuleParamType::Bool:
            return true;
        case ModuleParamType::RoiRect:
            return value.isObject() || value.isNull();
        case ModuleParamType::Color:
            return true;
        }
        return true;
    }
};

#endif // MODULEPARAMDEF_H
