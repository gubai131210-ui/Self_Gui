#include "vision/runtime/runanalysis.h"

#include "vision/runtime/runtimediagnostic.h"

namespace Selt {

RuntimePerformanceSummary summarizePerformance(const VisionContext &context)
{
    RuntimePerformanceSummary summary;
    summary.totalElapsedMs = qMax<qint64>(0, context.elapsedMs);

    for (const QString &nodeId : context.executionOrder) {
        const ModuleRunResult result = context.moduleResult(nodeId);
        if (result.status == ModuleStatus::Idle)
            continue;

        ++summary.nodeCount;
        if (result.status == ModuleStatus::Success)
            ++summary.successCount;
        else if (result.status == ModuleStatus::Failed)
            ++summary.failedCount;
        else if (result.status == ModuleStatus::Disabled)
            ++summary.disabledCount;

        const qint64 elapsedMs = qMax<qint64>(0, result.elapsedMs);
        summary.nodeElapsedMs += elapsedMs;
        if (elapsedMs > summary.slowestElapsedMs) {
            summary.slowestElapsedMs = elapsedMs;
            summary.slowestNodeId = nodeId;
            summary.slowestNodeName = result.displayName.isEmpty()
                ? nodeId
                : result.displayName;
        }
    }

    return summary;
}

RuntimeFailureExplanation explainFailure(const VisionContext &context)
{
    RuntimeFailureExplanation explanation;
    explanation.nodeId = context.failedNodeId;

    if (!explanation.nodeId.isEmpty()) {
        const ModuleRunResult result = context.moduleResult(explanation.nodeId);
        explanation.nodeName = result.displayName.isEmpty()
            ? explanation.nodeId
            : result.displayName;
        explanation.failureKind = result.failureKind;
        explanation.diagnosticCode = result.diagnosticCode;
        explanation.message = result.errorMessage;
    }

    // Runtime diagnostics retain codes and details even when a module result
    // was not created, for example during preflight validation.
    for (const RuntimeDiagnostic &diagnostic : context.runtimeDiagnostics) {
        if (diagnostic.severity != GraphDiagnosticSeverity::Error)
            continue;
        if (explanation.nodeId.isEmpty())
            explanation.nodeId = diagnostic.nodeId;
        if (explanation.nodeId != diagnostic.nodeId && !diagnostic.nodeId.isEmpty())
            continue;
        if (explanation.failureKind.isEmpty())
            explanation.failureKind = runtimeFailureKindToString(diagnostic.kind);
        if (explanation.diagnosticCode.isEmpty())
            explanation.diagnosticCode = diagnostic.code;
        if (explanation.message.isEmpty())
            explanation.message = diagnostic.message;
        break;
    }

    if (explanation.nodeName.isEmpty() && !explanation.nodeId.isEmpty()) {
        const ModuleRunResult result = context.moduleResult(explanation.nodeId);
        explanation.nodeName = result.displayName.isEmpty()
            ? explanation.nodeId
            : result.displayName;
    }
    if (explanation.message.isEmpty())
        explanation.message = context.errorMessage;

    return explanation;
}

} // namespace Selt
