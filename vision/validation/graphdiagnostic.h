#ifndef GRAPHDIAGNOSTIC_H
#define GRAPHDIAGNOSTIC_H

#include <QString>

namespace Selt {

enum class GraphDiagnosticSeverity {
    Info,
    Warning,
    Error
};

struct GraphDiagnostic
{
    GraphDiagnosticSeverity severity{GraphDiagnosticSeverity::Error};
    QString code;
    QString nodeId;
    QString portId;
    QString parameterKey;
    QString connectionId;
    QString message;
};

} // namespace Selt

#endif // GRAPHDIAGNOSTIC_H
