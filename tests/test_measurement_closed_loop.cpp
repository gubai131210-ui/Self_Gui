#include "vision/algorithms/caliperalgorithm.h"
#include "vision/algorithms/regionalgorithms.h"
#include "vision/domain/variablescope.h"
#include "vision/model/measurementresult.h"
#include "vision/nodes/builtinexecutors.h"
#include "vision/registry/communicationnodeids.h"
#include "vision/registry/visionnodeids.h"
#include "vision/registry/visionnoderegistry.h"
#include "vision/runtime/bindingresolver.h"
#include "vision/runtime/flowruntime.h"
#include "vision/runtime/inodeexecutor.h"
#include "vision/runtime/nodeexecutorregistry.h"
#include "vision/runtime/portvaluestore.h"
#include "vision/validation/graphvalidator.h"
#include "vision/workflow/workflowtemplates.h"
#include "vision/data/datatype.h"
#include "core/model/document.h"

#include <QtTest>
#include <opencv2/imgproc.hpp>

using namespace Selt;

class TestMeasurementClosedLoop : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void blobSelectIndexIsStable();
    void regionStatsCarryGeometry();
    void pointPointAndPerpendicularity();
    void positionDeviationAndInspectionPack();
    void scopeSnapshotInjectedIntoRuntime();
    void newWorkflowTemplatesExist();
    void portContractsAndBoolToPlc();
    void runtimeValuesResolveInBinding();
    void blobEmptyTargetBehavior();
    void publishWarnsUnsafeModbusWrite();
};

void TestMeasurementClosedLoop::initTestCase()
{
    registerBuiltInOpenCvExecutors();
}

static VisionImage makeTwoBlobsImage()
{
    cv::Mat mat(120, 160, CV_8UC1, cv::Scalar(0));
    cv::circle(mat, cv::Point(40, 60), 12, cv::Scalar(255), cv::FILLED);   // smaller
    cv::circle(mat, cv::Point(110, 60), 22, cv::Scalar(255), cv::FILLED);  // larger
    return VisionImage(mat);
}

void TestMeasurementClosedLoop::blobSelectIndexIsStable()
{
    BlobAnalyzeAlgorithm::Options opts;
    opts.minArea = 10.0;
    opts.sortBy = QStringLiteral("area");
    opts.selectIndex = 0;
    RegionData region;
    QVector<double> scores;
    VisionImage overlay;
    int selected = -1;
    QVERIFY(BlobAnalyzeAlgorithm::apply(makeTwoBlobsImage(), opts, region, scores, overlay, &selected));
    QCOMPARE(region.regions.size(), 2);
    QCOMPARE(selected, 0);
    QVERIFY(region.regions.at(0).area >= region.regions.at(1).area);

    opts.selectIndex = 1;
    QVERIFY(BlobAnalyzeAlgorithm::apply(makeTwoBlobsImage(), opts, region, scores, overlay, &selected));
    QCOMPARE(selected, 1);

    auto executor = NodeExecutorRegistry::instance().create(VisionNodeIds::blobAnalyze());
    QVERIFY(executor);
    ExecutionRequest req;
    req.nodeId = QStringLiteral("blob");
    req.typeId = VisionNodeIds::blobAnalyze();
    req.inputs.insert(QStringLiteral("image"), DataValue(makeTwoBlobsImage()));
    req.parameters.insert(QStringLiteral("minArea"), 10.0);
    req.parameters.insert(QStringLiteral("selectIndex"), 1);
    req.parameters.insert(QStringLiteral("sortBy"), QStringLiteral("area"));
    ExecutionContext ctx;
    const ExecutionResult result = executor->execute(req, ctx);
    QCOMPARE(result.status, ModuleStatus::Success);
    QCOMPARE(result.outputs.value(QStringLiteral("selectedIndex")).toInt(), 1);
    QVERIFY(result.outputs.contains(QStringLiteral("area")));
    QVERIFY(result.outputs.contains(QStringLiteral("center")));
}

void TestMeasurementClosedLoop::regionStatsCarryGeometry()
{
    BlobAnalyzeAlgorithm::Options opts;
    opts.minArea = 10.0;
    RegionData region;
    QVector<double> scores;
    VisionImage overlay;
    int selected = -1;
    QVERIFY(BlobAnalyzeAlgorithm::apply(makeTwoBlobsImage(), opts, region, scores, overlay, &selected));
    QVERIFY(!region.regions.isEmpty());
    const RegionStats &s = region.regions.first();
    QVERIFY(s.area > 0.0);
    QVERIFY(s.circularity > 0.0);
    QVERIFY(s.aspectRatio >= 1.0);
    QVERIFY(s.width > 0.0 && s.height > 0.0);
    QVERIFY(s.boundingRect.width() > 0.0);
}

void TestMeasurementClosedLoop::pointPointAndPerpendicularity()
{
    double angle = 0.0;
    double deviation = 0.0;
    QVERIFY(PerpendicularityAlgorithm::fromLines(QPointF(0, 0), QPointF(10, 0),
                                                 QPointF(0, 0), QPointF(0, 10),
                                                 angle, deviation));
    QVERIFY(deviation < 1e-6);

    auto executor = NodeExecutorRegistry::instance().create(VisionNodeIds::pointPointDistance());
    QVERIFY(executor);
    ExecutionRequest req;
    req.inputs.insert(QStringLiteral("point1"), DataValue(QPointF(0, 0)));
    req.inputs.insert(QStringLiteral("point2"), DataValue(QPointF(30, 40)));
    ExecutionContext ctx;
    const ExecutionResult result = executor->execute(req, ctx);
    QCOMPARE(result.status, ModuleStatus::Success);
    QCOMPARE(result.outputs.value(QStringLiteral("distance")).toReal(), 50.0);
}

void TestMeasurementClosedLoop::positionDeviationAndInspectionPack()
{
    double dx = 0.0;
    double dy = 0.0;
    double distance = 0.0;
    QVERIFY(PositionDeviationAlgorithm::fromPoints(QPointF(13, 14), QPointF(10, 10), dx, dy, distance));
    QCOMPARE(dx, 3.0);
    QCOMPARE(dy, 4.0);
    QCOMPARE(distance, 5.0);

    const InspectionResult packed = InspectionResult::fromDecision(true, 12.5, QStringLiteral("合格"));
    QVERIFY(packed.ok);
    QCOMPARE(packed.state, QStringLiteral("ok"));
    QCOMPARE(packed.measurement.decisionState, QStringLiteral("ok"));

    auto executor = NodeExecutorRegistry::instance().create(VisionNodeIds::inspectionPack());
    QVERIFY(executor);
    ExecutionRequest req;
    req.inputs.insert(QStringLiteral("ok"), DataValue(false));
    req.inputs.insert(QStringLiteral("value"), DataValue(9.0));
    ExecutionContext ctx;
    const ExecutionResult result = executor->execute(req, ctx);
    QCOMPARE(result.status, ModuleStatus::Success);
    QCOMPARE(result.outputs.value(QStringLiteral("state")).toString(), QStringLiteral("ng"));
    QCOMPARE(result.outputs.value(QStringLiteral("pass")).toBool(), false);
    QVERIFY(result.outputs.contains(QStringLiteral("resultValue")));
}

void TestMeasurementClosedLoop::scopeSnapshotInjectedIntoRuntime()
{
    VariableScopeSnapshot snap = VariableScopeSnapshot::fromStores(nullptr);
    const QString id = snap.project.add(QStringLiteral("threshold"), DataTypeId::Real, DataValue(42.0));
    QVERIFY(!id.isEmpty());

    NodeModel node = VisionNodeRegistry::create(VisionNodeIds::variableWrite(), QPointF(0, 0));
    node.id = QStringLiteral("vw");
    node.parameters.insert(QStringLiteral("key"), QStringLiteral("runtime_ok"));
    Document doc;
    doc.addNode(node);

    RuntimeExecuteOptions options;
    options.hasVariableScopes = true;
    options.variableScopes = snap;

    VisionContext ctx;
    QVERIFY(FlowRuntime::executeOnce(doc, ctx, snap.project, options));

    // variableWrite with no value input writes 0 default; still succeeds and stores key.
    auto executor = NodeExecutorRegistry::instance().create(VisionNodeIds::variableWrite());
    ExecutionRequest req;
    req.parameters.insert(QStringLiteral("key"), QStringLiteral("ok_flag"));
    req.inputs.insert(QStringLiteral("value"), DataValue(1.0));
    ExecutionContext execCtx;
    VariableScopeSnapshot live = snap;
    execCtx.setVariableScopeSnapshot(&live);
    const ExecutionResult wr = executor->execute(req, execCtx);
    QCOMPARE(wr.status, ModuleStatus::Success);
    QCOMPARE(execCtx.variable(QStringLiteral("ok_flag")).toReal(), 1.0);
    QCOMPARE(live.runtimeRead(QStringLiteral("ok_flag")).toReal(), 1.0);

    const VariableScopeChain chain = live.chain();
    QCOMPARE(chain.resolveById(id).record.value.toReal(), 42.0);
}

void TestMeasurementClosedLoop::newWorkflowTemplatesExist()
{
    const QStringList ids = WorkflowTemplates::templateIds();
    QVERIFY(ids.contains(QStringLiteral("blob_area_inspect")));
    QVERIFY(ids.contains(QStringLiteral("blob_count_inspect")));
    QVERIFY(ids.contains(QStringLiteral("geometry_circle_inspect")));
    QVERIFY(ids.contains(QStringLiteral("vision_to_plc")));

    const WorkflowTemplateSpec area = WorkflowTemplates::build(QStringLiteral("blob_area_inspect"));
    QVERIFY(area.nodes.size() >= 5);
    QVERIFY(!area.connections.isEmpty());
}

void TestMeasurementClosedLoop::portContractsAndBoolToPlc()
{
    QVERIFY(dataTypesCompatible(DataTypeId::Bool, DataTypeId::Real));
    DataValue converted;
    QVERIFY(DataValue(true).convertTo(DataTypeId::Real, &converted));
    QCOMPARE(converted.toReal(), 1.0);

    const ModuleDescriptor pack = VisionNodeRegistry::descriptor(VisionNodeIds::inspectionPack());
    QStringList outIds;
    for (const ModulePortDef &p : pack.ports) {
        if (!p.isInput)
            outIds.append(p.id);
    }
    QVERIFY(outIds.contains(QStringLiteral("pass")));
    QVERIFY(outIds.contains(QStringLiteral("resultValue")));
    QVERIFY(outIds.contains(QStringLiteral("measurement")));

    const ModuleDescriptor write = VisionNodeRegistry::descriptor(CommunicationNodeIds::modbusWrite());
    bool hasOkIn = false;
    for (const ModulePortDef &p : write.ports) {
        if (p.isInput && p.id == QLatin1String("okIn"))
            hasOkIn = true;
    }
    QVERIFY(hasOkIn);

    Document doc;
    NodeModel decide = VisionNodeRegistry::create(VisionNodeIds::toleranceDecision(), QPointF(0, 0));
    NodeModel modbus = VisionNodeRegistry::create(CommunicationNodeIds::modbusWrite(), QPointF(200, 0));
    decide.id = QStringLiteral("decide");
    modbus.id = QStringLiteral("modbus");
    doc.addNode(decide);
    doc.addNode(modbus);
    QString err;
    QVERIFY(GraphValidator::canConnect(doc, decide.id, QStringLiteral("ok"),
                                       modbus.id, QStringLiteral("okIn"), &err, true));
    QVERIFY(GraphValidator::canConnect(doc, decide.id, QStringLiteral("ok"),
                                       modbus.id, QStringLiteral("value"), &err, true));

    const WorkflowTemplateSpec plc = WorkflowTemplates::build(QStringLiteral("vision_to_plc"));
    bool linkedOkIn = false;
    for (const ConnectionModel &c : plc.connections) {
        if (c.targetPortId == QLatin1String("okIn"))
            linkedOkIn = true;
    }
    QVERIFY(linkedOkIn);
}

void TestMeasurementClosedLoop::runtimeValuesResolveInBinding()
{
    VariableScopeSnapshot snap = VariableScopeSnapshot::fromStores(nullptr);
    snap.runtimeWrite(QStringLiteral("ok_flag"), DataValue(7.5));
    const VariableScopeChain chain = snap.chain();
    QVERIFY(chain.containsRuntimeKey(QStringLiteral("ok_flag")));

    ParameterBinding binding = ParameterBinding::projectVariable(QStringLiteral("ok_flag"),
                                                                 DataTypeId::Real);
    PortValueStore ports;
    const BindingResolveResult resolved =
        BindingResolver::resolve(binding, DataTypeId::Real, ports, chain);
    QVERIFY(resolved.ok);
    QCOMPARE(resolved.value.toReal(), 7.5);
}

void TestMeasurementClosedLoop::blobEmptyTargetBehavior()
{
    VisionImage empty(cv::Mat(80, 80, CV_8UC1, cv::Scalar(0)));

    auto analyze = NodeExecutorRegistry::instance().create(VisionNodeIds::blobAnalyze());
    QVERIFY(analyze);
    ExecutionRequest req;
    req.nodeId = QStringLiteral("blob");
    req.typeId = VisionNodeIds::blobAnalyze();
    req.inputs.insert(QStringLiteral("image"), DataValue(empty));
    req.parameters.insert(QStringLiteral("minArea"), 50.0);
    ExecutionContext ctx;
    ExecutionResult soft = analyze->execute(req, ctx);
    QCOMPARE(soft.status, ModuleStatus::Success);
    QCOMPARE(soft.outputs.value(QStringLiteral("count")).toInt(), 0);
    QCOMPARE(soft.outputs.value(QStringLiteral("selectedIndex")).toInt(), -1);
    QCOMPARE(soft.outputs.value(QStringLiteral("area")).toReal(), 0.0);

    req.parameters.insert(QStringLiteral("requireTarget"), true);
    ExecutionResult hard = analyze->execute(req, ctx);
    QCOMPARE(hard.status, ModuleStatus::Failed);

    auto locate = NodeExecutorRegistry::instance().create(VisionNodeIds::blobLocate());
    QVERIFY(locate);
    ExecutionRequest locReq;
    locReq.nodeId = QStringLiteral("locate");
    locReq.typeId = VisionNodeIds::blobLocate();
    locReq.inputs.insert(QStringLiteral("image"), DataValue(empty));
    locReq.parameters.insert(QStringLiteral("minArea"), 50.0);
    ExecutionResult loc = locate->execute(locReq, ctx);
    QCOMPARE(loc.status, ModuleStatus::Failed);
}

void TestMeasurementClosedLoop::publishWarnsUnsafeModbusWrite()
{
    Document safeDoc;
    NodeModel mockWrite = VisionNodeRegistry::create(CommunicationNodeIds::modbusWrite(), QPointF());
    mockWrite.id = QStringLiteral("w1");
    mockWrite.parameters.insert(QStringLiteral("allowWrite"), false);
    mockWrite.parameters.insert(QStringLiteral("useMock"), true);
    safeDoc.addNode(mockWrite);
    const QVector<GraphDiagnostic> safeDiags = GraphValidator::validateForPublish(safeDoc);
    bool sawProtected = false;
    for (const GraphDiagnostic &d : safeDiags) {
        if (d.code == QLatin1String("modbus_write_protected"))
            sawProtected = true;
    }
    QVERIFY(sawProtected);

    Document realDoc;
    NodeModel realWrite = VisionNodeRegistry::create(CommunicationNodeIds::modbusWrite(), QPointF());
    realWrite.id = QStringLiteral("w2");
    realWrite.parameters.insert(QStringLiteral("allowWrite"), true);
    realWrite.parameters.insert(QStringLiteral("useMock"), false);
    realDoc.addNode(realWrite);
    const QVector<GraphDiagnostic> realDiags = GraphValidator::validateForPublish(realDoc);
    bool sawRealWarn = false;
    for (const GraphDiagnostic &d : realDiags) {
        if (d.code == QLatin1String("modbus_real_write")
            && d.severity == GraphDiagnosticSeverity::Warning) {
            sawRealWarn = true;
        }
    }
    QVERIFY(sawRealWarn);
}

QTEST_MAIN(TestMeasurementClosedLoop)
#include "test_measurement_closed_loop.moc"
