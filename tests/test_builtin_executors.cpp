#include "vision/nodes/builtinexecutors.h"
#include "vision/registry/visionnodeids.h"
#include "vision/runtime/inodeexecutor.h"
#include "vision/runtime/nodeexecutorregistry.h"
#include "vision/runtime/diagnosticcodes.h"
#include "vision/model/calibrationmodel.h"
#include "vision/data/datatype.h"
#include "vision/model/contourdata.h"

#include <QCoreApplication>
#include <QHash>
#include <QJsonObject>
#include <QTest>
#include <QtMath>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

using namespace Selt;

class TestBuiltinExecutors : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void gaussianBlurSucceeds();
    void otsuThresholdSucceeds();
    void emptyImageFailsWithDiagnostic();
    void maskCombineAnd();
    void boolLogicNodes();
    void numericCompareAndTolerance();
    void findContoursProducesContourPort();
    void angleMeasureFromParams();
    void fitCircleExecutorSucceeds();
    void fitCircleExecutorRejectsCollinear();
    void parallelDistanceFailsOnDegenerate();
    void distanceMeasureAppliesCalibration();
    void circleMeasureEmptyFailsWithDiagnostic();
    void caliperMeasureFindsEdge();
    void subpixelRefineMarksCapabilityLimited();
    void subpixelRefineWithImageSucceeds();
};

void TestBuiltinExecutors::initTestCase()
{
    registerBuiltInOpenCvExecutors();
}

static VisionImage makeBlobImage()
{
    cv::Mat mat(80, 80, CV_8UC1, cv::Scalar(0));
    cv::circle(mat, cv::Point(40, 40), 18, cv::Scalar(255), cv::FILLED);
    return VisionImage(mat);
}

static ExecutionResult runNode(const QString &typeId,
                               const QHash<QString, DataValue> &inputs = {},
                               const QJsonObject &params = {},
                               ExecutionContext *contextOverride = nullptr)
{
    auto executor = NodeExecutorRegistry::instance().create(typeId);
    ExecutionRequest request;
    request.nodeId = typeId;
    request.typeId = typeId;
    request.inputs = inputs;
    request.parameters = params;
    ExecutionContext local;
    ExecutionContext &context = contextOverride ? *contextOverride : local;
    return executor->execute(request, context);
}

void TestBuiltinExecutors::gaussianBlurSucceeds()
{
    VisionImage input = makeBlobImage();
    const ExecutionResult r = runNode(VisionNodeIds::gaussianBlur(),
                                      {{QStringLiteral("image"), DataValue(input)}},
                                      QJsonObject{{QStringLiteral("ksizeX"), 5},
                                                  {QStringLiteral("ksizeY"), 5}});
    QCOMPARE(r.status, ModuleStatus::Success);
    QVERIFY(!r.outputs.value(QStringLiteral("image")).toImage().isEmpty());
}

void TestBuiltinExecutors::otsuThresholdSucceeds()
{
    const ExecutionResult r = runNode(VisionNodeIds::otsuThreshold(),
                                      {{QStringLiteral("image"), DataValue(makeBlobImage())}});
    QCOMPARE(r.status, ModuleStatus::Success);
    QCOMPARE(r.outputs.value(QStringLiteral("image")).toImage().channels(), 1);
}

void TestBuiltinExecutors::emptyImageFailsWithDiagnostic()
{
    const ExecutionResult r = runNode(VisionNodeIds::medianBlur(),
                                      {{QStringLiteral("image"), DataValue(VisionImage{})}});
    QCOMPARE(r.status, ModuleStatus::Failed);
    QVERIFY(!r.errorMessage.isEmpty());
}

void TestBuiltinExecutors::maskCombineAnd()
{
    VisionImage a = makeBlobImage();
    VisionImage b = makeBlobImage();
    const ExecutionResult r = runNode(VisionNodeIds::maskCombine(),
                                      {{QStringLiteral("imageA"), DataValue(a)},
                                       {QStringLiteral("imageB"), DataValue(b)}},
                                      QJsonObject{{QStringLiteral("op"), QStringLiteral("and")}});
    QCOMPARE(r.status, ModuleStatus::Success);
}

void TestBuiltinExecutors::boolLogicNodes()
{
    {
        const ExecutionResult r = runNode(VisionNodeIds::boolAnd(),
                                          {{QStringLiteral("a"), DataValue(true)},
                                           {QStringLiteral("b"), DataValue(false)}});
        QCOMPARE(r.status, ModuleStatus::Success);
        QCOMPARE(r.outputs.value(QStringLiteral("ok")).toBool(), false);
    }
    {
        const ExecutionResult r = runNode(VisionNodeIds::boolOr(),
                                          {{QStringLiteral("a"), DataValue(true)},
                                           {QStringLiteral("b"), DataValue(false)}});
        QCOMPARE(r.outputs.value(QStringLiteral("ok")).toBool(), true);
    }
    {
        const ExecutionResult r = runNode(VisionNodeIds::boolNot(),
                                          {{QStringLiteral("a"), DataValue(true)}});
        QCOMPARE(r.outputs.value(QStringLiteral("ok")).toBool(), false);
    }
}

void TestBuiltinExecutors::numericCompareAndTolerance()
{
    const ExecutionResult cmp = runNode(VisionNodeIds::numericCompare(),
                                        {{QStringLiteral("value"), DataValue(10.0)}},
                                        QJsonObject{{QStringLiteral("reference"), 10.0},
                                                    {QStringLiteral("op"), QStringLiteral("eq")}});
    QCOMPARE(cmp.status, ModuleStatus::Success);
    QVERIFY(cmp.outputs.value(QStringLiteral("ok")).toBool());

    const ExecutionResult tol = runNode(VisionNodeIds::toleranceDecision(),
                                        {{QStringLiteral("value"), DataValue(50.0)}},
                                        QJsonObject{{QStringLiteral("lower"), 40.0},
                                                    {QStringLiteral("upper"), 60.0},
                                                    {QStringLiteral("nominal"), 50.0}});
    QCOMPARE(tol.status, ModuleStatus::Success);
    QVERIFY(tol.outputs.value(QStringLiteral("ok")).toBool());
}

void TestBuiltinExecutors::findContoursProducesContourPort()
{
    const ExecutionResult r = runNode(VisionNodeIds::findContours(),
                                      {{QStringLiteral("image"), DataValue(makeBlobImage())}});
    QCOMPARE(r.status, ModuleStatus::Success);
    QVERIFY(r.outputs.value(QStringLiteral("count")).toInt() >= 1);
    QVERIFY(!r.outputs.value(QStringLiteral("contours")).toContours().contours.isEmpty());
}

void TestBuiltinExecutors::angleMeasureFromParams()
{
    const ExecutionResult r = runNode(VisionNodeIds::angleMeasure(), {},
                                      QJsonObject{{QStringLiteral("x1"), 0.0},
                                                  {QStringLiteral("y1"), 0.0},
                                                  {QStringLiteral("x2"), 100.0},
                                                  {QStringLiteral("y2"), 0.0},
                                                  {QStringLiteral("x3"), 0.0},
                                                  {QStringLiteral("y3"), 0.0},
                                                  {QStringLiteral("x4"), 0.0},
                                                  {QStringLiteral("y4"), 100.0}});
    QCOMPARE(r.status, ModuleStatus::Success);
    QVERIFY(qAbs(r.outputs.value(QStringLiteral("angle")).toReal() - 90.0) < 1e-3);
    QCOMPARE(r.measurement.unit, QStringLiteral("deg"));
}

void TestBuiltinExecutors::fitCircleExecutorSucceeds()
{
    ContourData contour;
    for (int i = 0; i < 36; ++i) {
        const double a = i * 10.0 * 3.141592653589793 / 180.0;
        contour.points.append(QPointF(40 + 15 * std::cos(a), 40 + 15 * std::sin(a)));
    }
    ContourList list;
    list.contours.append(contour);
    const ExecutionResult r = runNode(VisionNodeIds::fitCircle(),
                                      {{QStringLiteral("contours"), DataValue(list)}});
    QCOMPARE(r.status, ModuleStatus::Success);
    QVERIFY(r.measurement.valid);
    QCOMPARE(r.measurement.unit, QStringLiteral("px"));
    QVERIFY(qAbs(r.outputs.value(QStringLiteral("radius")).toReal() - 15.0) < 1.0);
}

void TestBuiltinExecutors::fitCircleExecutorRejectsCollinear()
{
    ContourData contour;
    for (int i = 0; i < 6; ++i)
        contour.points.append(QPointF(i * 10.0, 5.0));
    ContourList list;
    list.contours.append(contour);
    const ExecutionResult r = runNode(VisionNodeIds::fitCircle(),
                                      {{QStringLiteral("contours"), DataValue(list)}});
    QCOMPARE(r.status, ModuleStatus::Failed);
    QVERIFY(!r.measurement.valid);
    QCOMPARE(r.diagnosticCode, DiagnosticCodes::degenerateGeometry());
}

void TestBuiltinExecutors::parallelDistanceFailsOnDegenerate()
{
    Line2D a;
    a.start = QPointF(0, 0);
    a.end = QPointF(0, 0);
    Line2D b;
    b.start = QPointF(0, 10);
    b.end = QPointF(20, 10);
    const ExecutionResult r = runNode(VisionNodeIds::parallelDistance(),
                                      {{QStringLiteral("line1"), DataValue(a)},
                                       {QStringLiteral("line2"), DataValue(b)}});
    QCOMPARE(r.status, ModuleStatus::Failed);
    QCOMPARE(r.diagnosticCode, DiagnosticCodes::degenerateGeometry());
}

void TestBuiltinExecutors::distanceMeasureAppliesCalibration()
{
    ExecutionContext ctx;
    CalibrationModel cal;
    cal.calibrationId = QStringLiteral("cal-mm");
    cal.unit = QStringLiteral("mm");
    cal.unitPerPixel = 0.5;
    cal.valid = true;
    ctx.setCalibration(cal);
    const ExecutionResult r = runNode(VisionNodeIds::distanceMeasure(), {},
                                      QJsonObject{{QStringLiteral("x1"), 0.0},
                                                  {QStringLiteral("y1"), 0.0},
                                                  {QStringLiteral("x2"), 100.0},
                                                  {QStringLiteral("y2"), 0.0}},
                                      &ctx);
    QCOMPARE(r.status, ModuleStatus::Success);
    QCOMPARE(r.measurement.unit, QStringLiteral("mm"));
    QCOMPARE(r.measurement.calibrationId, QStringLiteral("cal-mm"));
    QVERIFY(qAbs(r.measurement.width - 50.0) < 1e-6);
}

void TestBuiltinExecutors::circleMeasureEmptyFailsWithDiagnostic()
{
    const ExecutionResult r = runNode(VisionNodeIds::circleMeasure(),
                                      {{QStringLiteral("image"), DataValue(VisionImage{})}});
    QCOMPARE(r.status, ModuleStatus::Failed);
    QCOMPARE(r.diagnosticCode, DiagnosticCodes::imageEmpty());
    QVERIFY(!r.measurement.valid);
}

void TestBuiltinExecutors::caliperMeasureFindsEdge()
{
    cv::Mat mat(100, 140, CV_8UC1, cv::Scalar(15));
    cv::rectangle(mat, cv::Rect(35, 20, 70, 60), cv::Scalar(230), cv::FILLED);
    const ExecutionResult r = runNode(VisionNodeIds::caliperMeasure(),
                                      {{QStringLiteral("image"), DataValue(VisionImage(mat))}},
                                      QJsonObject{{QStringLiteral("cx"), 70.0},
                                                  {QStringLiteral("cy"), 50.0},
                                                  {QStringLiteral("angle"), 0.0},
                                                  {QStringLiteral("length"), 110.0},
                                                  {QStringLiteral("width"), 8.0}});
    QCOMPARE(r.status, ModuleStatus::Success);
    QVERIFY(r.measurement.valid);
    QCOMPARE(r.measurement.unit, QStringLiteral("px"));
}

void TestBuiltinExecutors::subpixelRefineMarksCapabilityLimited()
{
    const ExecutionResult r = runNode(VisionNodeIds::subpixelRefine(), {},
                                      QJsonObject{{QStringLiteral("x"), 12.0},
                                                  {QStringLiteral("y"), 34.0}});
    QCOMPARE(r.status, ModuleStatus::Success);
    QCOMPARE(r.diagnosticCode, DiagnosticCodes::capabilityLimited());
    QCOMPARE(r.outputs.value(QStringLiteral("mode")).toString(), QStringLiteral("passthrough"));
}

void TestBuiltinExecutors::subpixelRefineWithImageSucceeds()
{
    cv::Mat mat(48, 48, CV_8UC1, cv::Scalar(0));
    cv::circle(mat, cv::Point(24, 24), 3, cv::Scalar(255), cv::FILLED);
    const ExecutionResult r = runNode(VisionNodeIds::subpixelRefine(),
                                      {{QStringLiteral("image"), DataValue(VisionImage(mat))},
                                       {QStringLiteral("point"), DataValue(QPointF(24.0, 24.0))}},
                                      QJsonObject{{QStringLiteral("mode"), QStringLiteral("peak")}});
    QCOMPARE(r.status, ModuleStatus::Success);
    QVERIFY(r.diagnosticCode != DiagnosticCodes::capabilityLimited());
    QCOMPARE(r.outputs.value(QStringLiteral("mode")).toString(), QStringLiteral("peak"));
    QVERIFY(r.outputs.value(QStringLiteral("confidence")).toReal() > 0.0);
}

QTEST_MAIN(TestBuiltinExecutors)
#include "test_builtin_executors.moc"
