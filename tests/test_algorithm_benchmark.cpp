#include "tests/metrologytesthelpers.h"
#include "vision/algorithms/caliperalgorithm.h"
#include "vision/algorithms/circlemeasurementalgorithm.h"
#include "vision/algorithms/locatealgorithms.h"
#include "vision/nodes/builtinexecutors.h"
#include "vision/registry/visionnodeids.h"
#include "vision/registry/visionnoderegistry.h"
#include "vision/runtime/flowruntime.h"
#include "vision/domain/projectvariables.h"
#include "core/model/document.h"

#include <QElapsedTimer>
#include <QImage>
#include <QTemporaryDir>
#include <QtTest>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

using namespace Selt;
using namespace Selt::MetrologyTest;

static bool saveGray(const QString &path, const cv::Mat &mat)
{
    QImage qimg(mat.data, mat.cols, mat.rows, int(mat.step), QImage::Format_Grayscale8);
    return qimg.copy().save(path);
}

class TestAlgorithmBenchmark : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void singleNodeBaselines();
    void tenNodeDagBaseline();
    void roiVsFullImage();
};

void TestAlgorithmBenchmark::initTestCase()
{
    registerBuiltInOpenCvExecutors();
}

void TestAlgorithmBenchmark::singleNodeBaselines()
{
    cv::Mat scene(1080, 1920, CV_8UC1, cv::Scalar(0));
    cv::circle(scene, cv::Point(960, 540), 120, cv::Scalar(255), cv::FILLED);
    cv::rectangle(scene, cv::Rect(200, 400, 70, 200), cv::Scalar(240), cv::FILLED);
    VisionImage image(scene);

    QElapsedTimer t;
    MeasurementResult measure;
    VisionImage overlay;
    t.start();
    QVERIFY(CircleMeasurementAlgorithm::measure(image, measure, overlay, 10.0, false));
    const qint64 circleMs = t.elapsed();
    QVERIFY(circleMs >= 0);

    QVector<QPointF> edges;
    double distance = 0.0;
    double confidence = 0.0;
    t.restart();
    QVERIFY(CaliperAlgorithm::sample(image, QPointF(235, 500), 0.0, 160.0, 10.0, CaliperOptions{},
                                     edges, distance, confidence, overlay));
    const qint64 caliperMs = t.elapsed();
    QVERIFY(caliperMs >= 0);

    // Template match against a crop of the circle neighborhood.
    VisionImage templ(scene(cv::Rect(900, 480, 120, 120)).clone());
    QVector<QPointF> peaks;
    QVector<double> scores;
    t.restart();
    QVERIFY(TemplateMatchMultiAlgorithm::apply(image, templ, 0.5, 3, peaks, scores, overlay));
    const qint64 matchMs = t.elapsed();
    QVERIFY(matchMs >= 0);

    qDebug("benchmark single-node Debug ms: circle=%lld caliper=%lld template=%lld",
           static_cast<long long>(circleMs),
           static_cast<long long>(caliperMs),
           static_cast<long long>(matchMs));
    // Soft Debug ceilings (not production Release gates).
    QVERIFY(circleMs < 5000);
    QVERIFY(caliperMs < 5000);
    QVERIFY(matchMs < 15000);
}

void TestAlgorithmBenchmark::tenNodeDagBaseline()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString path = temp.filePath(QStringLiteral("bench.png"));
    cv::Mat scene(480, 640, CV_8UC1, cv::Scalar(0));
    cv::circle(scene, cv::Point(320, 240), 40, cv::Scalar(255), cv::FILLED);
    QVERIFY(saveGray(path, scene));

    Document doc;
    NodeModel loader = VisionNodeRegistry::create(VisionNodeIds::imageLoader(), {});
    loader.id = QStringLiteral("n0");
    loader.parameters.insert(QStringLiteral("path"), path);
    doc.addNode(loader);

    QString prev = loader.id;
    for (int i = 1; i <= 8; ++i) {
        NodeModel gray = VisionNodeRegistry::create(VisionNodeIds::grayscale(), QPointF(i * 80.0, 0));
        gray.id = QStringLiteral("n%1").arg(i);
        doc.addNode(gray);
        ConnectionModel c;
        c.id = QStringLiteral("c%1").arg(i);
        c.sourceNodeId = prev;
        c.sourcePortId = QStringLiteral("out");
        c.targetNodeId = gray.id;
        c.targetPortId = QStringLiteral("in");
        doc.addConnection(c);
        prev = gray.id;
    }
    NodeModel measure = VisionNodeRegistry::create(VisionNodeIds::circleMeasure(), QPointF(800, 0));
    measure.id = QStringLiteral("n9");
    doc.addNode(measure);
    ConnectionModel cm;
    cm.id = QStringLiteral("cm");
    cm.sourceNodeId = prev;
    cm.sourcePortId = QStringLiteral("out");
    cm.targetNodeId = measure.id;
    cm.targetPortId = QStringLiteral("in");
    doc.addConnection(cm);

    VisionContext ctx;
    ProjectVariableStore vars;
    QElapsedTimer t;
    t.start();
    QVERIFY(FlowRuntime::executeOnce(doc, ctx, vars, nullptr));
    const qint64 dagMs = t.elapsed();
    qDebug("benchmark 10-node DAG Debug ms=%lld", static_cast<long long>(dagMs));
    QVERIFY(dagMs < 10000);
    QCOMPARE(ctx.moduleResult(measure.id).status, ModuleStatus::Success);
}

void TestAlgorithmBenchmark::roiVsFullImage()
{
    cv::Mat scene(1080, 1920, CV_8UC1, cv::Scalar(0));
    cv::circle(scene, cv::Point(200, 200), 40, cv::Scalar(255), cv::FILLED);
    VisionImage full(scene);
    VisionImage roi(scene(cv::Rect(100, 100, 200, 200)).clone());

    MeasurementResult r1;
    MeasurementResult r2;
    VisionImage overlay;
    QElapsedTimer t;
    t.start();
    QVERIFY(CircleMeasurementAlgorithm::measure(full, r1, overlay, 5.0, false));
    const qint64 fullMs = t.elapsed();
    t.restart();
    QVERIFY(CircleMeasurementAlgorithm::measure(roi, r2, overlay, 5.0, false));
    const qint64 roiMs = t.elapsed();
    qDebug("benchmark ROI vs full Debug ms: full=%lld roi=%lld",
           static_cast<long long>(fullMs), static_cast<long long>(roiMs));
    QVERIFY(roiMs <= fullMs + 50); // allow timer noise
}

QTEST_MAIN(TestAlgorithmBenchmark)
#include "test_algorithm_benchmark.moc"
