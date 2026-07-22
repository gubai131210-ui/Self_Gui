#ifndef RUNANALYSIS_H
#define RUNANALYSIS_H

#include "vision/model/visioncontext.h"

#include <QString>

namespace Selt {

struct RuntimePerformanceSummary
{
    qint64 totalElapsedMs{0};
    qint64 nodeElapsedMs{0};
    qint64 slowestElapsedMs{0};
    int nodeCount{0};
    int successCount{0};
    int failedCount{0};
    int disabledCount{0};
    QString slowestNodeId;
    QString slowestNodeName;
};

struct RuntimeFailureExplanation
{
    QString nodeId;
    QString nodeName;
    QString failureKind;
    QString diagnosticCode;
    QString message;

    bool isValid() const { return !message.isEmpty(); }
};

// Builds a stable, UI-independent summary for single-step profiling views.
RuntimePerformanceSummary summarizePerformance(const VisionContext &context);

// Selects the most actionable failure from a context without changing it.
RuntimeFailureExplanation explainFailure(const VisionContext &context);

} // namespace Selt

#endif // RUNANALYSIS_H
