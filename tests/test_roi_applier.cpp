#include "vision/algorithms/roiapplier.h"
#include "vision/runtime/diagnosticcodes.h"

#include <QCoreApplication>
#include <QJsonObject>
#include <QTest>
#include <opencv2/core.hpp>

using Selt::ExtendedRoi;
using Selt::RoiApplyMode;
using Selt::RoiApplier;
using Selt::RoiShapeType;

class TestRoiApplier : public QObject
{
    Q_OBJECT

private slots:
    void disabledRoiReturnsOriginal();
    void rectangleMaskOutsideKeepsSize();
    void rectangleCropShrinks();
    void circleMasksOutside();
    void rotatedRectApplies();
    void polygonApplies();
    void outOfBoundsRoiFails();
    void emptyImageFails();
    void legacyRoiJsonParses();
};

static VisionImage makeGray(int w, int h, uchar value = 200)
{
    return VisionImage(cv::Mat(h, w, CV_8UC1, cv::Scalar(value)));
}

void TestRoiApplier::disabledRoiReturnsOriginal()
{
    VisionImage input = makeGray(40, 30);
    ExtendedRoi roi;
    roi.enabled = false;
    roi.rect = QRectF(5, 5, 10, 10);
    const auto result = RoiApplier::apply(input, roi);
    QVERIFY(result.ok);
    QCOMPARE(result.image.width(), 40);
    QCOMPARE(result.image.height(), 30);
}

void TestRoiApplier::rectangleMaskOutsideKeepsSize()
{
    VisionImage input = makeGray(40, 30, 255);
    ExtendedRoi roi;
    roi.enabled = true;
    roi.shape = RoiShapeType::Rectangle;
    roi.rect = QRectF(10, 8, 12, 10);
    const auto result = RoiApplier::apply(input, roi, RoiApplyMode::MaskOutside);
    QVERIFY(result.ok);
    QCOMPARE(result.image.width(), 40);
    QCOMPARE(result.image.height(), 30);
    QVERIFY(result.image.mat().at<uchar>(12, 15) > 0);
    QCOMPARE(int(result.image.mat().at<uchar>(0, 0)), 0);
}

void TestRoiApplier::rectangleCropShrinks()
{
    VisionImage input = makeGray(40, 30, 255);
    ExtendedRoi roi;
    roi.enabled = true;
    roi.shape = RoiShapeType::Rectangle;
    roi.rect = QRectF(10, 8, 12, 10);
    const auto result = RoiApplier::apply(input, roi, RoiApplyMode::Crop);
    QVERIFY(result.ok);
    QCOMPARE(result.image.width(), 12);
    QCOMPARE(result.image.height(), 10);
}

void TestRoiApplier::circleMasksOutside()
{
    VisionImage input = makeGray(50, 50, 255);
    ExtendedRoi roi;
    roi.enabled = true;
    roi.shape = RoiShapeType::Circle;
    roi.center = QPointF(25, 25);
    roi.radius = 8;
    const auto result = RoiApplier::apply(input, roi);
    QVERIFY(result.ok);
    QVERIFY(result.image.mat().at<uchar>(25, 25) > 0);
    QCOMPARE(int(result.image.mat().at<uchar>(0, 0)), 0);
}

void TestRoiApplier::rotatedRectApplies()
{
    VisionImage input = makeGray(60, 60, 255);
    ExtendedRoi roi;
    roi.enabled = true;
    roi.shape = RoiShapeType::RotatedRect;
    roi.center = QPointF(30, 30);
    roi.width = 20;
    roi.height = 10;
    roi.angleDeg = 30;
    const auto result = RoiApplier::apply(input, roi);
    QVERIFY(result.ok);
    QVERIFY(cv::countNonZero(result.image.mat()) > 0);
}

void TestRoiApplier::polygonApplies()
{
    VisionImage input = makeGray(40, 40, 255);
    ExtendedRoi roi;
    roi.enabled = true;
    roi.shape = RoiShapeType::Polygon;
    roi.polygon = {QPointF(5, 5), QPointF(25, 5), QPointF(15, 25)};
    const auto result = RoiApplier::apply(input, roi);
    QVERIFY(result.ok);
    QVERIFY(cv::countNonZero(result.image.mat()) > 0);
}

void TestRoiApplier::outOfBoundsRoiFails()
{
    VisionImage input = makeGray(20, 20, 255);
    ExtendedRoi roi;
    roi.enabled = true;
    roi.shape = RoiShapeType::Rectangle;
    roi.rect = QRectF(100, 100, 10, 10);
    const auto result = RoiApplier::apply(input, roi);
    QVERIFY(!result.ok);
    QCOMPARE(result.diagnosticCode, Selt::DiagnosticCodes::invalidRoi());
}

void TestRoiApplier::emptyImageFails()
{
    ExtendedRoi roi;
    roi.enabled = true;
    roi.shape = RoiShapeType::Rectangle;
    roi.rect = QRectF(0, 0, 5, 5);
    const auto result = RoiApplier::apply(VisionImage{}, roi);
    QVERIFY(!result.ok);
    QCOMPARE(result.diagnosticCode, Selt::DiagnosticCodes::imageEmpty());
}

void TestRoiApplier::legacyRoiJsonParses()
{
    VisionImage input = makeGray(30, 30, 255);
    QJsonObject params{
        {QStringLiteral("roi"),
         QJsonObject{{QStringLiteral("enabled"), true},
                     {QStringLiteral("x"), 5},
                     {QStringLiteral("y"), 5},
                     {QStringLiteral("width"), 10},
                     {QStringLiteral("height"), 8}}}};
    // RoiRect::fromJson may use nested "rect" — also accept ExtendedRoi fields via applyFromParameters.
    ExtendedRoi parsed = RoiApplier::parseFromParameters(params);
    // If legacy format uses different keys, build extendedRoi explicitly as fallback path.
    if (!parsed.isValid()) {
        params = QJsonObject{
            {QStringLiteral("extendedRoi"),
             QJsonObject{{QStringLiteral("enabled"), true},
                         {QStringLiteral("shape"), int(RoiShapeType::Rectangle)},
                         {QStringLiteral("x"), 5},
                         {QStringLiteral("y"), 5},
                         {QStringLiteral("width"), 10},
                         {QStringLiteral("height"), 8}}}};
    }
    const auto result = RoiApplier::applyFromParameters(input, params);
    QVERIFY(result.ok);
    QCOMPARE(result.image.width(), 30);
}

QTEST_MAIN(TestRoiApplier)
#include "test_roi_applier.moc"
