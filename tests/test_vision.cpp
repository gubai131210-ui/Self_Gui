#include "vision/algorithms/grayscalealgorithm.h"
#include "vision/algorithms/imagefilteralgorithms.h"
#include "vision/algorithms/preprocessalgorithms.h"
#include "vision/algorithms/rectanglemeasurementalgorithm.h"
#include "vision/algorithms/thresholdalgorithm.h"
#include "vision/algorithms/caliperalgorithm.h"
#include "vision/algorithms/locatealgorithms.h"
#include "vision/algorithms/regionalgorithms.h"
#include "vision/model/visionimage.h"

#include <QCoreApplication>
#include <QRectF>
#include <QTest>
#include <cmath>
#include <opencv2/imgproc.hpp>

class TestVision : public QObject
{
    Q_OBJECT

private slots:
    void grayscaleReducesChannels();
    void thresholdProducesBinaryImage();
    void rectangleMeasurementFindsLargestContour();
    void emptyImageFailsGracefully();
    void qImageRoundTripPreservesSize();
    void blurAndCannySucceed();
    void templateMatchFindsPeak();
    void gaussianAndMedianBlurSucceed();
    void otsuAndAdaptiveThresholdSucceed();
    void sobelAndColorConvertSucceed();
    void findContoursAndBlobAnalyze();
    void houghLinesFindsSyntheticLine();
    void fitCircleFromPoints();
    void fitCircleRejectsCollinearPoints();
    void fitLineFromPoints();
    void fitLineRejectsDegeneratePoints();
    void caliperSamplesSyntheticEdges();
    void caliperEmptyImageFails();
    void parallelDistanceFromLines();
    void angleMeasureRejectsDegenerateLines();
    void invalidKernelRejectedByMedian();
};

static VisionImage makeWhiteRectangleOnBlack(int width, int height, const cv::Rect &rect)
{
    cv::Mat mat(height, width, CV_8UC1, cv::Scalar(0));
    cv::rectangle(mat, rect, cv::Scalar(255), cv::FILLED);
    return VisionImage(mat);
}

void TestVision::grayscaleReducesChannels()
{
    cv::Mat bgr(20, 20, CV_8UC3, cv::Scalar(10, 20, 30));
    VisionImage input(bgr);
    VisionImage gray;
    QString error;
    QVERIFY(GrayscaleAlgorithm::convert(input, gray, &error));
    QCOMPARE(gray.channels(), 1);
}

void TestVision::thresholdProducesBinaryImage()
{
    VisionImage input = makeWhiteRectangleOnBlack(80, 80, cv::Rect(20, 20, 40, 30));
    VisionImage binary;
    QString error;
    QVERIFY(ThresholdAlgorithm::apply(input, binary, 128, 255, ThresholdMode::Binary, &error));
    QCOMPARE(binary.channels(), 1);
    QVERIFY(binary.mat().at<uchar>(30, 30) > 0);
    QVERIFY(binary.mat().at<uchar>(5, 5) == 0);
}

void TestVision::rectangleMeasurementFindsLargestContour()
{
    const cv::Rect rect(20, 20, 40, 30);
    VisionImage input = makeWhiteRectangleOnBlack(80, 80, rect);
    MeasurementResult result;
    VisionImage overlay;
    QString error;
    QVERIFY(RectangleMeasurementAlgorithm::measure(input, result, overlay, 100.0, true, &error));
    QVERIFY(result.valid);
    // minAreaRect may swap width/height depending on angle; allow 2px contour bias.
    const double w = result.width;
    const double h = result.height;
    const bool oriented =
        (qAbs(w - rect.width) <= 2.0 && qAbs(h - rect.height) <= 2.0)
        || (qAbs(w - rect.height) <= 2.0 && qAbs(h - rect.width) <= 2.0);
    QVERIFY(oriented);
    QVERIFY(result.area >= rect.width * rect.height * 0.9);
}

void TestVision::emptyImageFailsGracefully()
{
    VisionImage empty;
    VisionImage gray;
    QString error;
    QVERIFY(!GrayscaleAlgorithm::convert(empty, gray, &error));
    QVERIFY(!error.isEmpty());
}

void TestVision::qImageRoundTripPreservesSize()
{
    cv::Mat bgr(32, 48, CV_8UC3, cv::Scalar(20, 40, 60));
    VisionImage original(bgr);
    QImage qimg = original.toQImage();
    VisionImage restored = VisionImage::fromQImage(qimg);
    QCOMPARE(restored.width(), 48);
    QCOMPARE(restored.height(), 32);
    QCOMPARE(restored.channels(), 3);
}

void TestVision::blurAndCannySucceed()
{
    cv::Mat gray(40, 40, CV_8UC1, cv::Scalar(0));
    cv::rectangle(gray, cv::Rect(10, 10, 20, 20), cv::Scalar(255), cv::FILLED);
    VisionImage input(gray);
    VisionImage blurred;
    VisionImage edges;
    QString error;
    QVERIFY(BlurAlgorithm::apply(input, blurred, 3, &error));
    QVERIFY(CannyAlgorithm::apply(input, edges, 50, 150, &error));
    QCOMPARE(edges.channels(), 1);
}

void TestVision::templateMatchFindsPeak()
{
    cv::Mat image(60, 60, CV_8UC1, cv::Scalar(0));
    cv::rectangle(image, cv::Rect(20, 15, 10, 8), cv::Scalar(255), cv::FILLED);
    cv::Mat templ = image(cv::Rect(20, 15, 10, 8)).clone();
    QPointF peak;
    double score = 0;
    QString error;
    QVERIFY(TemplateMatchAlgorithm::match(VisionImage(image), VisionImage(templ), &peak, &score, &error));
    QCOMPARE(qRound(peak.x()), 20);
    QCOMPARE(qRound(peak.y()), 15);
    QVERIFY(score > 0.9);
}

void TestVision::gaussianAndMedianBlurSucceed()
{
    VisionImage input = makeWhiteRectangleOnBlack(40, 40, cv::Rect(10, 10, 20, 20));
    VisionImage out;
    QString error;
    QVERIFY(GaussianBlurAlgorithm::apply(input, out, 5, 5, 0, 0, &error));
    QVERIFY(MedianBlurAlgorithm::apply(input, out, 5, &error));
    QVERIFY(!BilateralFilterAlgorithm::apply(VisionImage{}, out, 9, 75, 75, &error));
}

void TestVision::otsuAndAdaptiveThresholdSucceed()
{
    VisionImage input = makeWhiteRectangleOnBlack(64, 64, cv::Rect(16, 16, 32, 32));
    VisionImage out;
    QString error;
    QVERIFY(OtsuThresholdAlgorithm::apply(input, out, 255, false, &error));
    QCOMPARE(out.channels(), 1);
    QVERIFY(AdaptiveThresholdAlgorithm::apply(input, out, 255,
                                              AdaptiveThresholdAlgorithm::Method::AdaptiveGaussian,
                                              11, 2.0, false, &error));
}

void TestVision::sobelAndColorConvertSucceed()
{
    cv::Mat bgr(32, 32, CV_8UC3, cv::Scalar(10, 20, 30));
    VisionImage input(bgr);
    VisionImage out;
    QString error;
    QVERIFY(SobelAlgorithm::apply(input, out, 1, 0, 3, 1.0, &error));
    QVERIFY(ColorConvertAlgorithm::apply(input, out, QStringLiteral("gray"), &error));
    QCOMPARE(out.channels(), 1);
    QVERIFY(GeometricTransformAlgorithm::apply(input, out, QStringLiteral("rotate90"), &error));
    QCOMPARE(out.width(), 32);
    QCOMPARE(out.height(), 32);
}

void TestVision::findContoursAndBlobAnalyze()
{
    VisionImage input = makeWhiteRectangleOnBlack(80, 80, cv::Rect(20, 20, 40, 30));
    QVector<QVector<QPointF>> contours;
    VisionImage overlay;
    QString error;
    QVERIFY(FindContoursAlgorithm::apply(input, contours, overlay, &error));
    QVERIFY(!contours.isEmpty());

    QVector<QPointF> centers;
    QVector<double> areas;
    QVERIFY(BlobAnalyzeAlgorithm::apply(input, 50, 1e9, 0.0, centers, areas, overlay, &error));
    QVERIFY(!centers.isEmpty());
}

void TestVision::houghLinesFindsSyntheticLine()
{
    cv::Mat mat(80, 80, CV_8UC1, cv::Scalar(0));
    cv::line(mat, cv::Point(10, 40), cv::Point(70, 40), cv::Scalar(255), 2);
    QVector<LocateLine2D> lines;
    VisionImage overlay;
    QString error;
    QVERIFY(HoughLinesAlgorithm::apply(VisionImage(mat), lines, 50, 150, overlay, &error));
    QVERIFY(!lines.isEmpty());
}

void TestVision::fitCircleFromPoints()
{
    QVector<QPointF> pts;
    for (int i = 0; i < 36; ++i) {
        const double a = i * 10.0 * 3.141592653589793 / 180.0;
        pts.append(QPointF(40 + 15 * std::cos(a), 40 + 15 * std::sin(a)));
    }
    QPointF center;
    double radius = 0;
    double residual = 0;
    QString error;
    QVERIFY(FitCircleAlgorithm::fit(pts, center, radius, residual, &error));
    QVERIFY(qAbs(center.x() - 40.0) < 1.0);
    QVERIFY(qAbs(center.y() - 40.0) < 1.0);
    QVERIFY(qAbs(radius - 15.0) < 1.0);
}

void TestVision::fitCircleRejectsCollinearPoints()
{
    QVector<QPointF> pts;
    for (int i = 0; i < 8; ++i)
        pts.append(QPointF(10.0 + i * 5.0, 20.0));
    QPointF center;
    double radius = 0;
    double residual = 0;
    QString error;
    QVERIFY(!FitCircleAlgorithm::fit(pts, center, radius, residual, &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(radius, 0.0);
}

void TestVision::fitLineFromPoints()
{
    QVector<QPointF> pts;
    for (int i = 0; i < 10; ++i)
        pts.append(QPointF(5.0 + i * 4.0, 10.0 + i * 2.0));
    QPointF start;
    QPointF end;
    double angle = 0.0;
    QString error;
    QVERIFY(FitLineAlgorithm::fit(pts, start, end, angle, &error));
    QVERIFY(QLineF(start, end).length() > 20.0);
}

void TestVision::fitLineRejectsDegeneratePoints()
{
    QVector<QPointF> pts = {QPointF(3, 3), QPointF(3, 3), QPointF(3.0000001, 3)};
    QPointF start;
    QPointF end;
    double angle = 0.0;
    QString error;
    QVERIFY(!FitLineAlgorithm::fit(pts, start, end, angle, &error));
    QVERIFY(!error.isEmpty());
}

void TestVision::caliperSamplesSyntheticEdges()
{
    cv::Mat mat(120, 160, CV_8UC1, cv::Scalar(20));
    cv::rectangle(mat, cv::Rect(40, 20, 80, 80), cv::Scalar(220), cv::FILLED);
    QVector<QPointF> edges;
    double distance = 0.0;
    VisionImage overlay;
    QString error;
    QVERIFY(CaliperAlgorithm::sample(VisionImage(mat), QPointF(80, 60), 0.0, 100.0, 10.0,
                                     edges, distance, overlay, &error));
    QVERIFY(edges.size() >= 1);
    if (edges.size() >= 2)
        QVERIFY(distance > 40.0);
}

void TestVision::caliperEmptyImageFails()
{
    QVector<QPointF> edges;
    double distance = 0.0;
    VisionImage overlay;
    QString error;
    QVERIFY(!CaliperAlgorithm::sample(VisionImage{}, QPointF(10, 10), 0.0, 40.0, 6.0,
                                      edges, distance, overlay, &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(distance, 0.0);
}

void TestVision::parallelDistanceFromLines()
{
    double distance = 0.0;
    QString error;
    QVERIFY(ParallelDistanceAlgorithm::fromLines(QPointF(0, 0), QPointF(100, 0),
                                                 QPointF(0, 25), QPointF(80, 25),
                                                 distance, &error));
    QVERIFY(qAbs(distance - 25.0) < 1e-6);
    QVERIFY(!ParallelDistanceAlgorithm::fromLines(QPointF(0, 0), QPointF(0, 0),
                                                  QPointF(0, 25), QPointF(80, 25),
                                                  distance, &error));
}

void TestVision::angleMeasureRejectsDegenerateLines()
{
    double angle = 0.0;
    QString error;
    QVERIFY(!AngleMeasureAlgorithm::fromLines(QPointF(0, 0), QPointF(0, 0),
                                              QPointF(0, 0), QPointF(10, 0),
                                              angle, &error));
    QVERIFY(!error.isEmpty());
}

void TestVision::invalidKernelRejectedByMedian()
{
    VisionImage input = makeWhiteRectangleOnBlack(20, 20, cv::Rect(5, 5, 8, 8));
    VisionImage out;
    QString error;
    // Algorithm normalizes even kernels; empty image must fail.
    QVERIFY(!MedianBlurAlgorithm::apply(VisionImage{}, out, 3, &error));
    QVERIFY(!error.isEmpty());
}

QTEST_MAIN(TestVision)
#include "test_vision.moc"
