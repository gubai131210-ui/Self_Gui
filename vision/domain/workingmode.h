#ifndef WORKINGMODE_H
#define WORKINGMODE_H

#include <QString>

namespace Selt {

/// Separates topology/configuration work from safe production operation.
enum class WorkingMode {
    Engineer,
    Operator
};

inline QString workingModeToString(WorkingMode mode)
{
    return mode == WorkingMode::Operator ? QStringLiteral("operator")
                                         : QStringLiteral("engineer");
}

inline WorkingMode workingModeFromString(const QString &value)
{
    return value.compare(QStringLiteral("operator"), Qt::CaseInsensitive) == 0
        ? WorkingMode::Operator
        : WorkingMode::Engineer;
}

struct WorkingModePolicy
{
    WorkingMode mode{WorkingMode::Engineer};

    bool canEditTopology() const { return mode == WorkingMode::Engineer; }
    bool canEditParameters() const { return true; }
    bool canRun() const { return true; }
    bool canManagePlugins() const { return mode == WorkingMode::Engineer; }
    bool canOpenDiagnostics() const { return mode == WorkingMode::Engineer; }
};

} // namespace Selt

#endif // WORKINGMODE_H
