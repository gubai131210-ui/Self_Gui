#include "core/model/document.h"
#include "vision/domain/projectvariables.h"
#include "vision/model/roi.h"
#include "vision/nodes/imageexecutorhelpers.h"
#include "vision/registry/visionnodeids.h"
#include "vision/registry/visionnoderegistry.h"
#include "vision/runtime/bindingresolver.h"
#include "vision/runtime/flowruntime.h"
#include "vision/runtime/portvaluestore.h"

#include <QtTest>
#include <opencv2/core.hpp>

class RuntimeBindingTest : public QObject
{
    Q_OBJECT
private slots:
    void portValueStoreAliases();
    void resolveProjectVariable();
    void resolveMissingUpstream();
    void executeLegacyJsonParameters();
    void disabledNodeSkipped();
    void upstreamRoiOverridesParameterRoi();
};

void RuntimeBindingTest::portValueStoreAliases()
{
    Selt::PortValueStore store;
    VisionImage img(cv::Mat(4, 4, CV_8UC1, cv::Scalar(10)));
    QVERIFY(store.set(QStringLiteral("n1"), QStringLiteral("image"), Selt::DataValue(img)));
    QVERIFY(store.containsAliased(QStringLiteral("n1"), QStringLiteral("out")));
    QCOMPARE(store.valueAliased(QStringLiteral("n1"), QStringLiteral("out")).type(), Selt::DataTypeId::Image);
}

void RuntimeBindingTest::resolveProjectVariable()
{
    Selt::ProjectVariableStore vars;
    const QString id = vars.add(QStringLiteral("thr"), Selt::DataTypeId::Int, Selt::DataValue(90));
    Selt::PortValueStore ports;
    auto binding = Selt::ParameterBinding::projectVariable(id, Selt::DataTypeId::Int);
    const auto result = Selt::BindingResolver::resolve(binding, Selt::DataTypeId::Int, ports, vars);
    QVERIFY(result.ok);
    QCOMPARE(result.value.toInt(), 90);
}

void RuntimeBindingTest::resolveMissingUpstream()
{
    Selt::ProjectVariableStore vars;
    Selt::PortValueStore ports;
    auto binding = Selt::ParameterBinding::upstream(QStringLiteral("x"), QStringLiteral("out"),
                                                    Selt::DataTypeId::Image);
    const auto result = Selt::BindingResolver::resolve(binding, Selt::DataTypeId::Image, ports, vars);
    QVERIFY(!result.ok);
    QVERIFY(result.error.contains(QStringLiteral("尚未执行")));
}

void RuntimeBindingTest::executeLegacyJsonParameters()
{
    Document doc;
    NodeModel loader = VisionNodeRegistry::create(VisionNodeIds::imageLoader(), QPointF(0, 0));
    loader.id = QStringLiteral("loader");
    // Intentionally invalid path -> executor fails, but binding/param path should reach executor.
    loader.parameters.insert(QStringLiteral("path"), QStringLiteral("Z:/not-exist-selt-test.png"));
    doc.addNode(loader);

    VisionContext ctx;
    Selt::ProjectVariableStore vars;
    const bool ok = Selt::FlowRuntime::executeOnce(doc, ctx, vars, nullptr);
    QVERIFY(!ok);
    QVERIFY(ctx.moduleResults.contains(loader.id));
    QCOMPARE(ctx.moduleResults.value(loader.id).failureKind, QStringLiteral("execution"));
}

void RuntimeBindingTest::disabledNodeSkipped()
{
    Document doc;
    NodeModel loader = VisionNodeRegistry::create(VisionNodeIds::imageLoader(), QPointF(0, 0));
    loader.id = QStringLiteral("loader");
    loader.enabled = false;
    loader.parameters.insert(QStringLiteral("path"), QStringLiteral("Z:/not-exist.png"));
    doc.addNode(loader);

    VisionContext ctx;
    // Only disabled node -> topo order has it, but skipped; no success image path.
    // With only disabled vision nodes, execution completes with skipped modules.
    Selt::ProjectVariableStore vars;
    const bool ok = Selt::FlowRuntime::executeOnce(doc, ctx, vars, nullptr);
    QVERIFY(ok);
    QCOMPARE(ctx.moduleResults.value(loader.id).status, ModuleStatus::Disabled);
}

void RuntimeBindingTest::upstreamRoiOverridesParameterRoi()
{
    Selt::ExecutionRequest req;
    RoiRect paramRoi;
    paramRoi.enabled = true;
    paramRoi.rect = QRectF(0, 0, 10, 10);
    req.parameters.insert(QStringLiteral("roi"), paramRoi.toJson());

    RoiRect upstreamRoi;
    upstreamRoi.enabled = true;
    upstreamRoi.rect = QRectF(20, 30, 40, 50);
    req.inputs.insert(QStringLiteral("roi"), Selt::DataValue(upstreamRoi));

    const Selt::ExtendedRoi effective = Selt::resolveEffectiveRoi(req);
    QCOMPARE(effective.boundingRect().x(), 20.0);
    QCOMPARE(effective.boundingRect().y(), 30.0);
    QCOMPARE(effective.boundingRect().width(), 40.0);
    QCOMPARE(effective.boundingRect().height(), 50.0);
}

QTEST_MAIN(RuntimeBindingTest)
#include "test_runtime_bindings.moc"
