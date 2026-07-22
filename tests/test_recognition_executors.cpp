#include "vision/algorithms/recognitionalgorithms.h"
#include "vision/nodes/builtinexecutors.h"
#include "vision/registry/visionnodeids.h"
#include "vision/registry/visionnoderegistry.h"
#include "vision/runtime/diagnosticcodes.h"
#include "vision/runtime/inodeexecutor.h"
#include "vision/runtime/nodeexecutorregistry.h"
#include "vision/workflow/workflowtemplates.h"

#include <QElapsedTimer>
#include <QImage>
#include <QTemporaryDir>
#include <QtTest>
#include <opencv2/imgproc.hpp>
#if defined(SELT_HAS_OPENCV_QR) && SELT_HAS_OPENCV_QR
#include <opencv2/objdetect.hpp>
#endif

using namespace Selt;

namespace {

#if defined(SELT_HAS_OPENCV_QR) && SELT_HAS_OPENCV_QR
cv::Mat makeQrBgr(const std::string &payload, int canvas = 240, int codeSize = 160)
{
    cv::Mat mat(canvas, canvas, CV_8UC3, cv::Scalar(255, 255, 255));
    cv::QRCodeEncoder::Params params;
    auto encoder = cv::QRCodeEncoder::create(params);
    cv::Mat qr;
    encoder->encode(payload, qr);
    if (qr.empty())
        return {};
    cv::Mat resized;
    cv::resize(qr, resized, cv::Size(codeSize, codeSize), 0, 0, cv::INTER_NEAREST);
    if (resized.channels() == 1)
        cv::cvtColor(resized, resized, cv::COLOR_GRAY2BGR);
    const int x = (canvas - codeSize) / 2;
    const int y = (canvas - codeSize) / 2;
    resized.copyTo(mat(cv::Rect(x, y, resized.cols, resized.rows)));
    return mat;
}
#endif

} // namespace

class TestRecognitionExecutors : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void capabilityStatusIsStable();
    void barcodePortsRegistered();
    void emptyImageFailsCleanly();
    void qrDecodeWhenAvailable();
    void qrNoTargetOnBlankImage();
    void qrRotatedDecode();
    void qrLowResolutionEnhance();
    void qrUnevenLightingClahe();
    void qrInvertedDecode();
    void qrCompressedDecodeDiagnostic();
    void qrOldParameterJsonCompatible();
    void algorithmPrimaryGeometryFilled();
    void barcodeModeWithoutBackendIsLimited();
    void ocrWithoutBackendIsLimited();
    void diagnosticCodesExist();
    void templatesExist();
    void recognitionStrategyBenchmark();
};

void TestRecognitionExecutors::initTestCase()
{
    registerBuiltInOpenCvExecutors();
}

void TestRecognitionExecutors::capabilityStatusIsStable()
{
    QVERIFY(!RecognitionCapability::qrStatusText().isEmpty());
    QVERIFY(!RecognitionCapability::barcodeStatusText().isEmpty());
    QVERIFY(!RecognitionCapability::ocrStatusText().isEmpty());
    QVERIFY(RecognitionCapability::combinedStatusText().contains(QStringLiteral("QR")));
}

void TestRecognitionExecutors::barcodePortsRegistered()
{
    const ModuleDescriptor barcode = VisionNodeRegistry::descriptor(VisionNodeIds::barcodeDecode());
    QVERIFY(barcode.findInputPort(QStringLiteral("in")));
    QVERIFY(barcode.findOutputPort(QStringLiteral("text")));
    QVERIFY(barcode.findOutputPort(QStringLiteral("found")));
    QVERIFY(barcode.findOutputPort(QStringLiteral("overlay")));
    QVERIFY(barcode.findOutputPort(QStringLiteral("strategy")));
    QVERIFY(barcode.findOutputPort(QStringLiteral("failureStage")));
    QVERIFY(barcode.findOutputPort(QStringLiteral("debug")));
    QVERIFY(barcode.findParam(QStringLiteral("enhanceMode")));
    QVERIFY(barcode.findParam(QStringLiteral("tryClahe")));
    QVERIFY(barcode.findParam(QStringLiteral("maxTimeMs")));
    QVERIFY(barcode.findOutputPort(QStringLiteral("backendStatus")));

    const ModuleDescriptor ocr = VisionNodeRegistry::descriptor(VisionNodeIds::ocr());
    QVERIFY(ocr.findInputPort(QStringLiteral("in")));
    QVERIFY(ocr.findOutputPort(QStringLiteral("text")));
    QVERIFY(ocr.findOutputPort(QStringLiteral("found")));
}

void TestRecognitionExecutors::emptyImageFailsCleanly()
{
    auto executor = NodeExecutorRegistry::instance().create(VisionNodeIds::barcodeDecode());
    QVERIFY(executor);
    ExecutionRequest request;
    request.typeId = VisionNodeIds::barcodeDecode();
    ExecutionContext context;
    const ExecutionResult result = executor->execute(request, context);
    QCOMPARE(result.status, ModuleStatus::Failed);
    QCOMPARE(result.diagnosticCode, DiagnosticCodes::imageEmpty());
    QVERIFY(result.outputs.contains(QStringLiteral("found")));
    QCOMPARE(result.outputs.value(QStringLiteral("found")).toBool(), false);
    QCOMPARE(result.outputs.value(QStringLiteral("failureStage")).toString(), QStringLiteral("input"));
}

void TestRecognitionExecutors::qrDecodeWhenAvailable()
{
    if (!RecognitionCapability::hasQrBackend())
        QSKIP("OpenCV QR backend unavailable");

#if defined(SELT_HAS_OPENCV_QR) && SELT_HAS_OPENCV_QR
    const std::string payload = "SELT-QR-1";
    const cv::Mat mat = makeQrBgr(payload);
    QVERIFY(!mat.empty());

    auto executor = NodeExecutorRegistry::instance().create(VisionNodeIds::barcodeDecode());
    QVERIFY(executor);
    ExecutionRequest request;
    request.typeId = VisionNodeIds::barcodeDecode();
    request.inputs.insert(QStringLiteral("image"), DataValue(VisionImage(mat)));
    request.parameters.insert(QStringLiteral("symbology"), QStringLiteral("qr"));
    request.parameters.insert(QStringLiteral("enhanceMode"), QStringLiteral("none"));
    ExecutionContext context;
    const ExecutionResult result = executor->execute(request, context);
    QCOMPARE(result.status, ModuleStatus::Success);
    QVERIFY(result.outputs.value(QStringLiteral("found")).toBool());
    QCOMPARE(result.outputs.value(QStringLiteral("text")).toString(), QString::fromStdString(payload));
    QVERIFY(result.outputs.contains(QStringLiteral("overlay")));
    QVERIFY(result.outputs.contains(QStringLiteral("attempts")));
    QVERIFY(result.outputs.value(QStringLiteral("attempts")).toInt() >= 1);
#else
    QSKIP("OpenCV QR headers not compiled in");
#endif
}

void TestRecognitionExecutors::qrNoTargetOnBlankImage()
{
    if (!RecognitionCapability::hasQrBackend())
        QSKIP("OpenCV QR backend unavailable");

    cv::Mat mat(96, 96, CV_8UC1, cv::Scalar(255));
    auto executor = NodeExecutorRegistry::instance().create(VisionNodeIds::barcodeDecode());
    QVERIFY(executor);
    ExecutionRequest request;
    request.typeId = VisionNodeIds::barcodeDecode();
    request.inputs.insert(QStringLiteral("image"), DataValue(VisionImage(mat)));
    request.parameters.insert(QStringLiteral("symbology"), QStringLiteral("qr"));
    request.parameters.insert(QStringLiteral("enhanceMode"), QStringLiteral("none"));
    ExecutionContext context;
    const ExecutionResult result = executor->execute(request, context);
    QCOMPARE(result.status, ModuleStatus::Success);
    QCOMPARE(result.diagnosticCode, DiagnosticCodes::noCandidate());
    QCOMPARE(result.outputs.value(QStringLiteral("found")).toBool(), false);
    QCOMPARE(result.outputs.value(QStringLiteral("text")).toString(), QString());
    QCOMPARE(result.outputs.value(QStringLiteral("confidence")).toReal(), 0.0);
    QCOMPARE(result.outputs.value(QStringLiteral("failureStage")).toString(), QStringLiteral("detect"));
}

void TestRecognitionExecutors::qrRotatedDecode()
{
    if (!RecognitionCapability::hasQrBackend())
        QSKIP("OpenCV QR backend unavailable");
#if defined(SELT_HAS_OPENCV_QR) && SELT_HAS_OPENCV_QR
    const std::string payload = "SELT-QR-ROT";
    cv::Mat mat = makeQrBgr(payload);
    QVERIFY(!mat.empty());
    cv::rotate(mat, mat, cv::ROTATE_90_CLOCKWISE);

    RecognitionResult decoded;
    BarcodeQrAlgorithm::Options options;
    options.symbology = QStringLiteral("qr");
    options.enhanceMode = QStringLiteral("auto");
    options.tryRotate = true;
    QVERIFY(BarcodeQrAlgorithm::apply(VisionImage(mat), options, decoded));
    QVERIFY(decoded.found);
    QCOMPARE(decoded.joinedText, QString::fromStdString(payload));
    QVERIFY(!decoded.strategyUsed.isEmpty());
#else
    QSKIP("OpenCV QR headers not compiled in");
#endif
}

void TestRecognitionExecutors::qrLowResolutionEnhance()
{
    if (!RecognitionCapability::hasQrBackend())
        QSKIP("OpenCV QR backend unavailable");
#if defined(SELT_HAS_OPENCV_QR) && SELT_HAS_OPENCV_QR
    const std::string payload = "SELT-QR-LQ";
    cv::Mat mat = makeQrBgr(payload, 240, 160);
    QVERIFY(!mat.empty());
    cv::Mat small;
    cv::resize(mat, small, cv::Size(80, 80), 0, 0, cv::INTER_AREA);

    RecognitionResult decoded;
    BarcodeQrAlgorithm::Options options;
    options.symbology = QStringLiteral("qr");
    options.enhanceMode = QStringLiteral("auto");
    options.scale = 1.0;
    options.trySharpen = true;
    options.tryClahe = true;
    QVERIFY(BarcodeQrAlgorithm::apply(VisionImage(small), options, decoded));
    // 低分辨率不一定总能解出，但必须给出明确诊断而不是空诊断。
    if (decoded.found) {
        QCOMPARE(decoded.joinedText, QString::fromStdString(payload));
        QVERIFY(decoded.attemptCount >= 1);
    } else {
        QVERIFY(!decoded.diagnosticCode.isEmpty());
        QVERIFY(decoded.diagnosticCode == DiagnosticCodes::noCandidate()
                || decoded.diagnosticCode == DiagnosticCodes::decodeFailed());
        QVERIFY(decoded.attemptCount >= 1);
    }
#else
    QSKIP("OpenCV QR headers not compiled in");
#endif
}

void TestRecognitionExecutors::qrUnevenLightingClahe()
{
    if (!RecognitionCapability::hasQrBackend())
        QSKIP("OpenCV QR backend unavailable");
#if defined(SELT_HAS_OPENCV_QR) && SELT_HAS_OPENCV_QR
    const std::string payload = "SELT-QR-LIT";
    cv::Mat mat = makeQrBgr(payload);
    QVERIFY(!mat.empty());
    // 模拟亮度不均：左侧变暗。
    for (int y = 0; y < mat.rows; ++y) {
        for (int x = 0; x < mat.cols / 2; ++x) {
            cv::Vec3b &px = mat.at<cv::Vec3b>(y, x);
            px[0] = uchar(px[0] * 0.35);
            px[1] = uchar(px[1] * 0.35);
            px[2] = uchar(px[2] * 0.35);
        }
    }

    RecognitionResult decoded;
    BarcodeQrAlgorithm::Options options;
    options.symbology = QStringLiteral("qr");
    options.enhanceMode = QStringLiteral("full");
    options.tryClahe = true;
    options.tryInvert = true;
    QVERIFY(BarcodeQrAlgorithm::apply(VisionImage(mat), options, decoded));
    if (decoded.found) {
        QCOMPARE(decoded.joinedText, QString::fromStdString(payload));
    } else {
        QVERIFY(!decoded.diagnosticCode.isEmpty());
        QVERIFY(!decoded.errorMessage.isEmpty());
    }
#else
    QSKIP("OpenCV QR headers not compiled in");
#endif
}

void TestRecognitionExecutors::qrInvertedDecode()
{
    if (!RecognitionCapability::hasQrBackend())
        QSKIP("OpenCV QR backend unavailable");
#if defined(SELT_HAS_OPENCV_QR) && SELT_HAS_OPENCV_QR
    const std::string payload = "SELT-QR-INV";
    cv::Mat mat = makeQrBgr(payload);
    QVERIFY(!mat.empty());
    cv::bitwise_not(mat, mat);

    RecognitionResult decoded;
    BarcodeQrAlgorithm::Options options;
    options.symbology = QStringLiteral("qr");
    options.enhanceMode = QStringLiteral("auto");
    options.tryInvert = true;
    options.maxTimeMs = 2000;
    QVERIFY(BarcodeQrAlgorithm::apply(VisionImage(mat), options, decoded));
    if (decoded.found) {
        QCOMPARE(decoded.joinedText, QString::fromStdString(payload));
        QVERIFY(decoded.strategyUsed.contains(QStringLiteral("invert"))
                || decoded.attemptCount >= 1);
    } else {
        QVERIFY(!decoded.diagnosticCode.isEmpty());
        QVERIFY(decoded.attemptCount >= 1);
    }
#else
    QSKIP("OpenCV QR headers not compiled in");
#endif
}

void TestRecognitionExecutors::qrCompressedDecodeDiagnostic()
{
    if (!RecognitionCapability::hasQrBackend())
        QSKIP("OpenCV QR backend unavailable");
#if defined(SELT_HAS_OPENCV_QR) && SELT_HAS_OPENCV_QR
    const std::string payload = "SELT-QR-JPG";
    cv::Mat mat = makeQrBgr(payload, 320, 200);
    QVERIFY(!mat.empty());
    // 模拟压缩伪影：缩小再放大。
    cv::Mat tiny;
    cv::resize(mat, tiny, cv::Size(64, 64), 0, 0, cv::INTER_AREA);
    cv::Mat compressed;
    cv::resize(tiny, compressed, mat.size(), 0, 0, cv::INTER_LINEAR);

    RecognitionResult decoded;
    BarcodeQrAlgorithm::Options options;
    options.symbology = QStringLiteral("qr");
    options.enhanceMode = QStringLiteral("full");
    options.trySharpen = true;
    options.tryClahe = true;
    options.maxAttempts = 16;
    options.maxTimeMs = 3000;
    QVERIFY(BarcodeQrAlgorithm::apply(VisionImage(compressed), options, decoded));
    QVERIFY(decoded.attemptCount >= 1);
    QVERIFY(decoded.elapsedMs >= 0);
    if (!decoded.found) {
        QVERIFY(!decoded.diagnosticCode.isEmpty());
        QVERIFY(decoded.diagnosticCode == DiagnosticCodes::noCandidate()
                || decoded.diagnosticCode == DiagnosticCodes::decodeFailed());
    } else {
        QCOMPARE(decoded.joinedText, QString::fromStdString(payload));
        QVERIFY(decoded.boundingRect.isValid() || !decoded.corners.isEmpty());
    }
#else
    QSKIP("OpenCV QR headers not compiled in");
#endif
}

void TestRecognitionExecutors::qrOldParameterJsonCompatible()
{
    auto executor = NodeExecutorRegistry::instance().create(VisionNodeIds::barcodeDecode());
    QVERIFY(executor);
    ExecutionRequest request;
    request.typeId = VisionNodeIds::barcodeDecode();
    // 旧流程仅有 symbology / maxCount / passthroughOnFail，不应因新增参数崩溃。
    request.parameters.insert(QStringLiteral("symbology"), QStringLiteral("auto"));
    request.parameters.insert(QStringLiteral("maxCount"), 4);
    request.parameters.insert(QStringLiteral("passthroughOnFail"), true);
    cv::Mat mat(48, 48, CV_8UC1, cv::Scalar(255));
    request.inputs.insert(QStringLiteral("image"), DataValue(VisionImage(mat)));
    ExecutionContext context;
    const ExecutionResult result = executor->execute(request, context);
    QVERIFY(result.status == ModuleStatus::Success || result.status == ModuleStatus::Failed);
    QVERIFY(result.outputs.contains(QStringLiteral("found")));
    QVERIFY(result.outputs.contains(QStringLiteral("text")));
    QVERIFY(result.outputs.contains(QStringLiteral("strategy")));
    QVERIFY(result.outputs.contains(QStringLiteral("failureStage")));
}

void TestRecognitionExecutors::algorithmPrimaryGeometryFilled()
{
    if (!RecognitionCapability::hasQrBackend())
        QSKIP("OpenCV QR backend unavailable");
#if defined(SELT_HAS_OPENCV_QR) && SELT_HAS_OPENCV_QR
    const std::string payload = "SELT-QR-GEO";
    const cv::Mat mat = makeQrBgr(payload);
    QVERIFY(!mat.empty());
    RecognitionResult decoded;
    BarcodeQrAlgorithm::Options options;
    options.symbology = QStringLiteral("qr");
    options.enhanceMode = QStringLiteral("none");
    QVERIFY(BarcodeQrAlgorithm::apply(VisionImage(mat), options, decoded));
    QVERIFY(decoded.found);
    QVERIFY(decoded.boundingRect.isValid() || decoded.corners.size() >= 3);
    QCOMPARE(decoded.failureStage, QStringLiteral("none"));
#else
    QSKIP("OpenCV QR headers not compiled in");
#endif
}

void TestRecognitionExecutors::barcodeModeWithoutBackendIsLimited()
{
    if (RecognitionCapability::hasBarcodeBackend())
        QSKIP("Barcode backend available on this machine");

    cv::Mat mat(64, 64, CV_8UC1, cv::Scalar(255));
    auto executor = NodeExecutorRegistry::instance().create(VisionNodeIds::barcodeDecode());
    QVERIFY(executor);
    ExecutionRequest request;
    request.typeId = VisionNodeIds::barcodeDecode();
    request.inputs.insert(QStringLiteral("image"), DataValue(VisionImage(mat)));
    request.parameters.insert(QStringLiteral("symbology"), QStringLiteral("barcode"));
    ExecutionContext context;
    const ExecutionResult result = executor->execute(request, context);
    QCOMPARE(result.status, ModuleStatus::Failed);
    QCOMPARE(result.diagnosticCode, DiagnosticCodes::backendMissing());
    QCOMPARE(result.outputs.value(QStringLiteral("found")).toBool(), false);
    QCOMPARE(result.outputs.value(QStringLiteral("failureStage")).toString(), QStringLiteral("backend"));
}

void TestRecognitionExecutors::ocrWithoutBackendIsLimited()
{
    if (RecognitionCapability::hasOcrBackend())
        QSKIP("Tesseract available on this machine");

    cv::Mat mat(64, 64, CV_8UC1, cv::Scalar(255));
    auto executor = NodeExecutorRegistry::instance().create(VisionNodeIds::ocr());
    QVERIFY(executor);
    ExecutionRequest request;
    request.typeId = VisionNodeIds::ocr();
    request.inputs.insert(QStringLiteral("image"), DataValue(VisionImage(mat)));
    ExecutionContext context;
    const ExecutionResult result = executor->execute(request, context);
    QCOMPARE(result.status, ModuleStatus::Failed);
    QCOMPARE(result.diagnosticCode, DiagnosticCodes::backendMissing());
}

void TestRecognitionExecutors::diagnosticCodesExist()
{
    QCOMPARE(DiagnosticCodes::backendMissing(), QStringLiteral("backend_missing"));
    QCOMPARE(DiagnosticCodes::noCandidate(), QStringLiteral("no_candidate"));
    QCOMPARE(DiagnosticCodes::decodeFailed(), QStringLiteral("decode_failed"));
    QCOMPARE(DiagnosticCodes::qualityLow(), QStringLiteral("quality_low"));
    QCOMPARE(DiagnosticCodes::imageInvalid(), QStringLiteral("image_invalid"));
}

void TestRecognitionExecutors::templatesExist()
{
    QVERIFY(WorkflowTemplates::templateIds().contains(QStringLiteral("qr_preview")));
    QVERIFY(WorkflowTemplates::templateIds().contains(QStringLiteral("ocr_preview")));
    const WorkflowTemplateSpec qr = WorkflowTemplates::build(QStringLiteral("qr_preview"));
    QVERIFY(!qr.nodes.isEmpty());
    const WorkflowTemplateSpec ocr = WorkflowTemplates::build(QStringLiteral("ocr_preview"));
    QVERIFY(!ocr.nodes.isEmpty());
}

void TestRecognitionExecutors::recognitionStrategyBenchmark()
{
    if (!RecognitionCapability::hasQrBackend())
        QSKIP("OpenCV QR backend unavailable");
#if defined(SELT_HAS_OPENCV_QR) && SELT_HAS_OPENCV_QR
    const std::string payload = "SELT-QR-BENCH";
    cv::Mat clear = makeQrBgr(payload);
    cv::Mat dark = clear.clone();
    for (int y = 0; y < dark.rows; ++y) {
        for (int x = 0; x < dark.cols; ++x) {
            cv::Vec3b &px = dark.at<cv::Vec3b>(y, x);
            px[0] = uchar(px[0] * 0.45);
            px[1] = uchar(px[1] * 0.45);
            px[2] = uchar(px[2] * 0.45);
        }
    }
    cv::Mat small;
    cv::resize(clear, small, cv::Size(90, 90), 0, 0, cv::INTER_AREA);

    struct Case {
        const char *name;
        cv::Mat mat;
        QString enhanceMode;
    };
    const QVector<Case> cases = {
        {"clear_none", clear, QStringLiteral("none")},
        {"clear_auto", clear, QStringLiteral("auto")},
        {"dark_full", dark, QStringLiteral("full")},
        {"small_auto", small, QStringLiteral("auto")},
    };

    int successCount = 0;
    for (const Case &c : cases) {
        RecognitionResult decoded;
        BarcodeQrAlgorithm::Options options;
        options.symbology = QStringLiteral("qr");
        options.enhanceMode = c.enhanceMode;
        options.tryClahe = true;
        options.trySharpen = true;
        options.tryInvert = true;
        options.maxTimeMs = 2500;
        QElapsedTimer t;
        t.start();
        QVERIFY(BarcodeQrAlgorithm::apply(VisionImage(c.mat), options, decoded));
        const qint64 ms = t.elapsed();
        if (decoded.found)
            ++successCount;
        qDebug("recognition bench %s: found=%d strategy=%s attempts=%d ms=%lld code=%s",
               c.name,
               int(decoded.found),
               qPrintable(decoded.strategyUsed),
               decoded.attemptCount,
               static_cast<long long>(ms),
               qPrintable(decoded.diagnosticCode));
        QVERIFY(decoded.attemptCount >= 1 || !RecognitionCapability::hasQrBackend());
        if (!decoded.found)
            QVERIFY(!decoded.diagnosticCode.isEmpty());
    }
    // 清晰图至少应有成功；困难图允许失败但需有诊断（上面已断言）。
    QVERIFY(successCount >= 1);
#else
    QSKIP("OpenCV QR headers not compiled in");
#endif
}

QTEST_MAIN(TestRecognitionExecutors)
#include "test_recognition_executors.moc"
