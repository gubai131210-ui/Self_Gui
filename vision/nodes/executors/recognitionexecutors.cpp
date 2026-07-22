#include "vision/algorithms/recognitionalgorithms.h"
#include "vision/nodes/imageexecutorhelpers.h"
#include "vision/registry/visionnodeids.h"
#include "vision/runtime/nodeexecutorregistry.h"

#include <QElapsedTimer>
#include <memory>

namespace Selt {
namespace {

void insertStableRecognitionOutputs(ExecutionResult &result, bool includeSymbology)
{
    if (!result.outputs.contains(QStringLiteral("text")))
        result.outputs.insert(QStringLiteral("text"), DataValue(QString()));
    if (!result.outputs.contains(QStringLiteral("confidence")))
        result.outputs.insert(QStringLiteral("confidence"), DataValue(0.0));
    if (!result.outputs.contains(QStringLiteral("found")))
        result.outputs.insert(QStringLiteral("found"), DataValue(false));
    if (!result.outputs.contains(QStringLiteral("overlay")))
        result.outputs.insert(QStringLiteral("overlay"), DataValue(OverlayList{}));
    if (includeSymbology && !result.outputs.contains(QStringLiteral("symbology")))
        result.outputs.insert(QStringLiteral("symbology"), DataValue(QString()));
}

ExecutionResult fillRecognitionOutputs(const RecognitionResult &decoded,
                                       const VisionImage &input,
                                       bool passthroughOnFail,
                                       bool includeSymbology,
                                       qint64 elapsedMs)
{
    ExecutionResult r;
    r.elapsedMs = elapsedMs;
    r.overlays = decoded.overlays;
    r.outputs.insert(QStringLiteral("text"), DataValue(decoded.joinedText));
    r.outputs.insert(QStringLiteral("confidence"), DataValue(decoded.confidence));
    r.outputs.insert(QStringLiteral("found"), DataValue(decoded.found));
    r.outputs.insert(QStringLiteral("overlay"), DataValue(decoded.overlays));
    if (includeSymbology) {
        r.outputs.insert(QStringLiteral("symbology"),
                         DataValue(decoded.symbologies.isEmpty()
                                       ? QString()
                                       : decoded.symbologies.join(QLatin1Char(','))));
    }

    VisionImage preview = decoded.overlayImage;
    if (preview.isEmpty() && (passthroughOnFail || decoded.found))
        preview = input;
    if (!preview.isEmpty())
        r.outputs.insert(QStringLiteral("image"), DataValue(preview));

    // 识别结果始终写入 measurement，便于 ResultSink / InspectionPack / 运行监视复用。
    r.measurement.valid = true;
    r.measurement.confidence = decoded.confidence;
    r.measurement.message = decoded.found
        ? decoded.joinedText
        : (decoded.errorMessage.isEmpty() ? QStringLiteral("no_target") : decoded.errorMessage);
    r.measurement.measurementType = QStringLiteral("recognition");
    r.measurement.decisionState = decoded.found ? QStringLiteral("ok") : QStringLiteral("ng");
    if (includeSymbology && !decoded.symbologies.isEmpty())
        r.measurement.unit = decoded.symbologies.join(QLatin1Char(','));

    if (!decoded.ok) {
        r.status = ModuleStatus::Failed;
        r.diagnosticCode = decoded.diagnosticCode;
        r.errorMessage = decoded.errorMessage;
        return r;
    }
    r.status = ModuleStatus::Success;
    r.diagnosticCode = decoded.diagnosticCode;
    r.errorMessage = decoded.errorMessage;
    return r;
}

class BarcodeDecodeExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::barcodeDecode(); }

    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        QElapsedTimer timer;
        timer.start();
        QString err;
        QString code;
        VisionImage input = requireImageWithRoi(request, &err, &code);
        if (input.isEmpty()) {
            ExecutionResult failed =
                failResult(err, code.isEmpty() ? DiagnosticCodes::imageEmpty() : code, timer.elapsed());
            insertStableRecognitionOutputs(failed, true);
            return failed;
        }

        BarcodeQrAlgorithm::Options options;
        options.symbology = resolveStringParam(request, QStringLiteral("symbology"),
                                               QStringLiteral("auto"));
        options.maxCount = resolveIntParam(request, QStringLiteral("maxCount"), 8);
        options.tryRotate = resolveBoolParam(request, QStringLiteral("tryRotate"), false);
        options.passthroughOnFail = resolveBoolParam(request, QStringLiteral("passthroughOnFail"), true);

        RecognitionResult decoded;
        if (!BarcodeQrAlgorithm::apply(input, options, decoded, &err)) {
            ExecutionResult failed = failResult(decoded.errorMessage.isEmpty() ? err : decoded.errorMessage,
                                                decoded.diagnosticCode.isEmpty()
                                                    ? DiagnosticCodes::capabilityLimited()
                                                    : decoded.diagnosticCode,
                                                timer.elapsed());
            insertStableRecognitionOutputs(failed, true);
            if (options.passthroughOnFail)
                failed.outputs.insert(QStringLiteral("image"), DataValue(input));
            return failed;
        }
        return fillRecognitionOutputs(decoded, input, options.passthroughOnFail, true, timer.elapsed());
    }
};

class OcrExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::ocr(); }

    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        QElapsedTimer timer;
        timer.start();
        QString err;
        QString code;
        VisionImage input = requireImageWithRoi(request, &err, &code);
        if (input.isEmpty()) {
            ExecutionResult failed =
                failResult(err, code.isEmpty() ? DiagnosticCodes::imageEmpty() : code, timer.elapsed());
            insertStableRecognitionOutputs(failed, false);
            return failed;
        }

        OcrAlgorithm::Options options;
        options.language = resolveStringParam(request, QStringLiteral("language"), QStringLiteral("eng"));
        options.tessdataPath = resolveStringParam(request, QStringLiteral("tessdataPath"));
        options.whitelist = resolveStringParam(request, QStringLiteral("whitelist"));
        options.multiLine = resolveBoolParam(request, QStringLiteral("multiLine"), true);
        options.scale = resolveRealInput(request, QStringLiteral("scale"), 1.0);
        options.minConfidence = resolveRealInput(request, QStringLiteral("minConfidence"), 0.0);
        options.binarize = resolveBoolParam(request, QStringLiteral("binarize"), false);
        const bool passthroughOnFail = resolveBoolParam(request, QStringLiteral("passthroughOnFail"), true);

        RecognitionResult decoded;
        if (!OcrAlgorithm::apply(input, options, decoded, &err)) {
            ExecutionResult failed = failResult(decoded.errorMessage.isEmpty() ? err : decoded.errorMessage,
                                                decoded.diagnosticCode.isEmpty()
                                                    ? DiagnosticCodes::capabilityLimited()
                                                    : decoded.diagnosticCode,
                                                timer.elapsed());
            insertStableRecognitionOutputs(failed, false);
            if (passthroughOnFail)
                failed.outputs.insert(QStringLiteral("image"), DataValue(input));
            return failed;
        }
        return fillRecognitionOutputs(decoded, input, passthroughOnFail, false, timer.elapsed());
    }
};

} // namespace

void registerRecognitionExecutors(NodeExecutorRegistry &reg)
{
    reg.registerFactory(VisionNodeIds::barcodeDecode(),
                        [] { return std::make_shared<BarcodeDecodeExecutor>(); });
    reg.registerFactory(VisionNodeIds::ocr(),
                        [] { return std::make_shared<OcrExecutor>(); });
}

} // namespace Selt
