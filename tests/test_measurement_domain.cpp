#include "vision/model/calibrationmodel.h"
#include "vision/model/measurementdefinition.h"
#include "vision/model/measurementresult.h"

#include <QtTest>

class TestMeasurementDomain : public QObject
{
    Q_OBJECT

private slots:
    void measurementRoundTripPreservesFields();
    void calibrationScalesLengths();
    void uncalibratedKeepsPixelUnit();
    void toleranceEvaluatesOkNg();
};

void TestMeasurementDomain::measurementRoundTripPreservesFields()
{
    MeasurementResult in;
    in.valid = true;
    in.unit = QStringLiteral("mm");
    in.confidence = 0.95;
    in.measurementType = QStringLiteral("rectangle");
    in.sourceNodeId = QStringLiteral("n1");
    in.decisionState = QStringLiteral("ok");
    in.calibrationId = QStringLiteral("cal1");
    in.toleranceId = QStringLiteral("tol1");
    in.width = 12.5;
    in.height = 8.0;
    in.area = 100.0;
    in.angle = 15.0;
    in.center = QPointF(3, 4);
    in.boundingRect = QRectF(1, 2, 12.5, 8.0);
    in.message = QStringLiteral("ok");

    const MeasurementResult out = MeasurementResult::fromJson(in.toJson());
    QCOMPARE(out.unit, in.unit);
    QCOMPARE(out.confidence, in.confidence);
    QCOMPARE(out.measurementType, in.measurementType);
    QCOMPARE(out.decisionState, in.decisionState);
    QCOMPARE(out.calibrationId, in.calibrationId);
    QCOMPARE(out.width, in.width);
    QCOMPARE(out.height, in.height);
}

void TestMeasurementDomain::calibrationScalesLengths()
{
    CalibrationModel cal;
    cal.calibrationId = QStringLiteral("cal");
    cal.unit = QStringLiteral("mm");
    cal.unitPerPixel = 0.1;
    cal.valid = true;

    MeasurementResult px;
    px.valid = true;
    px.unit = QStringLiteral("px");
    px.width = 100.0;
    px.height = 50.0;
    px.area = 5000.0;
    px.center = QPointF(10, 20);

    const MeasurementResult mm = cal.applyTo(px);
    QCOMPARE(mm.unit, QStringLiteral("mm"));
    QCOMPARE(mm.width, 10.0);
    QCOMPARE(mm.height, 5.0);
    QCOMPARE(mm.area, 50.0);
    QCOMPARE(mm.calibrationId, QStringLiteral("cal"));
    QCOMPARE(mm.center, QPointF(1.0, 2.0));
}

void TestMeasurementDomain::uncalibratedKeepsPixelUnit()
{
    CalibrationModel cal;
    cal.valid = false;
    cal.unit = QStringLiteral("mm");
    cal.unitPerPixel = 0.1;

    MeasurementResult px;
    px.valid = true;
    px.unit = QStringLiteral("px");
    px.width = 20.0;
    const MeasurementResult out = cal.applyTo(px);
    QCOMPARE(out.unit, QStringLiteral("px"));
    QCOMPARE(out.width, 20.0);
    QVERIFY(out.calibrationId.isEmpty());
}

void TestMeasurementDomain::toleranceEvaluatesOkNg()
{
    MeasurementDefinition def;
    def.hasTolerance = true;
    def.lower = 9.0;
    def.upper = 11.0;
    def.nominal = 10.0;
    QCOMPARE(def.evaluateDecision(10.0), QStringLiteral("ok"));
    QCOMPARE(def.evaluateDecision(12.0), QStringLiteral("ng"));

    MeasurementDefinition none;
    QCOMPARE(none.evaluateDecision(10.0), QStringLiteral("unknown"));
}

QTEST_MAIN(TestMeasurementDomain)
#include "test_measurement_domain.moc"
