#include "vision/runtime/runanalysis.h"

#include <QtTest>

class RunAnalysisTest : public QObject
{
    Q_OBJECT

private slots:
    void summarizesNodeTimings();
    void explainsModuleFailure();
    void explainsPreflightFailure();
    void handlesEmptyContext();
};

void RunAnalysisTest::summarizesNodeTimings()
{
    VisionContext context;
    context.elapsedMs = 17;
    context.executionOrder = {QStringLiteral("fast"), QStringLiteral("slow"),
                              QStringLiteral("disabled")};

    ModuleRunResult fast;
    fast.status = ModuleStatus::Success;
    fast.elapsedMs = 4;
    fast.displayName = QStringLiteral("Fast");
    context.moduleResults.insert(QStringLiteral("fast"), fast);

    ModuleRunResult slow;
    slow.status = ModuleStatus::Failed;
    slow.elapsedMs = 9;
    slow.displayName = QStringLiteral("Slow");
    context.moduleResults.insert(QStringLiteral("slow"), slow);

    ModuleRunResult disabled;
    disabled.status = ModuleStatus::Disabled;
    context.moduleResults.insert(QStringLiteral("disabled"), disabled);

    const Selt::RuntimePerformanceSummary summary = Selt::summarizePerformance(context);
    QCOMPARE(summary.totalElapsedMs, qint64(17));
    QCOMPARE(summary.nodeElapsedMs, qint64(13));
    QCOMPARE(summary.nodeCount, 3);
    QCOMPARE(summary.successCount, 1);
    QCOMPARE(summary.failedCount, 1);
    QCOMPARE(summary.disabledCount, 1);
    QCOMPARE(summary.slowestNodeId, QStringLiteral("slow"));
    QCOMPARE(summary.slowestNodeName, QStringLiteral("Slow"));
    QCOMPARE(summary.slowestElapsedMs, qint64(9));
}

void RunAnalysisTest::explainsModuleFailure()
{
    VisionContext context;
    context.failedNodeId = QStringLiteral("threshold");

    ModuleRunResult result;
    result.displayName = QStringLiteral("Threshold");
    result.failureKind = QStringLiteral("execution");
    result.diagnosticCode = QStringLiteral("opencv_exception");
    result.errorMessage = QStringLiteral("输入图像无效");
    context.moduleResults.insert(context.failedNodeId, result);

    const Selt::RuntimeFailureExplanation explanation = Selt::explainFailure(context);
    QVERIFY(explanation.isValid());
    QCOMPARE(explanation.nodeId, QStringLiteral("threshold"));
    QCOMPARE(explanation.nodeName, QStringLiteral("Threshold"));
    QCOMPARE(explanation.failureKind, QStringLiteral("execution"));
    QCOMPARE(explanation.diagnosticCode, QStringLiteral("opencv_exception"));
    QCOMPARE(explanation.message, QStringLiteral("输入图像无效"));
}

void RunAnalysisTest::explainsPreflightFailure()
{
    VisionContext context;
    context.errorMessage = QStringLiteral("运行前静态校验失败");
    context.runtimeDiagnostics.append(Selt::RuntimeDiagnostic::make(
        Selt::RuntimeFailureKind::Validation, QStringLiteral("缺少输入"),
        QStringLiteral("measure"), QStringLiteral("missing_input")));

    const Selt::RuntimeFailureExplanation explanation = Selt::explainFailure(context);
    QVERIFY(explanation.isValid());
    QCOMPARE(explanation.nodeId, QStringLiteral("measure"));
    QCOMPARE(explanation.failureKind, QStringLiteral("validation"));
    QCOMPARE(explanation.diagnosticCode, QStringLiteral("missing_input"));
    QCOMPARE(explanation.message, QStringLiteral("缺少输入"));
}

void RunAnalysisTest::handlesEmptyContext()
{
    const VisionContext context;
    const Selt::RuntimePerformanceSummary summary = Selt::summarizePerformance(context);
    const Selt::RuntimeFailureExplanation explanation = Selt::explainFailure(context);

    QCOMPARE(summary.nodeCount, 0);
    QCOMPARE(summary.totalElapsedMs, qint64(0));
    QVERIFY(!explanation.isValid());
}

QTEST_MAIN(RunAnalysisTest)
#include "test_run_analysis.moc"
