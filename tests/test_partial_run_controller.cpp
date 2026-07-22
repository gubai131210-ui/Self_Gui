#include "core/model/document.h"
#include "vision/registry/visionnodeids.h"
#include "vision/registry/visionnoderegistry.h"
#include "vision/runtime/runcontroller.h"

#include <QSignalSpy>
#include <QtTest>

class InspectingScheduler : public Selt::IFlowScheduler
{
public:
    Selt::ExecutionScope lastScope;
    int calls{0};

    QString name() const override { return QStringLiteral("inspecting"); }
    bool execute(const Document &, VisionContext &context,
                 const Selt::ProjectVariableStore &,
                 const Selt::RuntimeExecuteOptions &options) override
    {
        ++calls;
        lastScope = options.scope;
        context.elapsedMs = 1;
        return true;
    }
};

class TestPartialRunController : public QObject
{
    Q_OBJECT

private slots:
    void runScopeEmitsFinished();
};

void TestPartialRunController::runScopeEmitsFinished()
{
    Document document;
    NodeModel node = VisionNodeRegistry::create(VisionNodeIds::grayscale());
    node.id = QStringLiteral("g");
    document.addNode(node);

    auto scheduler = std::make_shared<InspectingScheduler>();
    Selt::RunController controller;
    controller.setDocument(&document);
    controller.setScheduler(scheduler);

    QSignalSpy finishedSpy(&controller, &Selt::RunController::runFinished);
    QVERIFY(controller.runScope(Selt::ExecutionScope::withUpstream(QStringLiteral("g"))));
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(scheduler->lastScope.kind, Selt::ExecutionScopeKind::NodeWithUpstream);
}

QTEST_MAIN(TestPartialRunController)
#include "test_partial_run_controller.moc"
