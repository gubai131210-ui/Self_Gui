#include "vision/algorithms/caliperalgorithm.h"
#include "vision/algorithms/regionalgorithms.h"
#include "vision/algorithms/roiapplier.h"
#include "vision/runtime/diagnosticcodes.h"

#include <QCoreApplication>
#include <QJsonObject>
#include <QTest>
#include <cmath>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

using Selt::ExtendedRoi;
using Selt::RoiApplyMode;
using Selt::RoiApplier;
using Selt::RoiShapeType;

class TestBlobFindCircleOptimize : public QObject
{
    Q_OBJECT

private slots:
    void blobFullImageFindsDarkDisk();
    void blobRectRoiFindsDarkDiskGlobalCenter();
    void blobCircleRoiCropMaskedFindsDisk();
    void blobMaskOutsideGrayDoesNotKillOtsu();
    void findCircleAutoFindsObviousDisk();
    void findCircleToleratesOffsetExpectedRadius();
    void cropMaskedReportsOriginOffset();
};

static VisionImage makeSceneWithDarkDisk(int w, int h, QPoint center, int radius)
{
    cv::Mat mat(h, w, CV_8UC1, cv::Scalar(200));
    cv::circle(mat, cv::Point(center.x(), center.y()), radius, cv::Scalar(30), cv::FILLED);
    return VisionImage(mat);
}

void TestBlobFindCircleOptimize::blobFullImageFindsDarkDisk()
{
    VisionImage input = makeSceneWithDarkDisk(200, 160, QPoint(120, 80), 25);
    BlobAnalyzeAlgorithm::Options opts;
    opts.minArea = 0.0;
    opts.maxArea = 0.0;
    opts.polarity = QStringLiteral("dark");
    opts.thresholdMode = QStringLiteral("auto");
    RegionData region;
    QVector<double> scores;
    VisionImage overlay;
    int selected = -1;
    int unfiltered = 0;
    QString err;
    QVERIFY(BlobAnalyzeAlgorithm::apply(input, opts, region, scores, overlay, &selected, &unfiltered, &err));
    QVERIFY2(region.regions.size() >= 1, qPrintable(err));
    QVERIFY(unfiltered >= 1);
    QCOMPARE(selected, 0);
    QVERIFY(qAbs(region.regions.first().centroid.x() - 120.0) < 3.0);
    QVERIFY(qAbs(region.regions.first().centroid.y() - 80.0) < 3.0);
}

void TestBlobFindCircleOptimize::blobRectRoiFindsDarkDiskGlobalCenter()
{
    VisionImage input = makeSceneWithDarkDisk(200, 160, QPoint(120, 80), 25);
    ExtendedRoi roi;
    roi.enabled = true;
    roi.shape = RoiShapeType::Rectangle;
    roi.rect = QRectF(90, 50, 70, 70);
    const auto applied = RoiApplier::apply(input, roi, RoiApplyMode::CropMasked);
    QVERIFY(applied.ok);
    QVERIFY(applied.originOffset.x() > 0.0);

    BlobAnalyzeAlgorithm::Options opts;
    opts.originOffset = applied.originOffset;
    opts.polarity = QStringLiteral("dark");
    RegionData region;
    QVector<double> scores;
    VisionImage overlay;
    int selected = -1;
    int unfiltered = 0;
    QString err;
    QVERIFY(BlobAnalyzeAlgorithm::apply(applied.image, opts, region, scores, overlay, &selected,
                                        &unfiltered, &err));
    QVERIFY2(!region.regions.isEmpty(), qPrintable(err));
    QVERIFY(qAbs(region.regions.first().centroid.x() - 120.0) < 4.0);
    QVERIFY(qAbs(region.regions.first().centroid.y() - 80.0) < 4.0);
}

void TestBlobFindCircleOptimize::blobCircleRoiCropMaskedFindsDisk()
{
    VisionImage input = makeSceneWithDarkDisk(200, 160, QPoint(120, 80), 25);
    ExtendedRoi roi;
    roi.enabled = true;
    roi.shape = RoiShapeType::Circle;
    roi.center = QPointF(120, 80);
    roi.radius = 40;
    const auto applied = RoiApplier::apply(input, roi, RoiApplyMode::CropMasked);
    QVERIFY(applied.ok);

    BlobAnalyzeAlgorithm::Options opts;
    opts.originOffset = applied.originOffset;
    opts.polarity = QStringLiteral("dark");
    RegionData region;
    QVector<double> scores;
    VisionImage overlay;
    int selected = -1;
    int unfiltered = 0;
    QString err;
    QVERIFY(BlobAnalyzeAlgorithm::apply(applied.image, opts, region, scores, overlay, &selected,
                                        &unfiltered, &err));
    QVERIFY2(!region.regions.isEmpty(), qPrintable(err));
    QVERIFY(qAbs(region.regions.first().centroid.x() - 120.0) < 4.0);
}

void TestBlobFindCircleOptimize::blobMaskOutsideGrayDoesNotKillOtsu()
{
    VisionImage input = makeSceneWithDarkDisk(240, 200, QPoint(180, 140), 20);
    ExtendedRoi roi;
    roi.enabled = true;
    roi.shape = RoiShapeType::Rectangle;
    roi.rect = QRectF(150, 110, 60, 60);
    const auto cropped = RoiApplier::apply(input, roi, RoiApplyMode::CropMasked);
    QVERIFY(cropped.ok);

    BlobAnalyzeAlgorithm::Options opts;
    opts.polarity = QStringLiteral("dark");
    opts.originOffset = cropped.originOffset;
    RegionData regionCrop;
    QVector<double> scores;
    VisionImage overlay;
    int selected = -1;
    QString err;
    QVERIFY(BlobAnalyzeAlgorithm::apply(cropped.image, opts, regionCrop, scores, overlay, &selected,
                                        nullptr, &err));
    QVERIFY2(!regionCrop.regions.isEmpty(), qPrintable(err));
}

void TestBlobFindCircleOptimize::findCircleAutoFindsObviousDisk()
{
    VisionImage input = makeSceneWithDarkDisk(320, 240, QPoint(160, 120), 50);
    FindCircleByCalipersOptions opts;
    opts.searchMode = QStringLiteral("auto");
    opts.numCalipers = 16;
    opts.searchLength = 0.0;
    opts.numToIgnore = -1;
    opts.searchInward = true;
    FitCircleResult result;
    QVector<QPointF> edges;
    VisionImage overlay;
    FindCircleDiagnostics diag;
    QString err;
    QVERIFY2(FindCircleByCalipersAlgorithm::apply(input, QPointF(160, 120), 50.0, opts, result, edges,
                                                  overlay, &diag, &err),
             qPrintable(err));
    QVERIFY(edges.size() >= 3);
    QVERIFY(qAbs(result.center.x() - 160.0) < 4.0);
    QVERIFY(qAbs(result.center.y() - 120.0) < 4.0);
    QVERIFY(qAbs(result.radius - 50.0) < 4.0);
}

void TestBlobFindCircleOptimize::findCircleToleratesOffsetExpectedRadius()
{
    VisionImage input = makeSceneWithDarkDisk(320, 240, QPoint(160, 120), 50);
    FindCircleByCalipersOptions opts;
    opts.searchMode = QStringLiteral("auto");
    opts.numCalipers = 16;
    opts.searchLength = 0.0; // auto ~0.45*radius
    opts.numToIgnore = -1;
    opts.searchInward = false; // wrong direction; auto should retry reverse
    FitCircleResult result;
    QVector<QPointF> edges;
    VisionImage overlay;
    FindCircleDiagnostics diag;
    QString err;
    // Expected radius intentionally offset by 12 px.
    QVERIFY2(FindCircleByCalipersAlgorithm::apply(input, QPointF(162, 118), 62.0, opts, result, edges,
                                                  overlay, &diag, &err),
             qPrintable(err));
    QVERIFY(qAbs(result.radius - 50.0) < 6.0);
}

void TestBlobFindCircleOptimize::cropMaskedReportsOriginOffset()
{
    VisionImage input = makeSceneWithDarkDisk(100, 80, QPoint(70, 50), 10);
    ExtendedRoi roi;
    roi.enabled = true;
    roi.shape = RoiShapeType::Rectangle;
    roi.rect = QRectF(50, 30, 40, 40);
    const auto result = RoiApplier::apply(input, roi, RoiApplyMode::CropMasked);
    QVERIFY(result.ok);
    QCOMPARE(result.originOffset, QPointF(50, 30));
    QCOMPARE(result.image.width(), 40);
    QCOMPARE(result.image.height(), 40);
}

QTEST_MAIN(TestBlobFindCircleOptimize)
#include "test_blob_find_circle_optimize.moc"
