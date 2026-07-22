#include "core/command/documentcommands.h"
#include "core/model/document.h"
#include "vision/model/portexposure.h"
#include "vision/model/subflowsupport.h"
#include "vision/registry/visionnodeids.h"
#include "vision/registry/visionnoderegistry.h"
#include "vision/runtime/runrecord.h"
#include "vision/validation/graphvalidator.h"

#include <QUndoStack>
#include <QUuid>
#include <QtTest>

using namespace Selt;

class TestPortExposure : public QObject
{
    Q_OBJECT
private slots:
    void builtinDescriptorsHaveExposureHints();
    void newNodeExposesPrimaryPortsOnly();
    void connectedPortsStayVisible();
    void customExposureRoundTrip();
    void pluginWithoutHintsExposesAll();
    void pluginWithCapabilityGetsRoiAndHints();
    void upgradeKeepsCustomExposure();
    void upgradePreservesUnknownConnectedPorts();
    void exposureCommandUndoRedo();
    void hiddenPortStillValidates();
    void subflowTerminalsFromInterface();
    void runRecordSerializationAndPolicy();
};

void TestPortExposure::builtinDescriptorsHaveExposureHints()
{
    const ModuleDescriptor blob = VisionNodeRegistry::descriptor(VisionNodeIds::blobAnalyze());
    QVERIFY(blob.hasExposureHints);
    QVERIFY(blob.findPort(QStringLiteral("in")));
    QVERIFY(blob.findPort(QStringLiteral("out")));
    const QStringList defaults = defaultExposedPortIds(blob);
    QVERIFY(defaults.contains(QStringLiteral("in")));
    QVERIFY(defaults.contains(QStringLiteral("out")));
    QVERIFY(!defaults.contains(QStringLiteral("overlay")));
}

void TestPortExposure::newNodeExposesPrimaryPortsOnly()
{
    NodeModel node = VisionNodeRegistry::create(VisionNodeIds::blobAnalyze());
    QVERIFY(!node.portExposureCustomized);
    QVERIFY(node.exposedPortIds.contains(QStringLiteral("in")));
    QVERIFY(node.exposedPortIds.contains(QStringLiteral("out")));
    QVERIFY(!node.exposedPortIds.contains(QStringLiteral("overlay")));
}

void TestPortExposure::connectedPortsStayVisible()
{
    NodeModel src = VisionNodeRegistry::create(VisionNodeIds::imageLoader());
    NodeModel dst = VisionNodeRegistry::create(VisionNodeIds::blobAnalyze());
    dst.portExposureCustomized = true;
    dst.exposedPortIds = {QStringLiteral("in"), QStringLiteral("out")};

    ConnectionModel conn;
    conn.id = QStringLiteral("c1");
    conn.sourceNodeId = src.id;
    conn.sourcePortId = QStringLiteral("out");
    conn.targetNodeId = dst.id;
    conn.targetPortId = QStringLiteral("count");

    const ModuleDescriptor desc = VisionNodeRegistry::descriptor(dst.type);
    const QStringList visible = resolveExposedPortIds(dst, desc, {conn});
    QVERIFY(visible.contains(QStringLiteral("count")));
    QVERIFY(isPortExposedOnCanvas(dst, desc, QStringLiteral("count"), {conn}));
}

void TestPortExposure::customExposureRoundTrip()
{
    NodeModel node = VisionNodeRegistry::create(VisionNodeIds::rectangleMeasure());
    node.portExposureCustomized = true;
    node.exposedPortIds = {QStringLiteral("in"), QStringLiteral("width"), QStringLiteral("height")};
    const QJsonObject json = node.toJson();
    const NodeModel restored = NodeModel::fromJson(json);
    QVERIFY(restored.portExposureCustomized);
    QCOMPARE(restored.exposedPortIds, node.exposedPortIds);
}

void TestPortExposure::pluginWithoutHintsExposesAll()
{
    ModuleDescriptor plugin;
    plugin.typeId = QStringLiteral("test.plugin.noHints.%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    plugin.displayName = QStringLiteral("Plugin");
    plugin.ports = {
        {QStringLiteral("in"), QStringLiteral("In"), true, DataTypeId::Image, true, {}},
        {QStringLiteral("out"), QStringLiteral("Out"), false, DataTypeId::Image, false, {}},
        {QStringLiteral("score"), QStringLiteral("Score"), false, DataTypeId::Real, false, {}}};
    QVERIFY(!plugin.hasExposureHints);
    const QStringList ids = defaultExposedPortIds(plugin);
    QCOMPARE(ids.size(), 3);
}

void TestPortExposure::pluginWithCapabilityGetsRoiAndHints()
{
    ModuleDescriptor plugin;
    plugin.typeId = QStringLiteral("test.plugin.hints.%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    plugin.displayName = QStringLiteral("PluginHints");
    plugin.capabilityVersion = 1;
    plugin.hasExposureHints = true;
    plugin.ports = {
        {QStringLiteral("in"), QStringLiteral("In"), true, DataTypeId::Image, true, {}},
        {QStringLiteral("out"), QStringLiteral("Out"), false, DataTypeId::Image, false, {}}};
    ModuleParamDef roi;
    roi.key = QStringLiteral("roi");
    roi.label = QStringLiteral("ROI");
    roi.type = ModuleParamType::RoiRect;
    plugin.parameters = {roi};

    QString error;
    QVERIFY2(VisionNodeRegistry::registerExternalDescriptor(plugin, &error), qPrintable(error));
    const ModuleDescriptor stored = VisionNodeRegistry::descriptor(plugin.typeId);
    QVERIFY(stored.findInputPort(QStringLiteral("roi")));
    QVERIFY(stored.hasExposureHints);
}

void TestPortExposure::upgradeKeepsCustomExposure()
{
    QVector<NodeModel> nodes;
    NodeModel node = VisionNodeRegistry::create(VisionNodeIds::blobAnalyze());
    node.portExposureCustomized = true;
    node.exposedPortIds = {QStringLiteral("in"), QStringLiteral("count")};
    nodes.append(node);
    QVector<ConnectionModel> connections;
    QVERIFY(VisionNodeRegistry::upgradeGraph(nodes, connections));
    QCOMPARE(nodes.first().exposedPortIds.contains(QStringLiteral("in")), true);
    QCOMPARE(nodes.first().exposedPortIds.contains(QStringLiteral("count")), true);
    QVERIFY(nodes.first().portExposureCustomized);
}

void TestPortExposure::upgradePreservesUnknownConnectedPorts()
{
    NodeModel node = VisionNodeRegistry::create(VisionNodeIds::grayscale());
    PortModel ghost;
    ghost.id = QStringLiteral("legacyGhost");
    ghost.name = QStringLiteral("Ghost");
    ghost.direction = PortDirection::Output;
    ghost.dataType = QStringLiteral("image");
    node.ports.append(ghost);

    NodeModel sink = VisionNodeRegistry::create(VisionNodeIds::resultPreview());
    ConnectionModel conn;
    conn.id = QStringLiteral("ghost-conn");
    conn.sourceNodeId = node.id;
    conn.sourcePortId = QStringLiteral("legacyGhost");
    conn.targetNodeId = sink.id;
    conn.targetPortId = QStringLiteral("in");

    QVector<NodeModel> nodes{node, sink};
    QVector<ConnectionModel> connections{conn};
    QVERIFY(VisionNodeRegistry::upgradeGraph(nodes, connections));
    bool found = false;
    for (const PortModel &port : nodes.first().ports) {
        if (port.id == QStringLiteral("legacyGhost"))
            found = true;
    }
    QVERIFY(found);
}

void TestPortExposure::exposureCommandUndoRedo()
{
    Document doc;
    NodeModel node = VisionNodeRegistry::create(VisionNodeIds::blobAnalyze());
    doc.addNode(node);

    NodeModel oldNode = doc.node(node.id);
    NodeModel newNode = oldNode;
    newNode.portExposureCustomized = true;
    newNode.exposedPortIds = {QStringLiteral("in"), QStringLiteral("count")};

    QUndoStack stack;
    stack.push(new ChangeNodePortExposureCommand(&doc, oldNode, newNode));
    QCOMPARE(doc.node(node.id).exposedPortIds.contains(QStringLiteral("count")), true);
    stack.undo();
    QCOMPARE(doc.node(node.id).exposedPortIds.contains(QStringLiteral("count")), false);
    stack.redo();
    QCOMPARE(doc.node(node.id).exposedPortIds.contains(QStringLiteral("count")), true);
}

void TestPortExposure::hiddenPortStillValidates()
{
    NodeModel src = VisionNodeRegistry::create(VisionNodeIds::blobAnalyze());
    src.portExposureCustomized = true;
    src.exposedPortIds = {QStringLiteral("in"), QStringLiteral("out")};

    NodeModel dst = VisionNodeRegistry::create(VisionNodeIds::numericCompare());
    ConnectionModel conn;
    conn.id = QStringLiteral("hidden-wire");
    conn.sourceNodeId = src.id;
    conn.sourcePortId = QStringLiteral("count");
    conn.targetNodeId = dst.id;
    conn.targetPortId = QStringLiteral("value");

    const auto diags = GraphValidator::validateStructure({src, dst}, {conn});
    for (const GraphDiagnostic &d : diags)
        QVERIFY2(d.code != QLatin1String("missing_source_port"), qPrintable(d.message));
}

void TestPortExposure::subflowTerminalsFromInterface()
{
    NodeModel node = VisionNodeRegistry::create(VisionNodeIds::subflow());
    SubflowInterface iface;
    iface.schemaVersion = 1;
    iface.flowId = QStringLiteral("flow-inner");
    iface.displayName = QStringLiteral("内流程");
    TypedPortDef in;
    in.id = QStringLiteral("imageIn");
    in.name = QStringLiteral("图像入");
    in.isInput = true;
    in.dataType = DataTypeId::Image;
    in.required = true;
    TypedPortDef out;
    out.id = QStringLiteral("scoreOut");
    out.name = QStringLiteral("得分");
    out.isInput = false;
    out.dataType = DataTypeId::Real;
    iface.inputs = {in};
    iface.outputs = {out};
    QVERIFY(applySubflowInterfaceToNode(node, iface));
    QVERIFY(node.findPort(QStringLiteral("imageIn")));
    QVERIFY(node.findPort(QStringLiteral("scoreOut")));
    QCOMPARE(node.parameters.value(QStringLiteral("flowId")).toString(), QStringLiteral("flow-inner"));
}

void TestPortExposure::runRecordSerializationAndPolicy()
{
    ModuleRunResult result;
    result.nodeId = QStringLiteral("n1");
    result.type = VisionNodeIds::grayscale();
    result.displayName = QStringLiteral("灰度");
    result.status = ModuleStatus::Success;
    result.inputSummary.insert(QStringLiteral("image"), QStringLiteral("Image"));
    result.outputSummary.insert(QStringLiteral("out"), QStringLiteral("Image"));
    result.elapsedMs = 12;
    result.outputImage = VisionImage();

    const RunRecord debugRec = RunRecord::fromModuleResult(result, RunRecordImagePolicy::DebugCopy);
    QCOMPARE(debugRec.nodeId, QStringLiteral("n1"));
    QCOMPARE(debugRec.elapsedMs, 12);

    VisionContext ctx;
    ctx.executionOrder = {QStringLiteral("n1")};
    ctx.moduleResults.insert(QStringLiteral("n1"), result);
    const QVector<RunRecord> records = buildRunRecords(ctx, RunRecordImagePolicy::ProductionLite);
    QCOMPARE(records.size(), 1);
    const QJsonObject json = records.first().toJson();
    QCOMPARE(json.value(QStringLiteral("nodeId")).toString(), QStringLiteral("n1"));
    const RunRecord restored = RunRecord::fromJson(json);
    QCOMPARE(restored.displayName, QStringLiteral("灰度"));
}

QTEST_MAIN(TestPortExposure)
#include "test_port_exposure.moc"
