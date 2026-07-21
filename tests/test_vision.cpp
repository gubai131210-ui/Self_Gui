#include "vision/algorithms/grayscalealgorithm.h"
#include "vision/algorithms/rectanglemeasurementalgorithm.h"
#include "vision/algorithms/thresholdalgorithm.h"
#include "vision/model/visionimage.h"

#include <QCoreApplication>
#include <QRectF>
#include <QTest>
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
    QCOMPARE(qRound(result.width), rect.width);
    QCOMPARE(qRound(result.height), rect.height);
    QVERIFY(result.area >= rect.width * rect.height);
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

QTEST_MAIN(TestVision)
#include "test_vision.moc"
