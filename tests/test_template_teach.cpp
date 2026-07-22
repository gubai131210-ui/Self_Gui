#include "vision/services/templateteachservice.h"
#include "vision/storage/projectresourcestore.h"

#include <QFileInfo>
#include <QImage>
#include <QTemporaryDir>
#include <QtTest>

class TestTemplateTeach : public QObject
{
    Q_OBJECT
private slots:
    void cropRejectsEmptyImage();
    void cropRejectsTinyRoi();
    void teachWritesRelativeResource();
};

void TestTemplateTeach::cropRejectsEmptyImage()
{
    VisionImage out;
    QString error;
    Selt::ExtendedRoi roi;
    roi.shape = Selt::RoiShapeType::Rectangle;
    roi.enabled = true;
    roi.rect = QRectF(0, 0, 20, 20);
    QVERIFY(!Selt::TemplateTeachService::cropTemplate(QImage(), roi, &out, &error));
    QVERIFY(!error.isEmpty());
}

void TestTemplateTeach::cropRejectsTinyRoi()
{
    QImage image(64, 64, QImage::Format_RGB888);
    image.fill(Qt::white);
    VisionImage out;
    QString error;
    Selt::ExtendedRoi roi;
    roi.shape = Selt::RoiShapeType::Rectangle;
    roi.enabled = true;
    roi.rect = QRectF(2, 2, 3, 3);
    QVERIFY(!Selt::TemplateTeachService::cropTemplate(image, roi, &out, &error));
    QVERIFY(error.contains(QStringLiteral("过小")));
}

void TestTemplateTeach::teachWritesRelativeResource()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    QImage image(80, 80, QImage::Format_RGB888);
    image.fill(Qt::black);
    for (int y = 10; y < 30; ++y)
        for (int x = 10; x < 40; ++x)
            image.setPixel(x, y, qRgb(255, 255, 255));

    Selt::ExtendedRoi roi;
    roi.shape = Selt::RoiShapeType::Rectangle;
    roi.enabled = true;
    roi.rect = QRectF(10, 10, 30, 20);

    Selt::ProjectResourceStore store;
    store.setProjectRoot(temp.path());
    const Selt::TemplateTeachResult result =
        Selt::TemplateTeachService::teachFromRoi(image, roi, store, QStringLiteral("unit"));
    QVERIFY2(result.ok, qPrintable(result.error));
    QVERIFY(store.contains(result.resourceId));
    QVERIFY(result.relativePath.startsWith(QStringLiteral("assets/templates/")));
    QVERIFY(QFileInfo::exists(result.absolutePath));
    QVERIFY(!result.preview.isNull());
}

QTEST_MAIN(TestTemplateTeach)
#include "test_template_teach.moc"
