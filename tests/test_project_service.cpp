#include "vision/project/projectservice.h"
#include "vision/data/datatype.h"
#include "vision/registry/visionnodeids.h"
#include "vision/registry/visionnoderegistry.h"

#include "core/command/documentcommands.h"
#include "core/serialization/documentserializer.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QUndoStack>

class TestProjectService : public QObject
{
    Q_OBJECT

private slots:
    void newProjectHasMainFlow();
    void createSwitchRenameRemoveFlows();
    void flowsDoNotLeakNodes();
    void legacyOpenSaveRoundTrip();
    void openBadFileReportsError();
    void perFlowUndoStacksAreIndependent();
    void documentEditsSyncToActiveFlow();
    void projectVariablesPersistAcrossSaveLoad();
    void parameterBindingsSurviveFlowSwitch();
};

void TestProjectService::newProjectHasMainFlow()
{
    Selt::ProjectService service;
    QCOMPARE(service.project()->flowCount(), 1);
    QCOMPARE(service.project()->activeFlow()->name(), QStringLiteral("主流程"));
    QVERIFY(service.project()->activeFlow()->isMainFlow());
    QVERIFY(service.document() != nullptr);
    QVERIFY(service.activeUndoStack() != nullptr);
    QVERIFY(!service.isModified());
}

void TestProjectService::createSwitchRenameRemoveFlows()
{
    Selt::ProjectService service;
    VisionFlow *a = service.createFlow(QStringLiteral("流程A"));
    VisionFlow *b = service.createFlow(QStringLiteral("流程B"));
    QVERIFY(a && b);
    QCOMPARE(service.project()->flowCount(), 3);
    QCOMPARE(service.project()->activeFlowId(), b->flowId());

    QVERIFY(service.renameFlow(a->flowId(), QStringLiteral("流程A2")));
    QCOMPARE(a->name(), QStringLiteral("流程A2"));

    QVERIFY(service.setActiveFlow(a->flowId()));
    QCOMPARE(service.project()->activeFlowId(), a->flowId());

    QVERIFY(service.removeFlow(a->flowId()));
    QCOMPARE(service.project()->flowCount(), 2);
    QVERIFY(service.project()->activeFlowId() != a->flowId());
}

void TestProjectService::flowsDoNotLeakNodes()
{
    Selt::ProjectService service;
    VisionFlow *a = service.createFlow(QStringLiteral("A"));
    NodeModel nodeA = VisionNodeRegistry::create(VisionNodeIds::grayscale(), QPointF(10, 10));
    nodeA.id = QStringLiteral("node_a");
    service.document()->addNode(nodeA);
    service.syncDocumentToActiveFlow();
    QCOMPARE(a->nodes().size(), 1);

    VisionFlow *b = service.createFlow(QStringLiteral("B"));
    QCOMPARE(service.document()->nodes().size(), 0);
    NodeModel nodeB = VisionNodeRegistry::create(VisionNodeIds::threshold(), QPointF(20, 20));
    nodeB.id = QStringLiteral("node_b");
    service.document()->addNode(nodeB);
    service.syncDocumentToActiveFlow();
    QCOMPARE(b->nodes().size(), 1);
    QCOMPARE(a->nodes().size(), 1);
    QCOMPARE(a->nodes().first().id, QStringLiteral("node_a"));

    QVERIFY(service.setActiveFlow(a->flowId()));
    QCOMPARE(service.document()->nodes().size(), 1);
    QCOMPARE(service.document()->nodes().first().id, QStringLiteral("node_a"));
}

void TestProjectService::legacyOpenSaveRoundTrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("svc.selt"));

    Selt::ProjectService service;
    service.project()->setTitle(QStringLiteral("服务存储"));
    NodeModel node = VisionNodeRegistry::create(VisionNodeIds::grayscale(), QPointF(3, 4));
    service.document()->addNode(node);
    service.syncDocumentToActiveFlow();

    QString error;
    QVERIFY(service.saveLegacySelt(path, &error));
    QVERIFY(!service.isModified());

    Selt::ProjectService loaded;
    Selt::MigrationReport report;
    QVERIFY(loaded.openLegacySelt(path, &report));
    QVERIFY(report.ok);
    QCOMPARE(loaded.project()->title(), QStringLiteral("服务存储"));
    QCOMPARE(loaded.project()->activeFlow()->name(), QStringLiteral("主流程"));
    QCOMPARE(loaded.document()->nodes().size(), 1);
}

void TestProjectService::openBadFileReportsError()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("bad.selt"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("{ not-json");
    file.close();

    Selt::ProjectService service;
    Selt::MigrationReport report;
    QVERIFY(!service.openLegacySelt(path, &report));
    QVERIFY(!report.ok);
    QVERIFY(!report.error.isEmpty());
}

void TestProjectService::perFlowUndoStacksAreIndependent()
{
    Selt::ProjectService service;
    VisionFlow *a = service.createFlow(QStringLiteral("A"));
    QUndoStack *stackA = service.activeUndoStack();
    QVERIFY(stackA);

    NodeModel node = VisionNodeRegistry::create(VisionNodeIds::grayscale(), QPointF(1, 1));
    stackA->push(new AddNodeCommand(service.document(), node));
    QCOMPARE(stackA->count(), 1);
    QCOMPARE(service.document()->nodes().size(), 1);

    VisionFlow *b = service.createFlow(QStringLiteral("B"));
    QUndoStack *stackB = service.activeUndoStack();
    QVERIFY(stackB);
    QVERIFY(stackA != stackB);
    QCOMPARE(stackB->count(), 0);
    QCOMPARE(service.document()->nodes().size(), 0);

    QVERIFY(service.setActiveFlow(a->flowId()));
    QCOMPARE(service.activeUndoStack(), stackA);
    QCOMPARE(service.document()->nodes().size(), 1);
    stackA->undo();
    QCOMPARE(service.document()->nodes().size(), 0);
    // Flow B untouched
    QVERIFY(service.setActiveFlow(b->flowId()));
    QCOMPARE(service.document()->nodes().size(), 0);
    QCOMPARE(stackB->count(), 0);
}

void TestProjectService::documentEditsSyncToActiveFlow()
{
    Selt::ProjectService service;
    NodeModel node = VisionNodeRegistry::create(VisionNodeIds::threshold(), QPointF(5, 5));
    node.id = QStringLiteral("sync_node");
    service.document()->addNode(node);
    // Document signals should sync into active flow automatically.
    QCOMPARE(service.project()->activeFlow()->nodes().size(), 1);
    QCOMPARE(service.project()->activeFlow()->nodes().first().id, QStringLiteral("sync_node"));
    QVERIFY(service.isModified());
}

void TestProjectService::projectVariablesPersistAcrossSaveLoad()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("vars.selt"));

    Selt::ProjectService service;
    const QString varId = service.project()->variables().add(
        QStringLiteral("thresholdVar"), Selt::DataTypeId::Int, Selt::DataValue(77),
        QStringLiteral("测试变量"));
    QVERIFY(!varId.isEmpty());
    service.project()->setModified(true);

    NodeModel node = VisionNodeRegistry::create(VisionNodeIds::threshold(), QPointF(1, 1));
    node.id = QStringLiteral("n1");
    node.parameterBindings.insert(
        QStringLiteral("threshold"),
        Selt::ParameterBinding::projectVariable(varId, Selt::DataTypeId::Int).toJson());
    service.document()->addNode(node);
    service.syncDocumentToActiveFlow();

    QString error;
    QVERIFY(service.saveLegacySelt(path, &error));

    Selt::ProjectService loaded;
    Selt::MigrationReport report;
    QVERIFY(loaded.openLegacySelt(path, &report));
    QVERIFY(report.ok);
    QVERIFY(loaded.project()->variables().contains(varId));
    QCOMPARE(loaded.project()->variables().get(varId).toInt(), 77);
    QCOMPARE(loaded.document()->nodes().size(), 1);
    const NodeModel restored = loaded.document()->nodes().first();
    QVERIFY(restored.parameterBindings.contains(QStringLiteral("threshold")));
    const auto binding = Selt::ParameterBinding::fromJson(
        restored.parameterBindings.value(QStringLiteral("threshold")).toObject());
    QCOMPARE(binding.kind, Selt::ParameterSourceKind::ProjectVariable);
    QCOMPARE(binding.projectVariableId, varId);
}

void TestProjectService::parameterBindingsSurviveFlowSwitch()
{
    Selt::ProjectService service;
    VisionFlow *a = service.createFlow(QStringLiteral("A"));
    NodeModel nodeA = VisionNodeRegistry::create(VisionNodeIds::threshold(), QPointF(0, 0));
    nodeA.id = QStringLiteral("a_thr");
    nodeA.parameterBindings.insert(
        QStringLiteral("threshold"),
        Selt::ParameterBinding::upstream(QStringLiteral("up"), QStringLiteral("width"),
                                         Selt::DataTypeId::Int)
            .toJson());
    service.document()->addNode(nodeA);
    service.syncDocumentToActiveFlow();

    VisionFlow *b = service.createFlow(QStringLiteral("B"));
    NodeModel nodeB = VisionNodeRegistry::create(VisionNodeIds::grayscale(), QPointF(0, 0));
    nodeB.id = QStringLiteral("b_gray");
    service.document()->addNode(nodeB);
    service.syncDocumentToActiveFlow();

    QVERIFY(service.setActiveFlow(a->flowId()));
    QCOMPARE(service.document()->nodes().size(), 1);
    const NodeModel again = service.document()->nodes().first();
    QCOMPARE(again.id, QStringLiteral("a_thr"));
    QVERIFY(again.parameterBindings.contains(QStringLiteral("threshold")));
    const auto binding = Selt::ParameterBinding::fromJson(
        again.parameterBindings.value(QStringLiteral("threshold")).toObject());
    QCOMPARE(binding.kind, Selt::ParameterSourceKind::UpstreamBinding);
    QCOMPARE(binding.upstreamNodeId, QStringLiteral("up"));
    Q_UNUSED(b);
}

QTEST_MAIN(TestProjectService)
#include "test_project_service.moc"
