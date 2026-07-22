#include "vision/runtime/bindingresolver.h"

#include "core/model/nodemodel.h"
#include "vision/domain/variablescope.h"
#include "vision/model/moduleparamdef.h"

namespace Selt {

BindingResolveResult BindingResolver::resolve(const ParameterBinding &binding,
                                              DataTypeId targetType,
                                              const PortValueStore &ports,
                                              const ProjectVariableStore &variables)
{
    VariableScopeChain chain;
    chain.setProject(&variables);
    return resolve(binding, targetType, ports, chain);
}

BindingResolveResult BindingResolver::resolve(const ParameterBinding &binding,
                                              DataTypeId targetType,
                                              const PortValueStore &ports,
                                              const VariableScopeChain &scopes)
{
    BindingResolveResult result;
    const DataTypeId expected = binding.targetType != DataTypeId::None ? binding.targetType : targetType;

    switch (binding.kind) {
    case ParameterSourceKind::Constant: {
        QString err;
        DataValue value;
        if (binding.constantValue.contains(QStringLiteral("type")))
            value = DataValue::fromJson(binding.constantValue, &err);
        else
            value = DataValue::fromParameterJson(binding.constantValue, expected, &err);
        if (value.isNull() && !err.isEmpty()) {
            result.error = err;
            return result;
        }
        if (expected != DataTypeId::None && !value.isNull()) {
            DataValue converted;
            if (!value.convertTo(expected, &converted, &err)) {
                if (!value.isValidFor(expected)) {
                    result.error = err;
                    return result;
                }
                converted = value;
            }
            value = converted;
        }
        result.ok = true;
        result.value = value;
        return result;
    }
    case ParameterSourceKind::UpstreamBinding: {
        if (!ports.containsAliased(binding.upstreamNodeId, binding.upstreamPortId)) {
            result.error = QStringLiteral("上游节点尚未执行或没有该输出: %1.%2")
                               .arg(binding.upstreamNodeId, binding.upstreamPortId);
            return result;
        }
        DataValue value = ports.valueAliased(binding.upstreamNodeId, binding.upstreamPortId);
        if (expected != DataTypeId::None) {
            DataValue converted;
            QString err;
            if (!value.convertTo(expected, &converted, &err)) {
                if (!value.isValidFor(expected) && !dataTypesCompatible(value.type(), expected)) {
                    result.error = err;
                    return result;
                }
                converted = value;
            } else {
                value = converted;
            }
        }
        result.ok = true;
        result.value = value;
        return result;
    }
    case ParameterSourceKind::ProjectVariable: {
        if (!scopes.containsId(binding.projectVariableId)
            && !scopes.containsRuntimeKey(binding.projectVariableId)) {
            result.error = QStringLiteral("作用域变量不存在: %1").arg(binding.projectVariableId);
            result.diagnosticCode = QStringLiteral("variable_undefined");
            return result;
        }
        DataValue value;
        if (scopes.containsRuntimeKey(binding.projectVariableId)) {
            value = scopes.runtimeValue(binding.projectVariableId);
        } else {
            const ScopedVariableHit hit = scopes.resolveById(binding.projectVariableId);
            value = hit.record.value;
            if (hit.shadowed)
                result.diagnosticCode = QStringLiteral("variable_shadowed");
        }
        if (expected != DataTypeId::None) {
            DataValue converted;
            QString err;
            if (!value.convertTo(expected, &converted, &err)) {
                result.error = err.isEmpty()
                    ? QStringLiteral("变量类型不兼容目标 %1").arg(dataTypeIdDisplayName(expected))
                    : err;
                result.diagnosticCode = QStringLiteral("variable_type_mismatch");
                return result;
            }
            value = converted;
        }
        result.ok = true;
        result.value = value;
        return result;
    }
    }
    result.error = QStringLiteral("未知绑定类型");
    return result;
}

ParameterBinding BindingResolver::bindingForParameter(const NodeModel &node, const ModuleParamDef &param)
{
    const DataTypeId expected = dataTypeIdFromModuleParamType(static_cast<int>(param.type));
    if (node.parameterBindings.contains(param.key)) {
        QString err;
        ParameterBinding binding =
            ParameterBinding::fromJson(node.parameterBindings.value(param.key).toObject(), &err);
        if (err.isEmpty() && binding.isValid()) {
            if (binding.targetType == DataTypeId::None)
                binding.targetType = expected;
            return binding;
        }
    }
    return ParameterBinding::legacyConstantBinding(node.parameters.value(param.key), expected);
}

bool BindingResolver::resolveNodeParameters(const NodeModel &node,
                                            const QVector<ModuleParamDef> &paramDefs,
                                            const PortValueStore &ports,
                                            const ProjectVariableStore &variables,
                                            QJsonObject *legacyJson,
                                            QHash<QString, DataValue> *typed,
                                            QString *error,
                                            QString *failedKey)
{
    VariableScopeChain chain;
    chain.setProject(&variables);
    return resolveNodeParameters(node, paramDefs, ports, chain, legacyJson, typed, error, failedKey);
}

bool BindingResolver::resolveNodeParameters(const NodeModel &node,
                                            const QVector<ModuleParamDef> &paramDefs,
                                            const PortValueStore &ports,
                                            const VariableScopeChain &scopes,
                                            QJsonObject *legacyJson,
                                            QHash<QString, DataValue> *typed,
                                            QString *error,
                                            QString *failedKey)
{
    QJsonObject json = node.parameters;
    if (typed)
        typed->clear();

    for (const ModuleParamDef &param : paramDefs) {
        const DataTypeId expected = dataTypeIdFromModuleParamType(static_cast<int>(param.type));
        const ParameterBinding binding = bindingForParameter(node, param);
        const BindingResolveResult resolved = resolve(binding, expected, ports, scopes);
        if (!resolved.ok) {
            if (error)
                *error = QStringLiteral("参数 %1: %2").arg(param.label, resolved.error);
            if (failedKey)
                *failedKey = param.key;
            return false;
        }
        if (typed)
            typed->insert(param.key, resolved.value);
        const QJsonValue asJson = resolved.value.toParameterJson();
        if (!asJson.isNull() && !(asJson.isObject() && asJson.toObject().isEmpty()
                                  && resolved.value.type() != DataTypeId::Roi)) {
            json.insert(param.key, asJson);
        } else if (binding.kind == ParameterSourceKind::Constant
                   && node.parameters.contains(param.key)) {
            json.insert(param.key, node.parameters.value(param.key));
        }
    }

    if (legacyJson)
        *legacyJson = json;
    return true;
}

} // namespace Selt
