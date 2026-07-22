#include "vision/algorithms/imagefilteralgorithms.h"
#include "vision/algorithms/imageloader.h"

#include <QDir>
#include <QImage>
#include <QTemporaryDir>
#include <QtTest>
#include <opencv2/imgproc.hpp>

class TestRuntimeGuards : public QObject
{
    Q_OBJECT

private slots:
    void imageLoaderSupportsUnicodePath();
    void templateMatchNormalizesImageTypes();
};

void TestRuntimeGuards::imageLoaderSupportsUnicodePath()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString path = QDir(temp.path()).filePath(QStringLiteral("中文模板.png"));
    QImage image(16, 16, QImage::Format_RGB888);
    image.fill(Qt::white);
    QVERIFY(image.save(path));

    VisionImage loaded;
    QString error;
    QVERIFY2(ImageLoader::load(path, loaded, &error), qPrintable(error));
    QVERIFY(!loaded.isEmpty());
}

void TestRuntimeGuards::templateMatchNormalizesImageTypes()
{
    cv::Mat search(32, 32, CV_8UC3, cv::Scalar(0, 0, 0));
    cv::rectangle(search, cv::Rect(8, 8, 8, 8), cv::Scalar(255, 255, 255), cv::FILLED);
    cv::Mat templ(8, 8, CV_8UC1, cv::Scalar(255));

    QPointF peak;
    double score = 0.0;
    QString error;
    QVERIFY2(TemplateMatchAlgorithm::match(VisionImage(search), VisionImage(templ),
                                           &peak, &score, &error),
             qPrintable(error));
    QVERIFY(score >= 0.0);
}

QTEST_MAIN(TestRuntimeGuards)
#include "test_runtime_guards.moc"
