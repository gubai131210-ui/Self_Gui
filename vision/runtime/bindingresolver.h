#ifndef BINDINGRESOLVER_H
#define BINDINGRESOLVER_H

#include "core/model/nodemodel.h"
#include "vision/data/datatype.h"
#include "vision/domain/projectvariables.h"
#include "vision/domain/variablescope.h"
#include "vision/model/moduleparamdef.h"
#include "vision/runtime/portvaluestore.h"

#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace Selt {

struct BindingResolveResult
{
    bool ok{false};
    DataValue value;
    QString error;
    QString diagnosticCode;
};

class BindingResolver
{
public:
    static BindingResolveResult resolve(const ParameterBinding &binding,
                                        DataTypeId targetType,
                                        const PortValueStore &ports,
                                        const ProjectVariableStore &variables);

    static BindingResolveResult resolve(const ParameterBinding &binding,
                                        DataTypeId targetType,
                                        const PortValueStore &ports,
                                        const VariableScopeChain &scopes);

    static ParameterBinding bindingForParameter(const NodeModel &node,
                                                const ModuleParamDef &param);

    /// Build legacy JSON parameters after resolving all bindings for a node.
    static bool resolveNodeParameters(const NodeModel &node,
                                      const QVector<ModuleParamDef> &paramDefs,
                                      const PortValueStore &ports,
                                      const ProjectVariableStore &variables,
                                      QJsonObject *legacyJson,
                                      QHash<QString, DataValue> *typed,
                                      QString *error,
                                      QString *failedKey = nullptr);

    static bool resolveNodeParameters(const NodeModel &node,
                                      const QVector<ModuleParamDef> &paramDefs,
                                      const PortValueStore &ports,
                                      const VariableScopeChain &scopes,
                                      QJsonObject *legacyJson,
                                      QHash<QString, DataValue> *typed,
                                      QString *error,
                                      QString *failedKey = nullptr);
};

} // namespace Selt

#endif // BINDINGRESOLVER_H
