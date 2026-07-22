#include "vision/storage/inspectionhistory.h"
#include "vision/storage/recipe.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QtTest>

class TestRecipeHistory : public QObject
{
    Q_OBJECT

private slots:
    void recipeRoundTrip();
    void historyFiltersByStateAndBatch();
    void ngArchiveUsesDateAndRetention();
};

void TestRecipeHistory::recipeRoundTrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Selt::RecipeMetadata recipe;
    recipe.id = QStringLiteral("r1");
    recipe.name = QStringLiteral("Housing");
    recipe.tags = {QStringLiteral("production"), QStringLiteral("v1")};
    Selt::RecipeVersion version;
    version.id = QStringLiteral("r1-v2");
    version.number = 2;
    version.definition = QJsonObject{{QStringLiteral("threshold"), 42}};
    recipe.versions.append(version);
    recipe.activeVersionId = version.id;

    Selt::RecipeStore store;
    QVERIFY(store.upsert(recipe));
    const QString path = dir.filePath(QStringLiteral("recipes.json"));
    QVERIFY(store.save(path));

    Selt::RecipeStore loaded;
    QVERIFY(loaded.load(path));
    QCOMPARE(loaded.recipe(QStringLiteral("r1")).activeVersion()->number, 2);
    QCOMPARE(loaded.recipe(QStringLiteral("r1")).tags.size(), 2);
}

void TestRecipeHistory::historyFiltersByStateAndBatch()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Selt::InspectionHistoryStore history(dir.filePath(QStringLiteral("history.jsonl")));

    Selt::InspectionRecord first;
    first.id = QStringLiteral("1");
    first.batchId = QStringLiteral("batch-a");
    first.result.batchId = first.batchId;
    first.result.decisionState = QStringLiteral("ng");
    first.result.measurement.decisionState = QStringLiteral("ng");
    QVERIFY(history.append(first));

    Selt::InspectionRecord second = first;
    second.id = QStringLiteral("2");
    second.batchId = QStringLiteral("batch-b");
    second.result.batchId = second.batchId;
    second.result.decisionState = QStringLiteral("ok");
    QVERIFY(history.append(second));

    Selt::InspectionQuery query;
    query.resultState = QStringLiteral("ng");
    query.batchId = QStringLiteral("batch-a");
    const QVector<Selt::InspectionRecord> records = history.query(query);
    QCOMPARE(records.size(), 1);
    QCOMPARE(records.first().id, QStringLiteral("1"));
}

void TestRecipeHistory::ngArchiveUsesDateAndRetention()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString source = dir.filePath(QStringLiteral("source.png"));
    QFile sourceFile(source);
    QVERIFY(sourceFile.open(QIODevice::WriteOnly));
    sourceFile.write("image");
    sourceFile.close();

    Selt::InspectionRecord record;
    record.id = QStringLiteral("ng/1");
    record.batchId = QStringLiteral("batch-1");
    record.timestamp = QDateTime(QDate(2026, 7, 22), QTime(8, 0), QTimeZone::UTC);
    Selt::NgImageArchivePolicy policy(dir.filePath(QStringLiteral("ng")));
    QString archived;
    QVERIFY(policy.archive(source, record, QStringLiteral(".png"), &archived));
    QVERIFY(QFile::exists(archived));
    QVERIFY(archived.contains(QStringLiteral("2026")));
    QVERIFY(archived.contains(QStringLiteral("07")));
    QVERIFY(archived.contains(QStringLiteral("22")));

    Selt::NgImageRetentionPolicy retention;
    retention.maxAgeDays = 1;
    QCOMPARE(policy.applyRetention(retention,
                                   QDateTime(QDate(2026, 7, 24), QTime(8, 0), QTimeZone::UTC)),
             1);
    QVERIFY(!QFile::exists(archived));
}

QTEST_MAIN(TestRecipeHistory)
#include "test_recipe_history.moc"
