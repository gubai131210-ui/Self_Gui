#include "vision/storage/projectresourcestore.h"

#include <QTemporaryDir>
#include <QtTest>

class TestResourceMigration : public QObject
{
    Q_OBJECT

private slots:
    void normalizesAbsolutePathIntoRelative();
    void rejectsEscapingRelativePath();
};

void TestResourceMigration::normalizesAbsolutePathIntoRelative()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    Selt::ProjectResourceStore store;
    store.setProjectRoot(temp.path());

    Selt::ProjectResource resource;
    resource.id = QStringLiteral("r1");
    resource.kind = QStringLiteral("template");
    resource.relativePath = QDir(temp.path()).filePath(QStringLiteral("assets/templates/a.png"));
    store.addOrUpdate(resource);

    QCOMPARE(store.resource(QStringLiteral("r1")).relativePath,
             QStringLiteral("assets/templates/a.png"));
}

void TestResourceMigration::rejectsEscapingRelativePath()
{
    Selt::ProjectResourceStore store;
    store.setProjectRoot(QStringLiteral("D:/project"));
    Selt::ProjectResource resource;
    resource.id = QStringLiteral("r1");
    resource.kind = QStringLiteral("template");
    resource.relativePath = QStringLiteral("../outside.png");
    store.addOrUpdate(resource);

    const QVector<Selt::ResourceDiagnostic> diagnostics = store.diagnostics();
    QVERIFY(!diagnostics.isEmpty());
    QCOMPARE(diagnostics.first().code, QStringLiteral("resource_path_invalid"));
}

QTEST_MAIN(TestResourceMigration)
#include "test_resource_migration.moc"
