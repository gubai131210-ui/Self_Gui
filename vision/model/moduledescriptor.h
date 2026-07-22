#ifndef MODULEDESCRIPTOR_H
#define MODULEDESCRIPTOR_H

#include "moduleparamdef.h"
#include "moduleuischema.h"
#include "interactivegeometry.h"
#include "vision/data/datatype.h"

#include <QColor>
#include <QSizeF>
#include <QString>
#include <QStringList>
#include <QVector>

namespace Selt {
enum class PortRole {
    Primary, ///< Main data-flow ports (usually Image / required inputs)
    Result,  ///< Business outputs (measurement, text, found, ...)
    Debug    ///< Overlay / diagnostics / verbose ports
};
} // namespace Selt

struct ModulePortDef
{
    QString id;
    QString name;
    bool isInput{true};
    Selt::DataTypeId dataType{Selt::DataTypeId::Image};
    bool required{true};
    QString description;
    /// Display group: Image / ROI / Geometry / Measurement / Debug / ...
    QString group;
    Selt::PortRole role{Selt::PortRole::Primary};
    /// Suggested canvas visibility for built-in descriptors with exposure hints.
    bool defaultExposed{true};
    bool exposable{true};

    Selt::TypedPortDef toTypedPortDef() const
    {
        Selt::TypedPortDef def;
        def.id = id;
        def.name = name;
        def.isInput = isInput;
        def.dataType = dataType;
        def.required = required;
        def.description = description;
        return def;
    }
};

struct ModuleDescriptor
{
    QString typeId;
    QString displayName;
    QString category;
    QString description;
    /// Short hover/help text; falls back to description when empty.
    QString helpText;
    QString iconKey;
    QSizeF fixedSize{160, 72};
    QColor fillColor{45, 48, 54};
    QColor borderColor{255, 140, 0};
    QColor accentColor{255, 140, 0};
    QVector<ModulePortDef> ports;
    QVector<ModuleParamDef> parameters;
    QStringList inputKeys;
    QStringList outputKeys;
    /// Optional declarative UI hints (editor, preview layers, capability tags).
    Selt::ModuleUiSchema uiSchema;
    /// Declarative pre-execution geometry editor configuration.
    Selt::InteractiveGeometrySpec interactiveGeometry;
    /// When false (plugins without hints), canvas exposes all ports for compatibility.
    bool hasExposureHints{false};
    /// Plugin/descriptor capability version. 0 = legacy (full expose unless hints set).
    int capabilityVersion{0};

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

    const ModulePortDef *findInputPort(const QString &portId) const
    {
        for (const ModulePortDef &port : ports) {
            if (port.isInput && port.id == portId)
                return &port;
        }
        return nullptr;
    }

    const ModulePortDef *findOutputPort(const QString &portId) const
    {
        for (const ModulePortDef &port : ports) {
            if (!port.isInput && port.id == portId)
                return &port;
        }
        return nullptr;
    }

    const ModulePortDef *findPort(const QString &portId) const
    {
        for (const ModulePortDef &port : ports) {
            if (port.id == portId)
                return &port;
        }
        return nullptr;
    }

    /// Resolve semantic/runtime key ("image") to a declared port, with in/out aliases.
    const ModulePortDef *resolvePortAlias(const QString &key, bool wantInput) const
    {
        if (const ModulePortDef *direct = wantInput ? findInputPort(key) : findOutputPort(key))
            return direct;
        if (key == QLatin1String("image")) {
            if (wantInput) {
                if (const ModulePortDef *p = findInputPort(QStringLiteral("in")))
                    return p;
            } else if (const ModulePortDef *p = findOutputPort(QStringLiteral("out"))) {
                return p;
            }
        }
        if ((key == QLatin1String("in") || key == QLatin1String("out"))) {
            for (const ModulePortDef &port : ports) {
                if (port.isInput == wantInput && port.dataType == Selt::DataTypeId::Image)
                    return &port;
            }
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
