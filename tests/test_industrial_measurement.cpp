#include "vision/algorithms/caliperalgorithm.h"
#include "vision/algorithms/circlemeasurementalgorithm.h"
#include "vision/algorithms/linemeasurementalgorithm.h"
#include "vision/algorithms/rectanglemeasurementalgorithm.h"
#include "vision/model/measurementdefinition.h"
#include "vision/model/visionimage.h"
#include "tests/metrologytesthelpers.h"

#include <cmath>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

using namespace Selt::MetrologyTest;

class TestIndustrialMeasurement : public QObject
{
    Q_OBJECT

private slots:
    void circleMeasuresFilledDisk();
    void lineMeasuresHorizontalEdge();
    void rectangleSupportsRotatedGeometry();
    void emptyImageFailsGracefully();
    void distanceDecisionOkNg();
    void caliperMeasuresBarWidth();
    void fitLineAndAnglePipelineGeometry();
    void circleBaselinePrecisionAndStability();
    void caliperBaselineDistanceError();
    void fitCircleRejectsCollinear();
    void fitCircleRansacHandlesOutliers();
};

void TestIndustrialMeasurement::circleMeasuresFilledDisk()
{
    const CircleGroundTruth gt;
    VisionImage img = makeFilledCircle(120, gt);
    MeasurementResult result;
    VisionImage overlay;
    QVERIFY(CircleMeasurementAlgorithm::measure(img, result, overlay, 5.0, true));
    QVERIFY(result.valid);
    QCOMPARE(result.measurementType, QStringLiteral("circle"));
    QVERIFY(absError(result.width, gt.radius * 2.0) <= 1.0);
}

void TestIndustrialMeasurement::lineMeasuresHorizontalEdge()
{
    const LineGroundTruth gt;
    VisionImage img = makeHorizontalLine(80, 120, gt, 2);
    MeasurementResult result;
    VisionImage overlay;
    QVERIFY(LineMeasurementAlgorithm::measure(img, result, overlay, 50, 150, true));
    QVERIFY(result.valid);
    QCOMPARE(result.measurementType, QStringLiteral("line"));
    QVERIFY(result.width > 80.0);
}

void TestIndustrialMeasurement::rectangleSupportsRotatedGeometry()
{
    VisionImage img = makeRotatedRectangle(160, QPointF(80, 80), 60, 30, 30);
    MeasurementResult result;
    VisionImage overlay;
    QVERIFY(RectangleMeasurementAlgorithm::measure(img, result, overlay, 100.0, true));
    QVERIFY(result.valid);
    QVERIFY(result.width > 20.0);
    QVERIFY(result.height > 10.0);
}

void TestIndustrialMeasurement::emptyImageFailsGracefully()
{
    MeasurementResult result;
    VisionImage overlay;
    QString error;
    QVERIFY(!CircleMeasurementAlgorithm::measure(VisionImage{}, result, overlay, 5.0, true, &error));
    QVERIFY(!error.isEmpty());
}

void TestIndustrialMeasurement::distanceDecisionOkNg()
{
    MeasurementDefinition def;
    def.hasTolerance = true;
    def.lower = 9.0;
    def.upper = 11.0;
    QCOMPARE(def.evaluateDecision(10.0), QStringLiteral("ok"));
    QCOMPARE(def.evaluateDecision(8.0), QStringLiteral("ng"));
}

void TestIndustrialMeasurement::caliperMeasuresBarWidth()
{
    BarGroundTruth gt;
    VisionImage img = makeVerticalBar(100, 140, gt);
    QVector<QPointF> edges;
    double distance = 0.0;
    VisionImage overlay;
    QString error;
    QVERIFY(CaliperAlgorithm::sample(img, gt.center, 0.0, 120.0, 8.0,
                                     edges, distance, overlay, &error));
    QVERIFY(edges.size() >= 1);
    if (edges.size() >= 2)
        QVERIFY(absError(distance, gt.widthPx) <= 1.5);
}

void TestIndustrialMeasurement::fitLineAndAnglePipelineGeometry()
{
    QVector<QPointF> linePts = {QPointF(0, 0), QPointF(50, 0), QPointF(100, 0)};
    QPointF start;
    QPointF end;
    double angle = 0.0;
    QString error;
    QVERIFY(FitLineAlgorithm::fit(linePts, start, end, angle, &error));
    double measured = 0.0;
    QVERIFY(AngleMeasureAlgorithm::fromLines(start, end, QPointF(0, 0), QPointF(0, 40), measured, &error));
    QVERIFY(qAbs(measured - 90.0) < 1e-3);
}

void TestIndustrialMeasurement::circleBaselinePrecisionAndStability()
{
    const CircleGroundTruth gt;
    VisionImage img = makeFilledCircle(120, gt);
    QVector<double> diameters;
    for (int i = 0; i < 8; ++i) {
        MeasurementResult result;
        VisionImage overlay;
        QVERIFY(CircleMeasurementAlgorithm::measure(img, result, overlay, 5.0, false));
        QVERIFY(result.valid);
        diameters.append(result.width);
        QVERIFY(absError(result.width, gt.radius * 2.0) <= 1.0);
    }
    const RepeatStats stats = computeRepeatStats(diameters);
    QVERIFY(stats.stddev <= 0.2);
}

void TestIndustrialMeasurement::caliperBaselineDistanceError()
{
    BarGroundTruth gt;
    VisionImage img = makeVerticalBar(100, 140, gt);
    CaliperOptions opts;
    opts.sampleStep = 0.5;
    opts.smoothSigma = 0.8;
    QVector<QPointF> edges;
    double distance = 0.0;
    double confidence = 0.0;
    VisionImage overlay;
    QString error;
    QVERIFY(CaliperAlgorithm::sample(img, gt.center, 0.0, 120.0, 8.0, opts,
                                     edges, distance, confidence, overlay, &error));
    QCOMPARE(edges.size(), 2);
    QVERIFY(absError(distance, gt.widthPx) <= 1.5);
    QVERIFY(confidence > 0.2);
}

void TestIndustrialMeasurement::fitCircleRejectsCollinear()
{
    QVector<QPointF> pts = {QPointF(0, 0), QPointF(10, 0), QPointF(20, 0), QPointF(30, 0)};
    FitCircleResult result;
    QString error;
    QVERIFY(!FitCircleAlgorithm::fit(pts, FitOptions{}, result, &error));
    QVERIFY(error.contains(QStringLiteral("共线")));
}

void TestIndustrialMeasurement::fitCircleRansacHandlesOutliers()
{
    QVector<QPointF> pts;
    const QPointF c(50, 50);
    const double r = 20.0;
    for (int i = 0; i < 36; ++i) {
        const double a = i * (2.0 * 3.141592653589793 / 36.0);
        pts.append(QPointF(c.x() + r * std::cos(a), c.y() + r * std::sin(a)));
    }
    // ~20% outliers
    pts.append(QPointF(5, 5));
    pts.append(QPointF(95, 5));
    pts.append(QPointF(5, 95));
    pts.append(QPointF(95, 95));
    pts.append(QPointF(10, 80));
    pts.append(QPointF(80, 10));
    pts.append(QPointF(0, 50));
    pts.append(QPointF(50, 0));

    FitOptions opts;
    opts.mode = QStringLiteral("ransac");
    opts.residualThreshold = 1.5;
    opts.maxIterations = 200;
    opts.minInlierRatio = 0.55;
    FitCircleResult result;
    QString error;
    QVERIFY(FitCircleAlgorithm::fit(pts, opts, result, &error));
    QVERIFY(absError(result.radius, r) <= 1.5);
    QVERIFY(absError(result.center.x(), c.x()) <= 1.5);
    QVERIFY(absError(result.center.y(), c.y()) <= 1.5);
    QVERIFY(result.inlierCount >= 30);
}

QTEST_MAIN(TestIndustrialMeasurement)
#include "test_industrial_measurement.moc"
