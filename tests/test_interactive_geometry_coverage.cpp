#include "vision/model/autoparameter.h"
#include "vision/model/interactivegeometry.h"
#include "vision/registry/visionnoderegistry.h"
#include "vision/registry/visionnodeids.h"

#include <QSet>
#include <QtTest>

class InteractiveGeometryCoverageTest : public QObject
{
    Q_OBJECT
private slots:
    void everyRegisteredNodeHasGeometrySpec();
    void planA_roiOperatorsAreTeachable();
    void geometryTeachingKinds_areCorrect();
    void autoParameterContract_distinguishesUnsetAndZero();
    void autoCaliperHelpers_areDeterministic();
};

void InteractiveGeometryCoverageTest::everyRegisteredNodeHasGeometrySpec()
{
    const QVector<ModuleDescriptor> all = VisionNodeRegistry::allDescriptors();
    QVERIFY2(!all.isEmpty(), "registry must expose descriptors");

    for (const ModuleDescriptor &d : all) {
        const Selt::InteractiveGeometrySpec spec =
            Selt::interactiveGeometrySpecForNode(d.typeId);
        QVERIFY2(spec.kind != Selt::GeometryKind::None,
                 qPrintable(QStringLiteral("%1 resolved to GeometryKind::None")
                                .arg(d.typeId)));
        QCOMPARE(spec.nodeType, d.typeId);
        QCOMPARE(d.interactiveGeometry.kind, spec.kind);
        QVERIFY2(!spec.coverageNote.isEmpty(),
                 qPrintable(QStringLiteral("%1 missing coverageNote").arg(d.typeId)));
    }
}

void InteractiveGeometryCoverageTest::planA_roiOperatorsAreTeachable()
{
    const QStringList roiNodes = {
        VisionNodeIds::grayscale(),
        VisionNodeIds::threshold(),
        VisionNodeIds::blur(),
        VisionNodeIds::canny(),
        VisionNodeIds::morphology(),
        VisionNodeIds::rectangleMeasure(),
        VisionNodeIds::circleMeasure(),
        VisionNodeIds::lineMeasure(),
        VisionNodeIds::barcodeDecode(),
        VisionNodeIds::ocr(),
        VisionNodeIds::gaussianBlur(),
        VisionNodeIds::medianBlur(),
        VisionNodeIds::bilateralFilter(),
        VisionNodeIds::otsuThreshold(),
        VisionNodeIds::adaptiveThreshold(),
        VisionNodeIds::sobel(),
        VisionNodeIds::colorConvert(),
        VisionNodeIds::gammaCorrect(),
        VisionNodeIds::contrastBrightness(),
        VisionNodeIds::claheEnhance(),
        VisionNodeIds::sharpen(),
        VisionNodeIds::findContours(),
        VisionNodeIds::connectedComponents(),
        VisionNodeIds::regionFill(),
        VisionNodeIds::blobAnalyze(),
        VisionNodeIds::houghLines(),
        VisionNodeIds::houghCircles(),
        VisionNodeIds::blobLocate(),
        VisionNodeIds::featureMatch(),
        VisionNodeIds::templateMatchMulti(),
        VisionNodeIds::subpixelRefine(),
        VisionNodeIds::caliperMeasure(),
        VisionNodeIds::findLine(),
        VisionNodeIds::findCircle(),
        VisionNodeIds::fitCircle(),
        VisionNodeIds::fitLine(),
        VisionNodeIds::angleMeasure(),
        VisionNodeIds::parallelDistance(),
        VisionNodeIds::pointLineDistance(),
        VisionNodeIds::circularityMeasure(),
    };

    for (const QString &typeId : roiNodes) {
        const ModuleDescriptor d = VisionNodeRegistry::descriptor(typeId);
        QVERIFY2(!d.typeId.isEmpty(), qPrintable(typeId));
        const Selt::InteractiveGeometrySpec &spec = d.interactiveGeometry;
        QVERIFY2(Selt::isSpatialTeachable(spec) || spec.supportsRoi,
                 qPrintable(QStringLiteral("%1 not teachable").arg(typeId)));
        QVERIFY2(spec.supportsRoi || spec.kind == Selt::GeometryKind::Roi
                     || spec.kind == Selt::GeometryKind::TemplateRoi
                     || spec.kind == Selt::GeometryKind::LineSegment
                     || spec.kind == Selt::GeometryKind::Circle
                     || spec.kind == Selt::GeometryKind::CaliperStrip
                     || spec.kind == Selt::GeometryKind::ContourPick
                     || spec.kind == Selt::GeometryKind::TwoLineSegment
                     || spec.kind == Selt::GeometryKind::Point,
                 qPrintable(QStringLiteral("%1 lacks ROI/geometry kind").arg(typeId)));

        bool hasRoiParam = false;
        for (const ModuleParamDef &p : d.parameters) {
            if (p.key == QLatin1String("roi") || p.key == QLatin1String("extendedRoi")
                || p.type == ModuleParamType::RoiRect) {
                hasRoiParam = true;
                break;
            }
        }
        QVERIFY2(hasRoiParam || spec.supportsRoi,
                 qPrintable(QStringLiteral("%1 missing roi parameter").arg(typeId)));
    }
}

void InteractiveGeometryCoverageTest::geometryTeachingKinds_areCorrect()
{
    QCOMPARE(Selt::interactiveGeometrySpecForNode(VisionNodeIds::findLine()).kind,
             Selt::GeometryKind::LineSegment);
    QCOMPARE(Selt::interactiveGeometrySpecForNode(VisionNodeIds::findCircle()).kind,
             Selt::GeometryKind::Circle);
    QCOMPARE(Selt::interactiveGeometrySpecForNode(VisionNodeIds::caliperMeasure()).kind,
             Selt::GeometryKind::CaliperStrip);
    QCOMPARE(Selt::interactiveGeometrySpecForNode(VisionNodeIds::referencePoint()).kind,
             Selt::GeometryKind::Point);
    QCOMPARE(Selt::interactiveGeometrySpecForNode(VisionNodeIds::pointPointDistance()).kind,
             Selt::GeometryKind::TwoPoint);
    QCOMPARE(Selt::interactiveGeometrySpecForNode(VisionNodeIds::angleMeasure()).kind,
             Selt::GeometryKind::TwoLineSegment);
    QCOMPARE(Selt::interactiveGeometrySpecForNode(VisionNodeIds::fitLine()).kind,
             Selt::GeometryKind::ContourPick);
    QCOMPARE(Selt::interactiveGeometrySpecForNode(VisionNodeIds::templateMatch()).kind,
             Selt::GeometryKind::TemplateRoi);
    QCOMPARE(Selt::interactiveGeometrySpecForNode(VisionNodeIds::blobAnalyze()).kind,
             Selt::GeometryKind::Roi);
    QCOMPARE(Selt::interactiveGeometrySpecForNode(VisionNodeIds::imageLoader()).kind,
             Selt::GeometryKind::NotApplicable);
    QCOMPARE(Selt::interactiveGeometrySpecForNode(VisionNodeIds::boolAnd()).kind,
             Selt::GeometryKind::NotApplicable);
}

void InteractiveGeometryCoverageTest::autoParameterContract_distinguishesUnsetAndZero()
{
    QJsonObject unset;
    QCOMPARE(Selt::paramBindingMode(unset, QStringLiteral("searchLength")),
             Selt::ParamBindingMode::Unset);
    QCOMPARE(Selt::resolveAutoDouble(unset, QStringLiteral("searchLength"), 40.0, 55.0), 55.0);

    QJsonObject zeroAuto{{QStringLiteral("searchLength"), 0.0}};
    QCOMPARE(Selt::resolveAutoDouble(zeroAuto, QStringLiteral("searchLength"), 40.0, 55.0), 55.0);

    QJsonObject user{{QStringLiteral("searchLength"), 33.0}};
    QCOMPARE(Selt::resolveAutoDouble(user, QStringLiteral("searchLength"), 40.0, 55.0), 33.0);

    QJsonObject strict{
        {QStringLiteral("searchLength"), 0.0},
        {QStringLiteral("_mode_searchLength"), QStringLiteral("strict")}};
    QCOMPARE(Selt::resolveAutoDouble(strict, QStringLiteral("searchLength"), 40.0, 55.0), 0.0);

    QJsonObject calipers{{QStringLiteral("numCalipers"), 0}};
    QCOMPARE(Selt::resolveAutoInt(calipers, QStringLiteral("numCalipers"), 8, 12), 12);

    QJsonObject ignore{{QStringLiteral("numToIgnore"), -1}};
    QCOMPARE(Selt::resolveAutoInt(ignore, QStringLiteral("numToIgnore"), 0, 3), 3);
}

void InteractiveGeometryCoverageTest::autoCaliperHelpers_areDeterministic()
{
    QCOMPARE(Selt::autoCaliperCountForLine(200.0), Selt::autoCaliperCountForLine(200.0));
    QCOMPARE(Selt::autoCaliperCountForArc(50.0, 360.0),
             Selt::autoCaliperCountForArc(50.0, 360.0));
    QVERIFY(Selt::autoSearchLength(100.0) > 10.0);

    QJsonObject params;
    Selt::suggestDefaultLineGeometry(QSize(640, 480), &params);
    QVERIFY(params.contains(QStringLiteral("x0")));
    QVERIFY(params.contains(QStringLiteral("x1")));
    QCOMPARE(params.value(QStringLiteral("_mode_x0")).toString(), QStringLiteral("auto"));
}

QTEST_MAIN(InteractiveGeometryCoverageTest)
#include "test_interactive_geometry_coverage.moc"
