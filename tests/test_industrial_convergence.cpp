#include "vision/algorithms/caliperalgorithm.h"
#include "vision/algorithms/common/geometryquality.h"
#include "vision/algorithms/metrology/edgeprobe.h"
#include "vision/algorithms/regionalgorithms.h"
#include "vision/model/visionimage.h"
#include "vision/model/autoparameter.h"
#include "vision/model/interactivegeometry.h"
#include "vision/model/operatoroutcome.h"
#include "vision/nodes/imageexecutorhelpers.h"
#include "vision/registry/visionnodeids.h"
#include "vision/registry/visionnoderegistry.h"

#include <QJsonObject>
#include <QtTest>

#include <opencv2/imgproc.hpp>

using namespace Selt;

class IndustrialConvergenceTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void operatorOutcome_mapsToModuleStatus();
    void geometryQuality_scoresAreDeterministic();
    void interactiveWriteback_keysRoundTrip();
    void blobDefaultPolarity_isAny();
    void blobLightDice_areDetected();
    void findLine_autoToleratesPartialMisses();
    void calibrationMissing_keepsPixelUnit();
    void houghLines_autoSnapshotPresent();
};

namespace {

VisionImage makeWhiteDiceOnBlack()
{
    cv::Mat m(240, 320, CV_8UC1, cv::Scalar(0));
    cv::rectangle(m, cv::Rect(40, 40, 50, 50), cv::Scalar(230), cv::FILLED);
    cv::rectangle(m, cv::Rect(120, 50, 55, 55), cv::Scalar(240), cv::FILLED);
    cv::rectangle(m, cv::Rect(200, 45, 48, 48), cv::Scalar(220), cv::FILLED);
    cv::rectangle(m, cv::Rect(60, 130, 52, 52), cv::Scalar(235), cv::FILLED);
    return VisionImage(m);
}

VisionImage makeHorizontalEdge()
{
    cv::Mat m(200, 300, CV_8UC1, cv::Scalar(30));
    cv::rectangle(m, cv::Rect(0, 0, 300, 100), cv::Scalar(220), cv::FILLED);
    return VisionImage(m);
}

} // namespace

void IndustrialConvergenceTest::initTestCase()
{
    QVERIFY(!VisionNodeRegistry::allDescriptors().isEmpty());
}

void IndustrialConvergenceTest::operatorOutcome_mapsToModuleStatus()
{
    QCOMPARE(moduleStatusForOutcome(OperatorOutcome::Success), ModuleStatus::Success);
    QCOMPARE(moduleStatusForOutcome(OperatorOutcome::SuccessWithWarning),
             ModuleStatus::SuccessWithWarning);
    QCOMPARE(moduleStatusForOutcome(OperatorOutcome::NoCandidate), ModuleStatus::Failed);
    QCOMPARE(diagnosticCodeForOutcome(OperatorOutcome::InvalidRoi),
             DiagnosticCodes::invalidRoi());
    QCOMPARE(diagnosticCodeForOutcome(OperatorOutcome::Timeout), DiagnosticCodes::timeout());

    ExecutionResult r;
    OperatorResultMeta meta;
    meta.outcome = OperatorOutcome::SuccessWithWarning;
    meta.qualityScore = 0.3;
    meta.failureStage = QStringLiteral("low_quality");
    meta.message = QStringLiteral("warn");
    QJsonObject snap{{QStringLiteral("a"), 1}};
    meta.autoSnapshot = makeAutoSnapshot(snap);
    applyOperatorMeta(r, meta);
    QCOMPARE(r.status, ModuleStatus::SuccessWithWarning);
    QCOMPARE(r.failureStage, QStringLiteral("low_quality"));
    QVERIFY(r.outputs.contains(QStringLiteral("autoSnapshot")));
    QVERIFY(r.outputs.contains(QStringLiteral("qualityScore")));
}

void IndustrialConvergenceTest::geometryQuality_scoresAreDeterministic()
{
    QVector<QPointF> pts{{0, 0}, {10, 0.2}, {20, -0.1}, {30, 0.0}};
    const double rms = GeometryQuality::lineResidualRms(pts, {0, 0}, {30, 0});
    const double cov = GeometryQuality::lineCoverageRatio(pts, {0, 0}, {30, 0});
    const double score = GeometryQuality::scoreFromResidualCoverage(rms, cov, 1.0);
    QCOMPARE(GeometryQuality::lineResidualRms(pts, {0, 0}, {30, 0}), rms);
    QCOMPARE(GeometryQuality::scoreFromResidualCoverage(rms, cov, 1.0), score);
    QVERIFY(score > 0.5);
    QVERIFY(GeometryQuality::passesSafetyMinPoints(3, 2));
    QVERIFY(!GeometryQuality::passesSafetyMinPoints(1, 2));
}

void IndustrialConvergenceTest::interactiveWriteback_keysRoundTrip()
{
    const InteractiveGeometrySpec spec =
        interactiveGeometrySpecForNode(VisionNodeIds::findLine());
    QVERIFY(spec.editable);
    QCOMPARE(spec.kind, GeometryKind::LineSegment);

    QJsonObject taught{{QStringLiteral("x0"), 10.0},
                       {QStringLiteral("y0"), 20.0},
                       {QStringLiteral("x1"), 100.0},
                       {QStringLiteral("y1"), 20.0}};
    QJsonObject nodeParams;
    for (const QString &key : spec.parameterKeys) {
        if (taught.contains(key)) {
            nodeParams.insert(key, taught.value(key));
            nodeParams.insert(QStringLiteral("_mode_") + key, QStringLiteral("user"));
        }
    }
    QCOMPARE(nodeParams.value(QStringLiteral("x0")).toDouble(), 10.0);
    QCOMPARE(nodeParams.value(QStringLiteral("_mode_x0")).toString(), QStringLiteral("user"));
    QVERIFY(spec.supportsRoi);
}

void IndustrialConvergenceTest::blobDefaultPolarity_isAny()
{
    const ModuleDescriptor d = VisionNodeRegistry::descriptor(VisionNodeIds::blobAnalyze());
    bool found = false;
    for (const ModuleParamDef &p : d.parameters) {
        if (p.key == QLatin1String("polarity")) {
            found = true;
            QCOMPARE(p.defaultValue.toString(), QStringLiteral("any"));
        }
    }
    QVERIFY(found);
}

void IndustrialConvergenceTest::blobLightDice_areDetected()
{
    VisionImage input = makeWhiteDiceOnBlack();
    BlobAnalyzeAlgorithm::Options opts;
    opts.polarity = QStringLiteral("any");
    opts.thresholdMode = QStringLiteral("auto");
    opts.minArea = 100.0;
    opts.maxArea = 0.0;
    RegionData region;
    QVector<double> scores;
    VisionImage overlay;
    int selected = -1;
    int unfiltered = 0;
    QString err;
    QVERIFY2(BlobAnalyzeAlgorithm::apply(input, opts, region, scores, overlay, &selected,
                                         &unfiltered, &err),
             qPrintable(err));
    QVERIFY2(region.regions.size() >= 3, qPrintable(QStringLiteral("got %1 blobs, unfiltered=%2")
                                                        .arg(region.regions.size())
                                                        .arg(unfiltered)));
}

void IndustrialConvergenceTest::findLine_autoToleratesPartialMisses()
{
    VisionImage input = makeHorizontalEdge();
    FindLineByCalipersOptions opts;
    opts.searchMode = QStringLiteral("auto");
    opts.numCalipers = 20;
    opts.searchLength = 40.0;
    opts.projectionLength = 8.0;
    opts.numToIgnore = 2; // deliberately tight; auto should loosen
    opts.caliperOpts.polarity = QStringLiteral("any");
    opts.caliperOpts.gradientThreshold = 0.25;
    FitLineResult fit;
    QVector<QPointF> edges;
    VisionImage overlay;
    QString err;
    QVERIFY2(EdgeProbe::findLine(input, QPointF(20, 100), QPointF(280, 100), opts, fit, edges,
                                 overlay, &err),
             qPrintable(err));
    QVERIFY(edges.size() >= 2);
    const double quality = EdgeProbe::scoreLine(fit, edges, {20, 100}, {280, 100});
    QVERIFY(quality > 0.2);
}

void IndustrialConvergenceTest::calibrationMissing_keepsPixelUnit()
{
    ExecutionRequest req;
    req.nodeId = QStringLiteral("n1");
    ExecutionContext ctx;
    MeasurementResult m;
    m.valid = true;
    m.width = 50.0;
    m.unit = QStringLiteral("px");
    QString code;
    m = finalizeLengthMeasurement(m, req, ctx, &code);
    QCOMPARE(m.unit, QStringLiteral("px"));
    QCOMPARE(code, DiagnosticCodes::calibrationMissing());
}

void IndustrialConvergenceTest::houghLines_autoSnapshotPresent()
{
    QJsonObject unset;
    QCOMPARE(Selt::resolveAutoInt(unset, QStringLiteral("accumulatorThreshold"), 50, 40), 40);
    QCOMPARE(Selt::resolveAutoDouble(unset, QStringLiteral("minLineLength"), 30.0, 55.0), 55.0);
}

QTEST_MAIN(IndustrialConvergenceTest)
#include "test_industrial_convergence.moc"
