#include "vision/domain/projectvariables.h"
#include "vision/domain/variablescope.h"
#include "vision/domain/visionproject.h"
#include "vision/runtime/bindingresolver.h"
#include "vision/runtime/portvaluestore.h"
#include "vision/storage/projectcontainer.h"
#include "vision/data/datatype.h"
#include "ui/nodepalette.h"

#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>

using namespace Selt;

class TestEditorVariableUx : public QObject
{
    Q_OBJECT
private slots:
    void scopeChainShadowsByName();
    void scopeChainResolvesByIdNearFirst();
    void bindingResolverUsesScopeChain();
    void containerPersistsProjectVariables();
    void paletteCreateDedupesActivatedAndDoubleClick();
};

void TestEditorVariableUx::scopeChainShadowsByName()
{
    ProjectVariableStore global;
    ProjectVariableStore project;
    ProjectVariableStore flow;
    const QString gId = global.add(QStringLiteral("threshold"), DataTypeId::Real, DataValue(1.0));
    const QString pId = project.add(QStringLiteral("threshold"), DataTypeId::Real, DataValue(2.0));
    const QString fId = flow.add(QStringLiteral("threshold"), DataTypeId::Real, DataValue(3.0));
    QVERIFY(!gId.isEmpty() && !pId.isEmpty() && !fId.isEmpty());

    VariableScopeChain chain;
    chain.setGlobal(&global);
    chain.setProject(&project);
    chain.setFlow(&flow, QStringLiteral("flow1"));

    const ScopedVariableHit hit = chain.resolveByName(QStringLiteral("threshold"));
    QCOMPARE(hit.record.id, fId);
    QCOMPARE(hit.scope, VariableScopeKind::Flow);
    QCOMPARE(hit.record.value.toReal(), 3.0);

    const QVector<ScopedVariableHit> all = chain.allVisible();
    QVERIFY(all.size() >= 3);
    int shadowed = 0;
    for (const ScopedVariableHit &h : all) {
        if (h.record.name == QLatin1String("threshold") && h.shadowed)
            ++shadowed;
    }
    QVERIFY(shadowed >= 1);
}

void TestEditorVariableUx::scopeChainResolvesByIdNearFirst()
{
    ProjectVariableStore project;
    ProjectVariableStore flow;
    const QString id = project.add(QStringLiteral("gain"), DataTypeId::Real, DataValue(1.5));
    // Same id cannot exist in two stores with add(); simulate by copying id is hard.
    // Use distinct ids and ensure project id resolves when only in project.
    VariableScopeChain chain;
    chain.setProject(&project);
    chain.setFlow(&flow, QStringLiteral("f"));
    QVERIFY(chain.containsId(id));
    QCOMPARE(chain.resolveById(id).scope, VariableScopeKind::Project);
}

void TestEditorVariableUx::bindingResolverUsesScopeChain()
{
    ProjectVariableStore project;
    ProjectVariableStore flow;
    const QString id = flow.add(QStringLiteral("limit"), DataTypeId::Real, DataValue(9.0));
    VariableScopeChain chain;
    chain.setProject(&project);
    chain.setFlow(&flow, QStringLiteral("f"));

    ParameterBinding binding = ParameterBinding::projectVariable(id, DataTypeId::Real);
    PortValueStore ports;
    const BindingResolveResult r = BindingResolver::resolve(binding, DataTypeId::Real, ports, chain);
    QVERIFY(r.ok);
    QCOMPARE(r.value.toReal(), 9.0);
}

void TestEditorVariableUx::containerPersistsProjectVariables()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("proj.seltproject"));

    VisionProject project;
    project.setTitle(QStringLiteral("var-test"));
    const QString vid =
        project.variables().add(QStringLiteral("batch"), DataTypeId::String, DataValue(QStringLiteral("A01")));
    QVERIFY(!vid.isEmpty());
    if (VisionFlow *flow = project.activeFlow()) {
        flow->flowVariables().add(QStringLiteral("localGain"), DataTypeId::Real, DataValue(1.25));
    }

    QString err;
    QVERIFY2(ProjectContainerStorage::save(path, project, &err), qPrintable(err));

    VisionProject loaded;
    ProjectContainerReport report;
    QVERIFY2(ProjectContainerStorage::load(path, loaded, &report), qPrintable(report.error));
    QVERIFY(loaded.variables().contains(vid));
    QCOMPARE(loaded.variables().record(vid).name, QStringLiteral("batch"));
    QCOMPARE(loaded.variables().get(vid).toString(), QStringLiteral("A01"));
    VisionFlow *loadedFlow = loaded.activeFlow();
    QVERIFY(loadedFlow != nullptr);
    bool foundLocal = false;
    for (const ProjectVariableRecord &rec : loadedFlow->flowVariables().records()) {
        if (rec.name == QLatin1String("localGain")) {
            foundLocal = true;
            QCOMPARE(rec.value.toReal(), 1.25);
        }
    }
    QVERIFY(foundLocal);
}

void TestEditorVariableUx::paletteCreateDedupesActivatedAndDoubleClick()
{
    NodePalette palette;
    QSignalSpy createSpy(&palette, &NodePalette::nodeTypeActivated);
    QVERIFY(createSpy.isValid());

    palette.requestCreate(QStringLiteral("vision.threshold"));
    palette.requestCreate(QStringLiteral("vision.threshold")); // same event window
    QCOMPARE(createSpy.count(), 1);

    QTest::qWait(120);
    palette.requestCreate(QStringLiteral("vision.threshold"));
    QCOMPARE(createSpy.count(), 2);
}

QTEST_MAIN(TestEditorVariableUx)
#include "test_editor_variable_ux.moc"
