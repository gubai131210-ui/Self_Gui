#ifndef MODULEDESCRIPTOR_H
#define MODULEDESCRIPTOR_H

#include "moduleparamdef.h"

#include <QColor>
#include <QSizeF>
#include <QString>
#include <QStringList>
#include <QVector>

struct ModulePortDef
{
    QString id;
    QString name;
    bool isInput{true};
};

struct ModuleDescriptor
{
    QString typeId;
    QString displayName;
    QString category;
    QString description;
    QSizeF fixedSize{160, 72};
    QColor fillColor{235, 248, 240};
    QColor borderColor{40, 140, 90};
    QVector<ModulePortDef> ports;
    QVector<ModuleParamDef> parameters;
    QStringList inputKeys;
    QStringList outputKeys;

    QJsonObject defaultParameters() const
    {
        QJsonObject obj;
        for (const ModuleParamDef &param : parameters)
            obj.insert(param.key, param.defaultJsonValue());
        return obj;
    }

    const ModuleParamDef *findParam(const QString &key) const
    {
        for (const ModuleParamDef &param : parameters) {
            if (param.key == key)
                return &param;
        }
        return nullptr;
    }

    bool validateParameters(const QJsonObject &params, QString *error) const
    {
        for (const ModuleParamDef &param : parameters) {
            const QJsonValue value = params.contains(param.key)
                ? params.value(param.key)
                : param.defaultJsonValue();
            if (!param.validate(value, error))
                return false;
        }
        return true;
    }
};

#endif // MODULEDESCRIPTOR_H
