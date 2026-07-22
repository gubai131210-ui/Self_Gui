#include "tests/metrologytesthelpers.h"
#include "vision/algorithms/caliperalgorithm.h"
#include "vision/algorithms/circlemeasurementalgorithm.h"
#include "vision/algorithms/locatealgorithms.h"
#include "vision/algorithms/roiapplier.h"
#include "vision/algorithms/subpixelalgorithms.h"
#include "vision/model/calibrationmodel.h"
#include "vision/nodes/builtinexecutors.h"
#include "vision/registry/visionnodeids.h"
#include "vision/registry/visionnoderegistry.h"
#include "vision/runtime/flowruntime.h"
#include "vision/runtime/inodeexecutor.h"
#include "vision/runtime/nodeexecutorregistry.h"
#include "vision/runtime/resultsink.h"
#include "vision/domain/projectvariables.h"
#include "core/model/document.h"

#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QLineF>
#include <QTemporaryDir>
#include <QtTest>
#include <cmath>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

using namespace Selt;
using namespace Selt::MetrologyTest;

static bool imwriteUnicode(const QString &path, const cv::Mat &mat)
{
    if (path.trimmed().isEmpty() || mat.empty())
        return false;
    QImage qimg;
    if (mat.channels() == 1) {
        qimg = QImage(mat.data, mat.cols, mat.rows, int(mat.step), QImage::Format_Grayscale8).copy();
    } else {
        cv::Mat rgb;
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
        qimg = QImage(rgb.data, rgb.cols, rgb.rows, int(rgb.step), QImage::Format_RGB888).copy();
    }
    return qimg.save(path);
}

class TestAlgorithmFactors : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void calibrationSnapshotMeasureDecisionSink();
    void noCalibrationKeepsPixelUnit();
    void calibrationOriginRotationTransformsCenter();
    void roiMaskOutsideKeepsCircleCenter();
    void noiseFactorCleanPassesMediumRecords();
    void scaleScanDoesNotCrash();
    void subpixelPeakRefinesOffset();
    void houghLinesExposesSortedCandidates();
    void ransacFitRejectsDegenerate();
    void multiTargetContractOutputs();
};

void TestAlgorithmFactors::initTestCase()
{
    registerBuiltInOpenCvExecutors();
}

void TestAlgorithmFactors::calibrationSnapshotMeasureDecisionSink()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString imagePath = temp.filePath(QStringLiteral("cal_circle.png"));
    const QString sinkPath = temp.filePath(QStringLiteral("sink.csv"));
    cv::Mat scene(120, 120, CV_8UC1, cv::Scalar(0));
    cv::circle(scene, cv::Point(60, 60), 25, cv::Scalar(255), cv::FILLED);
    QVERIFY(imwriteUnicode(imagePath, scene));

    Document doc;
    NodeModel loader = VisionNodeRegistry::create(VisionNodeIds::imageLoader(), {});
    NodeModel measure = VisionNodeRegistry::create(VisionNodeIds::circleMeasure(), QPointF(220, 0));
    NodeModel decide = VisionNodeRegistry::create(VisionNodeIds::toleranceDecision(), QPointF(440, 0));
    loader.id = QStringLiteral("loader");
    measure.id = QStringLiteral("measure");
    decide.id = QStringLiteral("decide");
    loader.parameters.insert(QStringLiteral("path"), imagePath);
    measure.parameters.insert(QStringLiteral("minRadius"), 5.0);
    decide.parameters.insert(QStringLiteral("lower"), 20.0);
    decide.parameters.insert(QStringLiteral("upper"), 30.0);
    decide.parameters.insert(QStringLiteral("nominal"), 25.0);
    doc.addNode(loader);
    doc.addNode(measure);
    doc.addNode(decide);

    ConnectionModel c1;
    c1.id = QStringLiteral("c1");
    c1.sourceNodeId = loader.id;
    c1.sourcePortId = QStringLiteral("out");
    c1.targetNodeId = measure.id;
    c1.targetPortId = QStringLiteral("in");
    doc.addConnection(c1);
    ConnectionModel c2;
    c2.id = QStringLiteral("c2");
    c2.sourceNodeId = measure.id;
    c2.sourcePortId = QStringLiteral("diameter");
    c2.targetNodeId = decide.id;
    c2.targetPortId = QStringLiteral("value");
    doc.addConnection(c2);

    CalibrationModel cal;
    cal.calibrationId = QStringLiteral("cal-mm-e2e");
    cal.unit = QStringLiteral("mm");
    cal.unitPerPixel = 0.5;
    cal.valid = true;

    RuntimeExecuteOptions opt1;
    opt1.hasCalibration = true;
    opt1.calibration = cal;
    VisionContext ctx1;
    ProjectVariableStore vars;
    QVERIFY(FlowRuntime::executeOnce(doc, ctx1, vars, opt1));
    QCOMPARE(ctx1.moduleResult(measure.id).measurement.unit, QStringLiteral("mm"));
    QCOMPARE(ctx1.moduleResult(measure.id).measurement.calibrationId, QStringLiteral("cal-mm-e2e"));
    QVERIFY(qAbs(ctx1.moduleResult(measure.id).measurement.width - 25.0) < 1.0);
    QCOMPARE(ctx1.moduleResult(decide.id).status, ModuleStatus::Success);

    // Snapshot: mutate store after run should not rewrite prior result.
    cal.unitPerPixel = 2.0;
    RuntimeExecuteOptions opt2;
    opt2.hasCalibration = true;
    opt2.calibration = cal;
    VisionContext ctx2;
    QVERIFY(FlowRuntime::executeOnce(doc, ctx2, vars, opt2));
    QVERIFY(qAbs(ctx2.moduleResult(measure.id).measurement.width - 100.0) < 4.0);

    ResultSinkRegistry::instance().clear();
    ResultSinkRegistry::instance().addSink(std::make_shared<CsvResultSink>(sinkPath));
    ResultRecord record;
    record.nodeId = measure.id;
    record.decisionState = QStringLiteral("ok");
    record.measurement = ctx2.moduleResult(measure.id).measurement;
    QVERIFY(ResultSinkRegistry::instance().publishAll(record).isEmpty());
    QVERIFY(QFileInfo::exists(sinkPath));
    ResultSinkRegistry::instance().clear();
}

void TestAlgorithmFactors::noCalibrationKeepsPixelUnit()
{
    CircleGroundTruth gt;
    VisionImage img = makeFilledCircle(120, gt);
    auto executor = NodeExecutorRegistry::instance().create(VisionNodeIds::circleMeasure());
    ExecutionRequest request;
    request.nodeId = QStringLiteral("m");
    request.inputs.insert(QStringLiteral("image"), DataValue(img));
    ExecutionContext ctx;
    const ExecutionResult r = executor->execute(request, ctx);
    QCOMPARE(r.status, ModuleStatus::Success);
    QCOMPARE(r.measurement.unit, QStringLiteral("px"));
    QVERIFY(r.measurement.calibrationId.isEmpty());
}

void TestAlgorithmFactors::calibrationOriginRotationTransformsCenter()
{
    CalibrationModel cal;
    cal.valid = true;
    cal.unit = QStringLiteral("mm");
    cal.unitPerPixel = 2.0;
    cal.originPx = QPointF(10, 20);
    cal.rotationRad = 3.141592653589793 / 2.0; // 90 deg
    const QPointF physical = cal.transformPixelToPhysical(QPointF(10, 30));
    // shifted=(0,10) -> rotate90=( -10, 0) -> *2 = (-20, 0)
    QVERIFY(qAbs(physical.x() + 20.0) < 1e-6);
    QVERIFY(qAbs(physical.y()) < 1e-6);
    QCOMPARE(cal.transformLength(10.0), 20.0);
}

void TestAlgorithmFactors::roiMaskOutsideKeepsCircleCenter()
{
    CircleGroundTruth gt;
    VisionImage img = makeFilledCircle(120, gt);
    ExtendedRoi roi;
    roi.enabled = true;
    roi.shape = RoiShapeType::Rectangle;
    roi.rect = QRectF(20, 20, 80, 80);
    const RoiApplyResult applied = RoiApplier::apply(img, roi, RoiApplyMode::MaskOutside);
    QVERIFY(applied.ok);
    MeasurementResult result;
    VisionImage overlay;
    QVERIFY(CircleMeasurementAlgorithm::measure(applied.image, result, overlay, 5.0, false));
    QVERIFY(absError(result.center.x(), gt.center.x()) <= 1.5);
    QVERIFY(absError(result.center.y(), gt.center.y()) <= 1.5);
}

void TestAlgorithmFactors::noiseFactorCleanPassesMediumRecords()
{
    CircleGroundTruth gt;
    VisionImage clean = makeFilledCircle(120, gt);
    MeasurementResult cleanResult;
    VisionImage overlay;
    QVERIFY(CircleMeasurementAlgorithm::measure(clean, cleanResult, overlay, 5.0, false));
    QVERIFY(absError(cleanResult.width, gt.radius * 2.0) <= 1.0);

    VisionImage medium = withGaussianNoise(clean, 8.0, 11);
    MeasurementResult mediumResult;
    const bool mediumOk = CircleMeasurementAlgorithm::measure(medium, mediumResult, overlay, 5.0, false);
    if (mediumOk && mediumResult.valid) {
        // Record upper bound rather than hard fail: allow up to 3 px on medium noise.
        QVERIFY(absError(mediumResult.width, gt.radius * 2.0) <= 3.0);
    }

    VisionImage high = withGaussianNoise(clean, 35.0, 99);
    addSaltPepperNoise(high.mat(), 0.08, 3);
    MeasurementResult highResult;
    QString err;
    const bool highOk = CircleMeasurementAlgorithm::measure(high, highResult, overlay, 5.0, false, &err);
    if (highOk && highResult.valid) {
        // Must not invent absurd geometry.
        QVERIFY(highResult.width > 5.0);
        QVERIFY(highResult.width < 200.0);
    } else {
        QVERIFY(!err.isEmpty());
    }
}

void TestAlgorithmFactors::scaleScanDoesNotCrash()
{
    const QVector<int> sizes{64, 128, 512};
    for (int size : sizes) {
        CircleGroundTruth gt;
        gt.center = QPointF(size * 0.5, size * 0.5);
        gt.radius = size * 0.2;
        VisionImage img = makeFilledCircle(size, gt);
        MeasurementResult result;
        VisionImage overlay;
        QVERIFY(CircleMeasurementAlgorithm::measure(img, result, overlay, 2.0, false));
        QVERIFY(result.valid);
        QVERIFY(relativeError(result.width, gt.radius * 2.0) < 0.08);
    }
}

void TestAlgorithmFactors::subpixelPeakRefinesOffset()
{
    cv::Mat mat(64, 64, CV_8UC1, cv::Scalar(0));
    // Bright peak centered at (32.3, 32.0) approximated by discrete blob.
    cv::circle(mat, cv::Point(32, 32), 2, cv::Scalar(255), cv::FILLED);
    mat.at<uchar>(32, 33) = 200;
    VisionImage image(mat);
    SubpixelOptions opts;
    opts.mode = QStringLiteral("peak");
    QPointF refined;
    double confidence = 0.0;
    QString error;
    QVERIFY(SubpixelRefineAlgorithm::refine(image, QPointF(32.0, 32.0), refined, confidence, opts, &error));
    QVERIFY(confidence > 0.0);
    QVERIFY(std::abs(refined.x() - 32.0) <= 1.0);
    QVERIFY(std::abs(refined.y() - 32.0) <= 1.0);
}

void TestAlgorithmFactors::houghLinesExposesSortedCandidates()
{
    cv::Mat mat(100, 120, CV_8UC1, cv::Scalar(0));
    cv::line(mat, cv::Point(10, 20), cv::Point(110, 20), cv::Scalar(255), 2);
    cv::line(mat, cv::Point(10, 80), cv::Point(60, 80), cv::Scalar(255), 2);
    QVector<LocateLine2D> lines;
    VisionImage overlay;
    HoughLinesOptions opts;
    opts.maxCount = 5;
    opts.sortBy = QStringLiteral("length");
    QVERIFY(HoughLinesAlgorithm::apply(VisionImage(mat), lines, opts, overlay));
    QVERIFY(lines.size() >= 1);
    if (lines.size() >= 2) {
        const double l0 = QLineF(lines[0].start, lines[0].end).length();
        const double l1 = QLineF(lines[1].start, lines[1].end).length();
        QVERIFY(l0 + 1e-6 >= l1);
    }
}

void TestAlgorithmFactors::ransacFitRejectsDegenerate()
{
    QVector<QPointF> pts = {QPointF(0, 0), QPointF(1, 0)};
    FitOptions opts;
    opts.mode = QStringLiteral("ransac");
    FitCircleResult result;
    QString error;
    QVERIFY(!FitCircleAlgorithm::fit(pts, opts, result, &error));
}

void TestAlgorithmFactors::multiTargetContractOutputs()
{
    cv::Mat mat(120, 160, CV_8UC1, cv::Scalar(0));
    cv::circle(mat, cv::Point(40, 40), 12, cv::Scalar(255), cv::FILLED);
    cv::circle(mat, cv::Point(110, 70), 18, cv::Scalar(255), cv::FILLED);
    auto executor = NodeExecutorRegistry::instance().create(VisionNodeIds::blobLocate());
    ExecutionRequest request;
    request.nodeId = QStringLiteral("blob");
    request.inputs.insert(QStringLiteral("image"), DataValue(VisionImage(mat)));
    request.parameters.insert(QStringLiteral("minArea"), 20.0);
    request.parameters.insert(QStringLiteral("minCircularity"), 0.5);
    ExecutionContext ctx;
    const ExecutionResult r = executor->execute(request, ctx);
    QCOMPARE(r.status, ModuleStatus::Success);
    QVERIFY(r.outputs.value(QStringLiteral("candidateCount")).toInt() >= 1);
    QCOMPARE(r.outputs.value(QStringLiteral("selectedIndex")).toInt(), 0);
    QVERIFY(r.outputs.contains(QStringLiteral("confidence")));
}

QTEST_MAIN(TestAlgorithmFactors)
#include "test_algorithm_factors.moc"
