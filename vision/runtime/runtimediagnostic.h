#ifndef RUNTIMEDIAGNOSTIC_H
#define RUNTIMEDIAGNOSTIC_H

#include "vision/validation/graphdiagnostic.h"

#include <QString>
#include <QVector>

namespace Selt {

enum class RuntimeFailureKind {
    None,
    Validation,
    Binding,
    Parameter,
    Device,
    Execution,
    Cancelled,
    Disabled
};

inline QString runtimeFailureKindToString(RuntimeFailureKind kind)
{
    switch (kind) {
    case RuntimeFailureKind::Validation: return QStringLiteral("validation");
    case RuntimeFailureKind::Binding: return QStringLiteral("binding");
    case RuntimeFailureKind::Parameter: return QStringLiteral("parameter");
    case RuntimeFailureKind::Device: return QStringLiteral("device");
    case RuntimeFailureKind::Execution: return QStringLiteral("execution");
    case RuntimeFailureKind::Cancelled: return QStringLiteral("cancelled");
    case RuntimeFailureKind::Disabled: return QStringLiteral("disabled");
    case RuntimeFailureKind::None:
    default: return QString();
    }
}

inline QString runtimeFailureKindDisplayName(RuntimeFailureKind kind)
{
    switch (kind) {
    case RuntimeFailureKind::Validation: return QStringLiteral("静态校验");
    case RuntimeFailureKind::Binding: return QStringLiteral("绑定错误");
    case RuntimeFailureKind::Parameter: return QStringLiteral("参数错误");
    case RuntimeFailureKind::Device: return QStringLiteral("设备错误");
    case RuntimeFailureKind::Execution: return QStringLiteral("执行失败");
    case RuntimeFailureKind::Cancelled: return QStringLiteral("已取消");
    case RuntimeFailureKind::Disabled: return QStringLiteral("被禁用");
    case RuntimeFailureKind::None:
    default: return QStringLiteral("无");
    }
}

inline RuntimeFailureKind runtimeFailureKindFromString(const QString &text)
{
    if (text == QLatin1String("validation")) return RuntimeFailureKind::Validation;
    if (text == QLatin1String("binding")) return RuntimeFailureKind::Binding;
    if (text == QLatin1String("parameter")) return RuntimeFailureKind::Parameter;
    if (text == QLatin1String("device")) return RuntimeFailureKind::Device;
    if (text == QLatin1String("execution")) return RuntimeFailureKind::Execution;
    if (text == QLatin1String("cancelled")) return RuntimeFailureKind::Cancelled;
    if (text == QLatin1String("disabled")) return RuntimeFailureKind::Disabled;
    return RuntimeFailureKind::None;
}

struct RuntimeDiagnostic
{
    GraphDiagnosticSeverity severity{GraphDiagnosticSeverity::Error};
    RuntimeFailureKind kind{RuntimeFailureKind::Execution};
    QString code;
    QString nodeId;
    QString portId;
    QString parameterKey;
    QString message;

    GraphDiagnostic toGraphDiagnostic() const
    {
        GraphDiagnostic d;
        d.severity = severity;
        d.code = code.isEmpty() ? runtimeFailureKindToString(kind) : code;
        d.nodeId = nodeId;
        d.portId = portId;
        d.parameterKey = parameterKey;
        d.message = message;
        return d;
    }

    static RuntimeDiagnostic make(RuntimeFailureKind kind, const QString &message,
                                  const QString &nodeId = {}, const QString &code = {},
                                  const QString &portId = {}, const QString &parameterKey = {})
    {
        RuntimeDiagnostic d;
        d.kind = kind;
        d.message = message;
        d.nodeId = nodeId;
        d.code = code.isEmpty() ? runtimeFailureKindToString(kind) : code;
        d.portId = portId;
        d.parameterKey = parameterKey;
        d.severity = (kind == RuntimeFailureKind::Disabled)
            ? GraphDiagnosticSeverity::Info
            : GraphDiagnosticSeverity::Error;
        return d;
    }
};

} // namespace Selt

#endif // RUNTIMEDIAGNOSTIC_H
