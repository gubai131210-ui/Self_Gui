#include "vision/algorithms/recognitionalgorithms.h"
#include "vision/nodes/builtinexecutors.h"
#include "vision/registry/visionnodeids.h"
#include "vision/registry/visionnoderegistry.h"
#include "vision/runtime/diagnosticcodes.h"
#include "vision/runtime/inodeexecutor.h"
#include "vision/runtime/nodeexecutorregistry.h"
#include "vision/workflow/workflowtemplates.h"

#include <QImage>
#include <QTemporaryDir>
#include <QtTest>
#include <opencv2/imgproc.hpp>
#if defined(SELT_HAS_OPENCV_QR) && SELT_HAS_OPENCV_QR
#include <opencv2/objdetect.hpp>
#endif

using namespace Selt;

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
    void barcodeModeWithoutBackendIsLimited();
    void ocrWithoutBackendIsLimited();
    void templatesExist();
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
}

void TestRecognitionExecutors::barcodePortsRegistered()
{
    const ModuleDescriptor barcode = VisionNodeRegistry::descriptor(VisionNodeIds::barcodeDecode());
    QVERIFY(barcode.findInputPort(QStringLiteral("in")));
    QVERIFY(barcode.findOutputPort(QStringLiteral("text")));
    QVERIFY(barcode.findOutputPort(QStringLiteral("found")));
    QVERIFY(barcode.findOutputPort(QStringLiteral("overlay")));

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
}

void TestRecognitionExecutors::qrDecodeWhenAvailable()
{
    if (!RecognitionCapability::hasQrBackend())
        QSKIP("OpenCV QR backend unavailable");

#if defined(SELT_HAS_OPENCV_QR) && SELT_HAS_OPENCV_QR
    cv::Mat mat(240, 240, CV_8UC3, cv::Scalar(255, 255, 255));
    const std::string payload = "SELT-QR-1";
    cv::QRCodeEncoder::Params params;
    auto encoder = cv::QRCodeEncoder::create(params);
    cv::Mat qr;
    encoder->encode(payload, qr);
    QVERIFY(!qr.empty());
    cv::Mat resized;
    cv::resize(qr, resized, cv::Size(160, 160), 0, 0, cv::INTER_NEAREST);
    // QRCodeEncoder 输出单通道；拷入 BGR 画布前需转换，否则 OpenCV 抛异常。
    if (resized.channels() == 1)
        cv::cvtColor(resized, resized, cv::COLOR_GRAY2BGR);
    resized.copyTo(mat(cv::Rect(40, 40, resized.cols, resized.rows)));

    auto executor = NodeExecutorRegistry::instance().create(VisionNodeIds::barcodeDecode());
    QVERIFY(executor);
    ExecutionRequest request;
    request.typeId = VisionNodeIds::barcodeDecode();
    request.inputs.insert(QStringLiteral("image"), DataValue(VisionImage(mat)));
    request.parameters.insert(QStringLiteral("symbology"), QStringLiteral("qr"));
    ExecutionContext context;
    const ExecutionResult result = executor->execute(request, context);
    QCOMPARE(result.status, ModuleStatus::Success);
    QVERIFY(result.outputs.value(QStringLiteral("found")).toBool());
    QCOMPARE(result.outputs.value(QStringLiteral("text")).toString(), QString::fromStdString(payload));
    QVERIFY(result.outputs.contains(QStringLiteral("overlay")));
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
    ExecutionContext context;
    const ExecutionResult result = executor->execute(request, context);
    QCOMPARE(result.status, ModuleStatus::Success);
    QCOMPARE(result.diagnosticCode, DiagnosticCodes::noTarget());
    QCOMPARE(result.outputs.value(QStringLiteral("found")).toBool(), false);
    QCOMPARE(result.outputs.value(QStringLiteral("text")).toString(), QString());
    QCOMPARE(result.outputs.value(QStringLiteral("confidence")).toReal(), 0.0);
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
    QCOMPARE(result.diagnosticCode, DiagnosticCodes::capabilityLimited());
    QCOMPARE(result.outputs.value(QStringLiteral("found")).toBool(), false);
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
    QCOMPARE(result.diagnosticCode, DiagnosticCodes::capabilityLimited());
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

QTEST_MAIN(TestRecognitionExecutors)
#include "test_recognition_executors.moc"
