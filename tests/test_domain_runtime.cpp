#include "foundation/seltversion.h"
#include "vision/data/datatype.h"
#include "vision/domain/visionproject.h"
#include "vision/model/extendedroi.h"
#include "vision/nodes/builtinexecutors.h"
#include "vision/runtime/flowruntime.h"
#include "vision/runtime/nodeexecutorregistry.h"
#include "vision/storage/projectstorage.h"

#include "core/model/document.h"
#include "core/serialization/documentserializer.h"
#include "vision/registry/visionnoderegistry.h"
#include "vision/registry/visionnodeids.h"

#include <QTemporaryDir>
#include <QTest>

class TestDomainRuntime : public QObject
{
    Q_OBJECT

private slots:
    void foundationVersionAvailable();
    void projectCreatesMainFlow();
    void flowOrderIsStable();
    void removeActiveFlowFallsBack();
    void renameFlowUpdatesList();
    void graphRejectsDuplicateIds();
    void graphRejectsDanglingConnections();
    void flowChangesMarkProjectModified();
    void documentFlowRoundTripSync();
    void typedDataRoundTrip();
    void extendedRoiJson();
    void executorRegistryHasBuiltins();
    void topoIsDeterministic();
    void projectStorageLegacyRoundTrip();
    void projectStorageReportsMigrationNotes();
    void nodeEnabledRoundTrip();
};

void TestDomainRuntime::foundationVersionAvailable()
{
    QVERIFY(!Selt::VersionInfo::productName().isEmpty());
    QCOMPARE(Selt::VersionInfo::pluginSdkVersion, 1);
}

void TestDomainRuntime::projectCreatesMainFlow()
{
    VisionProject project;
    QVERIFY(project.activeFlow() != nullptr);
    QCOMPARE(project.flowCount(), 1);
    QCOMPARE(project.flowIds().size(), 1);
    QVERIFY(project.activeFlow()->isMainFlow());
    QCOMPARE(project.activeFlow()->name(), QStringLiteral("主流程"));
    VisionFlow *second = project.createFlow(QStringLiteral("子流程A"));
    QVERIFY(second);
    QCOMPARE(project.flowCount(), 2);
}

void TestDomainRuntime::flowOrderIsStable()
{
    VisionProject project;
    project.createFlow(QStringLiteral("A"));
    project.createFlow(QStringLiteral("B"));
    project.createFlow(QStringLiteral("C"));
    QCOMPARE(project.flowNames(),
             (QStringList{QStringLiteral("主流程"), QStringLiteral("A"),
                          QStringLiteral("B"), QStringLiteral("C")}));
    QCOMPARE(project.flows().size(), 4);
    QCOMPARE(project.flows().at(1)->name(), QStringLiteral("A"));
}

void TestDomainRuntime::removeActiveFlowFallsBack()
{
    VisionProject project;
    VisionFlow *a = project.createFlow(QStringLiteral("A"));
    VisionFlow *b = project.createFlow(QStringLiteral("B"));
    QVERIFY(project.setActiveFlow(a->flowId()));
    QVERIFY(project.removeFlow(a->flowId()));
    QCOMPARE(project.activeFlowId(), b->flowId());
    QCOMPARE(project.flowCount(), 2); // 主流程 + B

    while (project.flowCount() > 1)
        QVERIFY(project.removeFlow(project.flowIds().last()));
    QVERIFY(!project.removeFlow(project.activeFlowId()));
    QCOMPARE(project.flowCount(), 1);
}

void TestDomainRuntime::renameFlowUpdatesList()
{
    VisionProject project;
    VisionFlow *flow = project.createFlow(QStringLiteral("旧名"));
    QVERIFY(project.renameFlow(flow->flowId(), QStringLiteral("新名")));
    QCOMPARE(flow->name(), QStringLiteral("新名"));
    QVERIFY(project.flowNames().contains(QStringLiteral("新名")));
}

void TestDomainRuntime::graphRejectsDuplicateIds()
{
    VisionFlow flow(QStringLiteral("f1"), QStringLiteral("测试"));
    NodeModel n1;
    n1.id = QStringLiteral("n1");
    n1.type = QStringLiteral("rectangle");
    NodeModel n2 = n1;
    QString error;
    QVERIFY(!flow.replaceGraph({n1, n2}, {}, &error));
    QVERIFY(error.contains(QStringLiteral("重复")));
}

void TestDomainRuntime::graphRejectsDanglingConnections()
{
    VisionFlow flow(QStringLiteral("f1"), QStringLiteral("测试"));
    NodeModel n1;
    n1.id = QStringLiteral("n1");
    ConnectionModel c;
    c.id = QStringLiteral("c1");
    c.sourceNodeId = QStringLiteral("n1");
    c.targetNodeId = QStringLiteral("missing");
    QString error;
    QVERIFY(!flow.replaceGraph({n1}, {c}, &error));
    QVERIFY(error.contains(QStringLiteral("不存在")));
}

void TestDomainRuntime::flowChangesMarkProjectModified()
{
    VisionProject project;
    project.markClean();
    QVERIFY(!project.isModified());
    NodeModel node;
    node.id = QStringLiteral("n1");
    node.type = QStringLiteral("rectangle");
    QVERIFY(!project.activeFlow()->addNode(node).isEmpty());
    QVERIFY(project.isModified());
}

void TestDomainRuntime::documentFlowRoundTripSync()
{
    VisionProject project;
    Document document;
    NodeModel node;
    node.id = QStringLiteral("n_sync");
    node.type = QStringLiteral("rectangle");
    node.text = QStringLiteral("同步");
    document.addNode(node);

    QString error;
    QVERIFY(project.activeFlow()->replaceGraph(document.nodes(), document.connections(), &error));
    QCOMPARE(project.activeFlow()->nodes().size(), 1);

    Document exported;
    project.exportActiveFlowToDocument(exported);
    QCOMPARE(exported.nodes().size(), 1);
    QCOMPARE(exported.nodes().first().id, QStringLiteral("n_sync"));
}

void TestDomainRuntime::typedDataRoundTrip()
{
    Selt::DataValue v(42);
    QCOMPARE(v.type(), Selt::DataTypeId::Int);
    QCOMPARE(v.toInt(), 42);
    QVERIFY(Selt::dataTypesCompatible(Selt::DataTypeId::Image, Selt::DataTypeId::Image));
    QVERIFY(!Selt::dataTypesCompatible(Selt::DataTypeId::Image, Selt::DataTypeId::Int));
}

void TestDomainRuntime::extendedRoiJson()
{
    Selt::ExtendedRoi roi;
    roi.shape = Selt::RoiShapeType::Circle;
    roi.center = QPointF(10, 20);
    roi.radius = 5;
    roi.enabled = true;
    const Selt::ExtendedRoi restored = Selt::ExtendedRoi::fromJson(roi.toJson());
    QCOMPARE(restored.shape, Selt::RoiShapeType::Circle);
    QCOMPARE(restored.radius, 5.0);
    QVERIFY(restored.enabled);
}

void TestDomainRuntime::executorRegistryHasBuiltins()
{
    Selt::registerBuiltInOpenCvExecutors();
    QVERIFY(Selt::NodeExecutorRegistry::instance().contains(VisionNodeIds::imageLoader()));
    QVERIFY(Selt::NodeExecutorRegistry::instance().contains(VisionNodeIds::threshold()));
}

void TestDomainRuntime::topoIsDeterministic()
{
    Document doc;
    NodeModel a = VisionNodeRegistry::create(VisionNodeIds::imageLoader());
    NodeModel b = VisionNodeRegistry::create(VisionNodeIds::grayscale());
    NodeModel c = VisionNodeRegistry::create(VisionNodeIds::threshold());
    a.id = QStringLiteral("n_a");
    b.id = QStringLiteral("n_b");
    c.id = QStringLiteral("n_c");
    doc.addNode(a);
    doc.addNode(b);
    doc.addNode(c);
    ConnectionModel c1;
    c1.id = QStringLiteral("c1");
    c1.sourceNodeId = a.id;
    c1.sourcePortId = QStringLiteral("out");
    c1.targetNodeId = b.id;
    c1.targetPortId = QStringLiteral("in");
    doc.addConnection(c1);
    ConnectionModel c2 = c1;
    c2.id = QStringLiteral("c2");
    c2.sourceNodeId = b.id;
    c2.targetNodeId = c.id;
    doc.addConnection(c2);

    QString err;
    const QStringList order1 = Selt::FlowRuntime::topologicalOrder(doc, &err);
    const QStringList order2 = Selt::FlowRuntime::topologicalOrder(doc, &err);
    QCOMPARE(order1, order2);
    QCOMPARE(order1, (QStringList{QStringLiteral("n_a"), QStringLiteral("n_b"), QStringLiteral("n_c")}));
}

void TestDomainRuntime::projectStorageLegacyRoundTrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("demo.selt"));

    VisionProject project;
    project.setTitle(QStringLiteral("存储测试"));
    NodeModel node = VisionNodeRegistry::create(VisionNodeIds::grayscale(), QPointF(1, 2));
    project.activeFlow()->addNode(node);

    QString error;
    QVERIFY(Selt::ProjectStorage::saveLegacySelt(path, project, &error));

    VisionProject loaded;
    Selt::MigrationReport report;
    QVERIFY(Selt::ProjectStorage::loadLegacySelt(path, loaded, &report));
    QVERIFY(report.ok);
    QCOMPARE(loaded.title(), QStringLiteral("存储测试"));
    QCOMPARE(loaded.activeFlow()->nodes().size(), 1);
    QCOMPARE(loaded.activeFlow()->name(), QStringLiteral("主流程"));
}

void TestDomainRuntime::projectStorageReportsMigrationNotes()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("notes.selt"));

    Document doc;
    doc.setTitle(QStringLiteral("迁移报告"));
    QString error;
    QVERIFY(DocumentSerializer::saveToFile(doc, path, &error));

    VisionProject loaded;
    Selt::MigrationReport report;
    QVERIFY(Selt::ProjectStorage::loadLegacySelt(path, loaded, &report));
    QVERIFY(report.ok);
    QVERIFY(!report.notes.isEmpty());
    QCOMPARE(report.toVersion, DocumentSerializer::CurrentVersion);
}

void TestDomainRuntime::nodeEnabledRoundTrip()
{
    NodeModel node;
    node.id = QStringLiteral("n1");
    node.enabled = false;
    const NodeModel restored = NodeModel::fromJson(node.toJson());
    QVERIFY(!restored.enabled);
}

QTEST_MAIN(TestDomainRuntime)
#include "test_domain_runtime.moc"
