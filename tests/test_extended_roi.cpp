#include "vision/model/extendedroi.h"

#include <QtTest>

class TestExtendedRoi : public QObject
{
    Q_OBJECT
private slots:
    void rectangleNormalizeAndClamp();
    void circleClampKeepsInside();
    void polygonRoundTrip();
    void rotatedRectLegacyConversion();
};

void TestExtendedRoi::rectangleNormalizeAndClamp()
{
    Selt::ExtendedRoi roi;
    roi.shape = Selt::RoiShapeType::Rectangle;
    roi.enabled = true;
    roi.rect = QRectF(10, 10, -40, -20);
    roi.normalize();
    QCOMPARE(roi.rect, QRectF(-30, -10, 40, 20));
    roi.clampToImage(QSize(100, 80));
    QVERIFY(roi.rect.left() >= 0);
    QVERIFY(roi.rect.top() >= 0);
}

void TestExtendedRoi::circleClampKeepsInside()
{
    Selt::ExtendedRoi roi;
    roi.shape = Selt::RoiShapeType::Circle;
    roi.enabled = true;
    roi.center = QPointF(5, 5);
    roi.radius = 20;
    roi.clampToImage(QSize(50, 50));
    QVERIFY(roi.radius <= 5.0 + 1e-6);
    QVERIFY(roi.isValid());
}

void TestExtendedRoi::polygonRoundTrip()
{
    Selt::ExtendedRoi roi;
    roi.shape = Selt::RoiShapeType::Polygon;
    roi.enabled = true;
    roi.polygon = {QPointF(1, 1), QPointF(10, 1), QPointF(10, 8)};
    const QJsonObject json = roi.toJson();
    const Selt::ExtendedRoi loaded = Selt::ExtendedRoi::fromJson(json);
    QCOMPARE(loaded.shape, Selt::RoiShapeType::Polygon);
    QCOMPARE(loaded.polygon.size(), 3);
    QVERIFY(loaded.isValid());
}

void TestExtendedRoi::rotatedRectLegacyConversion()
{
    Selt::ExtendedRoi roi;
    roi.shape = Selt::RoiShapeType::RotatedRect;
    roi.enabled = true;
    roi.center = QPointF(40, 30);
    roi.width = 20;
    roi.height = 10;
    roi.angleDeg = 30;
    const RoiRect legacy = roi.toLegacyRect();
    QVERIFY(legacy.enabled);
    QVERIFY(legacy.rect.width() >= 20);
    const Selt::ExtendedRoi back = Selt::ExtendedRoi::fromLegacyRect(legacy);
    QCOMPARE(back.shape, Selt::RoiShapeType::Rectangle);
}

QTEST_MAIN(TestExtendedRoi)
#include "test_extended_roi.moc"
