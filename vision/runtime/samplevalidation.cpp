#include "vision/runtime/samplevalidation.h"

#include "vision/algorithms/imageloader.h"
#include "vision/runtime/flowruntime.h"

#include <QFileInfo>

namespace Selt {

QString sampleLabelToString(SampleLabel label)
{
    switch (label) {
    case SampleLabel::Ok:
        return QStringLiteral("ok");
    case SampleLabel::Ng:
        return QStringLiteral("ng");
    case SampleLabel::Unknown:
    default:
        return QStringLiteral("unknown");
    }
}

SampleLabel sampleLabelFromString(const QString &text)
{
    const QString normalized = text.trimmed().toLower();
    if (normalized == QLatin1String("ok") || normalized == QStringLiteral("pass"))
        return SampleLabel::Ok;
    if (normalized == QLatin1String("ng") || normalized == QStringLiteral("fail"))
        return SampleLabel::Ng;
    return SampleLabel::Unknown;
}

double SampleValidationSummary::matchRate() const
{
    int knownCount = 0;
    for (const SampleRunResult &result : results) {
        if (result.sample.expected != SampleLabel::Unknown)
            ++knownCount;
    }
    return knownCount == 0 ? 0.0 : double(matchedCount) / double(knownCount);
}

namespace {

bool actualInspectionOk(const VisionContext &context)
{
    if (context.hasError())
        return false;
    if (context.measurement.decisionState == QLatin1String("ng"))
        return false;
    if (context.measurement.decisionState == QLatin1String("ok"))
        return true;

    for (const QString &nodeId : context.executionOrder) {
        const ModuleRunResult result = context.moduleResult(nodeId);
        if (result.status == ModuleStatus::Failed)
            return false;
        if (result.measurement.decisionState == QLatin1String("ng"))
            return false;
    }
    return true;
}

} // namespace

SampleValidationSummary SampleValidationRunner::run(const Document &document,
                                                    const QVector<VisionSample> &samples,
                                                    const ProjectVariableStore &variables,
                                                    const QString &defaultBatchId)
{
    SampleValidationSummary summary;
    for (const VisionSample &sample : samples) {
        SampleRunResult result;
        result.sample = sample;
        result.executedAt = QDateTime::currentDateTimeUtc();

        if (!sample.isValid()) {
            result.errorMessage = QStringLiteral("样本缺少 id 或图像路径");
            ++summary.executionFailureCount;
            summary.results.append(result);
            continue;
        }

        VisionImage image;
        QString loadError;
        if (!ImageLoader::load(sample.imagePath, image, &loadError)) {
            result.errorMessage = loadError.isEmpty()
                ? QStringLiteral("无法加载样本图像: %1").arg(sample.imagePath)
                : loadError;
            ++summary.executionFailureCount;
            summary.results.append(result);
            continue;
        }

        RuntimeExecuteOptions options;
        options.liveFrame = &image;
        options.batchId = sample.batchId.isEmpty() ? defaultBatchId : sample.batchId;
        options.sourceDescription = QFileInfo(sample.imagePath).absoluteFilePath();
        options.frameGrabbedAt = result.executedAt;
        VisionContext context;
        context.batchId = options.batchId;
        context.sourceDescription = options.sourceDescription;
        context.frameGrabbedAt = result.executedAt;

        const bool ok = FlowRuntime::executeOnce(document, context, variables, options);
        result.executed = true;
        result.context = context;
        result.actualOk = ok && actualInspectionOk(context);
        result.elapsedMs = context.elapsedMs;
        result.failedNodeId = context.failedNodeId;
        result.diagnosticCode = context.runtimeDiagnostics.isEmpty()
            ? QString()
            : context.runtimeDiagnostics.first().code;
        result.errorMessage = context.errorMessage;

        if (sample.expected == SampleLabel::Ok || sample.expected == SampleLabel::Ng) {
            const bool expectedOk = sample.expected == SampleLabel::Ok;
            result.matched = (result.actualOk == expectedOk);
            if (result.matched)
                ++summary.matchedCount;
            else
                ++summary.mismatchCount;
            if (expectedOk)
                ++summary.expectedOk;
            else
                ++summary.expectedNg;
        } else {
            ++summary.unknownExpected;
        }
        if (!ok)
            ++summary.executionFailureCount;
        summary.totalElapsedMs += result.elapsedMs;
        summary.results.append(result);
    }
    return summary;
}

} // namespace Selt
