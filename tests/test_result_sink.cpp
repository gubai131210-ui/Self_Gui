#include "vision/runtime/resultsink.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class TestResultSink : public QObject
{
    Q_OBJECT

private slots:
    void csvAndJsonPublish();
    void publishFailureIsIsolated();
};

void TestResultSink::csvAndJsonPublish()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString csvPath = dir.filePath(QStringLiteral("out.csv"));
    const QString jsonPath = dir.filePath(QStringLiteral("out.jsonl"));

    Selt::ResultSinkRegistry::instance().clear();
    Selt::ResultSinkRegistry::instance().addSink(std::make_shared<Selt::CsvResultSink>(csvPath));
    Selt::ResultSinkRegistry::instance().addSink(std::make_shared<Selt::JsonLinesResultSink>(jsonPath));

    Selt::ResultRecord record;
    record.batchId = QStringLiteral("b1");
    record.frameId = QStringLiteral("1");
    record.nodeId = QStringLiteral("n1");
    record.decisionState = QStringLiteral("ok");
    record.measurement.valid = true;
    record.measurement.width = 12.5;
    record.measurement.unit = QStringLiteral("mm");

    const QStringList errors = Selt::ResultSinkRegistry::instance().publishAll(record);
    QVERIFY(errors.isEmpty());
    QVERIFY(QFile::exists(csvPath));
    QVERIFY(QFile::exists(jsonPath));
    QVERIFY(QFile(csvPath).size() > 0);
    QVERIFY(QFile(jsonPath).size() > 0);
}

void TestResultSink::publishFailureIsIsolated()
{
    Selt::ResultSinkRegistry::instance().clear();
    Selt::ResultSinkRegistry::instance().addSink(
        std::make_shared<Selt::CsvResultSink>(QStringLiteral("Z:/unlikely/path/out.csv")));
    Selt::ResultSinkRegistry::instance().addSink(
        std::make_shared<Selt::JsonLinesResultSink>(
            QDir::temp().filePath(QStringLiteral("selt_result_ok.jsonl"))));

    Selt::ResultRecord record;
    record.measurement.valid = true;
    record.measurement.message = QStringLiteral("ok");
    const QStringList errors = Selt::ResultSinkRegistry::instance().publishAll(record);
    QVERIFY(!errors.isEmpty());
    QCOMPARE(errors.size(), 1);
}

QTEST_MAIN(TestResultSink)
#include "test_result_sink.moc"
