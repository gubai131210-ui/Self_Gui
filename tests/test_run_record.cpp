#include "vision/model/visioncontext.h"
#include "vision/runtime/runrecord.h"

#include <QtTest>

using namespace Selt;

class TestRunRecord : public QObject
{
    Q_OBJECT
private slots:
    void fromModuleResultKeepsSummaries();
    void productionPolicySkipsImageClone();
    void buildOrderedRecordsFromContext();
};

void TestRunRecord::fromModuleResultKeepsSummaries()
{
    ModuleRunResult result;
    result.nodeId = QStringLiteral("blob1");
    result.type = QStringLiteral("vision.blobAnalyze");
    result.displayName = QStringLiteral("Blob");
    result.status = ModuleStatus::Failed;
    result.failureKind = QStringLiteral("execution");
    result.diagnosticCode = QStringLiteral("no_target");
    result.errorMessage = QStringLiteral("未找到目标");
    result.inputSummary.insert(QStringLiteral("in"), QStringLiteral("Image"));
    result.outputSummary.insert(QStringLiteral("count"), QStringLiteral("0"));
    result.elapsedMs = 8;

    const RunRecord record = RunRecord::fromModuleResult(result);
    QCOMPARE(record.diagnosticCode, QStringLiteral("no_target"));
    QCOMPARE(record.failureKind, QStringLiteral("execution"));
    QCOMPARE(record.inputSummary.value(QStringLiteral("in")), QStringLiteral("Image"));
    const QJsonObject json = record.toJson();
    QCOMPARE(RunRecord::fromJson(json).errorMessage, QStringLiteral("未找到目标"));
}

void TestRunRecord::productionPolicySkipsImageClone()
{
    ModuleRunResult result;
    result.nodeId = QStringLiteral("n");
    result.status = ModuleStatus::Success;
    const RunRecord lite = RunRecord::fromModuleResult(result, RunRecordImagePolicy::ProductionLite);
    QVERIFY(lite.imageSnapshot.isEmpty());
}

void TestRunRecord::buildOrderedRecordsFromContext()
{
    ModuleRunResult a;
    a.nodeId = QStringLiteral("a");
    a.displayName = QStringLiteral("A");
    a.status = ModuleStatus::Success;
    ModuleRunResult b;
    b.nodeId = QStringLiteral("b");
    b.displayName = QStringLiteral("B");
    b.status = ModuleStatus::Failed;

    VisionContext ctx;
    ctx.executionOrder = {QStringLiteral("a"), QStringLiteral("b")};
    ctx.moduleResults.insert(QStringLiteral("b"), b);
    ctx.moduleResults.insert(QStringLiteral("a"), a);
    ctx.runRecords = {a, b};

    const QVector<RunRecord> records = buildRunRecords(ctx, RunRecordImagePolicy::ProductionLite);
    QCOMPARE(records.size(), 2);
    QCOMPARE(records.at(0).nodeId, QStringLiteral("a"));
    QCOMPARE(records.at(1).nodeId, QStringLiteral("b"));
}

QTEST_MAIN(TestRunRecord)
#include "test_run_record.moc"
