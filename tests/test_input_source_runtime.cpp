#include "vision/runtime/inputsourceservice.h"
#include "vision/runtime/runcontroller.h"
#include "vision/nodes/builtinexecutors.h"
#include "vision/registry/visionnodeids.h"
#include "vision/registry/visionnoderegistry.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

class InputSourceRuntimeTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void directoryReplayCyclesFrames();
    void emptyDirectoryFails();
    void badImageFailsOpen();
    void liveFrameOverridesImageLoader();
};

void InputSourceRuntimeTest::initTestCase()
{
    Selt::registerBuiltInOpenCvExecutors();
}

void InputSourceRuntimeTest::directoryReplayCyclesFrames()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    for (int i = 0; i < 3; ++i) {
        const QString path = dir.filePath(QStringLiteral("f%1.png").arg(i));
        cv::Mat mat(8, 8, CV_8UC1, cv::Scalar(i * 10 + 5));
        QVERIFY(cv::imwrite(path.toStdString(), mat));
    }

    Selt::InputSourceService service;
    QVERIFY(service.openDirectoryReplay(dir.path()).ok());
    VisionImage a, b, c, d;
    QVERIFY(service.grab(a).ok());
    QVERIFY(service.grab(b).ok());
    QVERIFY(service.grab(c).ok());
    QVERIFY(service.grab(d).ok()); // wraps to first
    QCOMPARE(int(a.mat().at<uchar>(0, 0)), 5);
    QCOMPARE(int(d.mat().at<uchar>(0, 0)), 5);
    service.close();
    QVERIFY(!service.isOpen());
}

void InputSourceRuntimeTest::emptyDirectoryFails()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Selt::InputSourceService service;
    QVERIFY(!service.openDirectoryReplay(dir.path()).ok());
    QVERIFY(!service.isOpen());
}

void InputSourceRuntimeTest::badImageFailsOpen()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("bad.png"));
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("not-an-image");
    f.close();

    Selt::InputSourceService service;
    QVERIFY(!service.openImageFile(path).ok());
}

void InputSourceRuntimeTest::liveFrameOverridesImageLoader()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("live.png"));
    cv::Mat mat(16, 16, CV_8UC1, cv::Scalar(77));
    QVERIFY(cv::imwrite(path.toStdString(), mat));

    Selt::InputSourceService service;
    QVERIFY(service.openImageFile(path).ok());

    Document doc;
    NodeModel loader = VisionNodeRegistry::create(VisionNodeIds::imageLoader(), {});
    loader.id = QStringLiteral("l");
    // Intentionally leave path empty: live frame should supply the image.
    loader.parameters.insert(QStringLiteral("path"), QString());
    doc.addNode(loader);

    Selt::RunController controller;
    controller.setDocument(&doc);
    controller.setInputSource(&service);
    QVERIFY(controller.runOnce());
    QVERIFY(controller.lastContext().moduleResults.contains(QStringLiteral("l")));
    QCOMPARE(controller.lastContext().moduleResults.value(QStringLiteral("l")).status, ModuleStatus::Success);
    QVERIFY(!controller.lastContext().originalImage.isEmpty());
    QCOMPARE(int(controller.lastContext().originalImage.mat().at<uchar>(0, 0)), 77);
}

QTEST_MAIN(InputSourceRuntimeTest)
#include "test_input_source_runtime.moc"
