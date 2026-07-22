#include "core/model/document.h"
#include "vision/nodes/builtinexecutors.h"
#include "vision/registry/visionnodeids.h"
#include "vision/registry/visionnoderegistry.h"
#include "vision/runtime/flowscheduler.h"
#include "vision/runtime/runcontroller.h"

#include <QSignalSpy>
#include <QtTest>

class RunControllerTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void rejectsReentrancy();
    void emitsProgressAndBusy();
    void stopEndsLoop();
    void customSchedulerIsUsed();
    void runSingleNodeUsesSingleScope();
    void runFromNodeUsesUpstreamScope();
};

void RunControllerTest::initTestCase()
{
    Selt::registerBuiltInOpenCvExecutors();
}

void RunControllerTest::rejectsReentrancy()
{
    Document doc;
    NodeModel loader = VisionNodeRegistry::create(VisionNodeIds::imageLoader(), {});
    loader.id = QStringLiteral("l");
    loader.enabled = false;
    doc.addNode(loader);

    Selt::RunController controller;
    controller.setDocument(&doc);
    QVERIFY(controller.runOnce());
    // After completion not busy
    QVERIFY(!controller.isBusy());
    // Concurrent start while busy is rejected via isBusy guard in startLoop after manual flag simulation:
    // startLoop then immediate second startLoop should fail.
    QVERIFY(controller.startLoop(50));
    QVERIFY(controller.isBusy());
    QVERIFY(!controller.startLoop(50));
    QVERIFY(!controller.runOnce());
    controller.stop();
    QVERIFY(!controller.isBusy());
}

void RunControllerTest::emitsProgressAndBusy()
{
    Document doc;
    NodeModel loader = VisionNodeRegistry::create(VisionNodeIds::imageLoader(), {});
    loader.id = QStringLiteral("l");
    loader.enabled = false;
    doc.addNode(loader);

    Selt::RunController controller;
    controller.setDocument(&doc);
    QSignalSpy progressSpy(&controller, &Selt::RunController::runProgress);
    QSignalSpy busySpy(&controller, &Selt::RunController::busyChanged);
    QSignalSpy finishedSpy(&controller, &Selt::RunController::runFinished);

    QVERIFY(controller.runOnce());
    QVERIFY(finishedSpy.count() >= 1);
    QVERIFY(busySpy.count() >= 1);
    QVERIFY(progressSpy.count() >= 1);
    QCOMPARE(progressSpy.last().at(0).toString(), QStringLiteral("l"));
}

void RunControllerTest::stopEndsLoop()
{
    Document doc;
    NodeModel loader = VisionNodeRegistry::create(VisionNodeIds::imageLoader(), {});
    loader.id = QStringLiteral("l");
    loader.enabled = false;
    doc.addNode(loader);

    Selt::RunController controller;
    controller.setDocument(&doc);
    QVERIFY(controller.startLoop(100));
    controller.stop();
    QVERIFY(!controller.isBusy());
    QVERIFY(!controller.isRunning());
}

class CountingScheduler : public Selt::IFlowScheduler
{
public:
    int calls{0};
    Selt::ExecutionScopeKind lastScopeKind{Selt::ExecutionScopeKind::FullFlow};
    QString name() const override { return QStringLiteral("counting"); }
    bool execute(const Document &document, VisionContext &context,
                 const Selt::ProjectVariableStore &variables,
                 const Selt::RuntimeExecuteOptions &options) override
    {
        ++calls;
        lastScopeKind = options.scope.kind;
        return Selt::SequentialScheduler().execute(document, context, variables, options);
    }
};

void RunControllerTest::customSchedulerIsUsed()
{
    Document doc;
    NodeModel loader = VisionNodeRegistry::create(VisionNodeIds::imageLoader(), {});
    loader.id = QStringLiteral("l");
    loader.enabled = false;
    doc.addNode(loader);

    auto scheduler = std::make_shared<CountingScheduler>();
    Selt::RunController controller;
    controller.setDocument(&doc);
    controller.setScheduler(scheduler);
    QVERIFY(controller.runOnce());
    QCOMPARE(scheduler->calls, 1);
}

void RunControllerTest::runSingleNodeUsesSingleScope()
{
    Document doc;
    NodeModel loader = VisionNodeRegistry::create(VisionNodeIds::imageLoader(), {});
    loader.id = QStringLiteral("l");
    loader.enabled = false;
    doc.addNode(loader);

    auto scheduler = std::make_shared<CountingScheduler>();
    Selt::RunController controller;
    controller.setDocument(&doc);
    controller.setScheduler(scheduler);
    QVERIFY(controller.runSingleNode(QStringLiteral("l")));
    QCOMPARE(scheduler->lastScopeKind, Selt::ExecutionScopeKind::SingleNode);
}

void RunControllerTest::runFromNodeUsesUpstreamScope()
{
    Document doc;
    NodeModel loader = VisionNodeRegistry::create(VisionNodeIds::imageLoader(), {});
    loader.id = QStringLiteral("l");
    loader.enabled = false;
    doc.addNode(loader);

    auto scheduler = std::make_shared<CountingScheduler>();
    Selt::RunController controller;
    controller.setDocument(&doc);
    controller.setScheduler(scheduler);
    QVERIFY(controller.runFromNode(QStringLiteral("l")));
    QCOMPARE(scheduler->lastScopeKind, Selt::ExecutionScopeKind::NodeWithUpstream);
}

QTEST_MAIN(RunControllerTest)
#include "test_run_controller.moc"
