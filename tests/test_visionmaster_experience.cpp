#include "ui/theme/uistyle.h"
#include "vision/registry/visionnodeids.h"
#include "vision/registry/visionnoderegistry.h"
#include "vision/workflow/workflowtemplates.h"

#include <QSet>
#include <QTest>

class TestVisionMasterExperience : public QObject
{
    Q_OBJECT

private slots:
    void parameterSchemaHasGroupsAndMissingKeys();
    void additiveParamsKeepLegacyDefaults();
    void workflowTemplatesBuildConnectedGraph();
    void categoriesFollowStageOrder();
    void themeTokensAreOrangeIndustrial();
};

void TestVisionMasterExperience::parameterSchemaHasGroupsAndMissingKeys()
{
    const ModuleDescriptor morph = VisionNodeRegistry::descriptor(VisionNodeIds::morphology());
    bool hasIterations = false;
    bool hasGroup = false;
    for (const ModuleParamDef &p : morph.parameters) {
        if (p.key == QLatin1String("iterations"))
            hasIterations = true;
        if (!p.group.isEmpty())
            hasGroup = true;
    }
    QVERIFY(hasIterations);
    QVERIFY(hasGroup);

    const ModuleDescriptor sub = VisionNodeRegistry::descriptor(VisionNodeIds::subpixelRefine());
    QStringList keys;
    for (const ModuleParamDef &p : sub.parameters)
        keys.append(p.key);
    QVERIFY(keys.contains(QStringLiteral("terminateEps")));
    QVERIFY(keys.contains(QStringLiteral("maxIterations")));

    const ModuleDescriptor caliper = VisionNodeRegistry::descriptor(VisionNodeIds::caliperMeasure());
    keys.clear();
    for (const ModuleParamDef &p : caliper.parameters)
        keys.append(p.key);
    QVERIFY(keys.contains(QStringLiteral("secondPeakRatio")));
    QVERIFY(keys.contains(QStringLiteral("roiApplyMode")));

    const ModuleDescriptor fit = VisionNodeRegistry::descriptor(VisionNodeIds::fitCircle());
    keys.clear();
    for (const ModuleParamDef &p : fit.parameters)
        keys.append(p.key);
    QVERIFY(keys.contains(QStringLiteral("minInliers")));
}

void TestVisionMasterExperience::additiveParamsKeepLegacyDefaults()
{
    const QJsonObject thr = VisionNodeRegistry::defaultParameters(VisionNodeIds::threshold());
    QCOMPARE(thr.value(QStringLiteral("threshold")).toInt(), 128);
    QCOMPARE(thr.value(QStringLiteral("mode")).toString(), QStringLiteral("binary"));

    const QJsonObject morph = VisionNodeRegistry::defaultParameters(VisionNodeIds::morphology());
    QCOMPARE(morph.value(QStringLiteral("iterations")).toInt(1), 1);
    QCOMPARE(morph.value(QStringLiteral("op")).toString(), QStringLiteral("erode"));

    const QJsonObject canny = VisionNodeRegistry::defaultParameters(VisionNodeIds::canny());
    QCOMPARE(canny.value(QStringLiteral("apertureSize")).toInt(3), 3);

    QString err;
    QVERIFY(VisionNodeRegistry::validateParameters(VisionNodeIds::morphology(), morph, &err));
    QVERIFY(err.isEmpty());
}

void TestVisionMasterExperience::workflowTemplatesBuildConnectedGraph()
{
    for (const QString &id : Selt::WorkflowTemplates::templateIds()) {
        const Selt::WorkflowTemplateSpec spec = Selt::WorkflowTemplates::build(id);
        QVERIFY2(spec.nodes.size() >= 3, qPrintable(id));
        QVERIFY2(!spec.connections.isEmpty(), qPrintable(id));
        QSet<QString> nodeIds;
        for (const NodeModel &n : spec.nodes)
            nodeIds.insert(n.id);
        for (const ConnectionModel &c : spec.connections) {
            QVERIFY2(nodeIds.contains(c.sourceNodeId), qPrintable(id + QLatin1Char(':') + c.id));
            QVERIFY2(nodeIds.contains(c.targetNodeId), qPrintable(id + QLatin1Char(':') + c.id));
            QCOMPARE(c.lineStyle, ConnectionLineStyle::Orthogonal);
        }
    }

    const Selt::WorkflowTemplateSpec full =
        Selt::WorkflowTemplates::build(QStringLiteral("full_chain"));
    QCOMPARE(full.nodes.size(), 7);
    QVERIFY(full.description.contains(QStringLiteral("结果写出")));
}

void TestVisionMasterExperience::categoriesFollowStageOrder()
{
    const QStringList cats = VisionNodeRegistry::categories();
    QVERIFY(cats.contains(QStringLiteral("输入")));
    QVERIFY(cats.contains(QStringLiteral("预处理")));
    QVERIFY(cats.contains(QStringLiteral("测量")));
    const int input = cats.indexOf(QStringLiteral("输入"));
    const int prep = cats.indexOf(QStringLiteral("预处理"));
    const int measure = cats.indexOf(QStringLiteral("测量"));
    const int decide = cats.indexOf(QStringLiteral("判定"));
    const int output = cats.indexOf(QStringLiteral("输出"));
    QVERIFY(input >= 0 && prep > input);
    QVERIFY(measure > prep);
    if (decide >= 0)
        QVERIFY(decide > measure);
    if (output >= 0 && decide >= 0)
        QVERIFY(output > decide);
}

void TestVisionMasterExperience::themeTokensAreOrangeIndustrial()
{
    QCOMPARE(Selt::UiStyle::accentOrange().name(QColor::HexRgb), QStringLiteral("#ff8c00"));
    QCOMPARE(Selt::UiStyle::connectionActive().name(QColor::HexRgb), QStringLiteral("#ff8c00"));
    QVERIFY(Selt::UiStyle::canvasBackground().lightness() < 80);
    QVERIFY(Selt::UiStyle::gridMajorLine().alpha() > 0);
}

QTEST_MAIN(TestVisionMasterExperience)
#include "test_visionmaster_experience.moc"
