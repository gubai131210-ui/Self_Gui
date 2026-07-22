#ifndef SAMPLEVALIDATION_H
#define SAMPLEVALIDATION_H

#include "core/model/document.h"
#include "vision/domain/projectvariables.h"
#include "vision/model/visioncontext.h"

#include <QDateTime>
#include <QString>
#include <QVector>

namespace Selt {

enum class SampleLabel {
    Unknown,
    Ok,
    Ng
};

QString sampleLabelToString(SampleLabel label);
SampleLabel sampleLabelFromString(const QString &text);

struct VisionSample
{
    QString id;
    QString imagePath;
    SampleLabel expected{SampleLabel::Unknown};
    QString batchId;
    QString note;

    bool isValid() const { return !id.isEmpty() && !imagePath.isEmpty(); }
};

struct SampleRunResult
{
    VisionSample sample;
    bool executed{false};
    bool actualOk{false};
    bool matched{false};
    QString errorMessage;
    QString failedNodeId;
    QString diagnosticCode;
    qint64 elapsedMs{0};
    QDateTime executedAt;
    VisionContext context;
};

struct SampleValidationSummary
{
    QVector<SampleRunResult> results;
    int expectedOk{0};
    int expectedNg{0};
    int unknownExpected{0};
    int matchedCount{0};
    int mismatchCount{0};
    int executionFailureCount{0};
    qint64 totalElapsedMs{0};

    double matchRate() const;
    int processedCount() const { return results.size(); }
};

/// Deterministic, hardware-independent batch validation over local image files.
class SampleValidationRunner
{
public:
    static SampleValidationSummary run(const Document &document,
                                       const QVector<VisionSample> &samples,
                                       const ProjectVariableStore &variables = {},
                                       const QString &defaultBatchId = {});
};

} // namespace Selt

#endif // SAMPLEVALIDATION_H
