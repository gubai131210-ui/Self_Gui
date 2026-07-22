#include "vision/storage/projectresourcestore.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class TestProjectResourceStore : public QObject
{
    Q_OBJECT
private slots:
    void relativePathAndMissingDiagnostics();
    void jsonRoundTrip();
    void moveProjectRootResolvesRelative();
};

void TestProjectResourceStore::relativePathAndMissingDiagnostics()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    Selt::ProjectResourceStore store;
    store.setProjectRoot(temp.path());

    Selt::ProjectResource resource;
    resource.id = QStringLiteral("r1");
    resource.kind = QStringLiteral("template");
    resource.displayName = QStringLiteral("t");
    resource.relativePath = QStringLiteral("assets/templates/missing.png");
    store.addOrUpdate(resource);

    QCOMPARE(store.refreshExistence().size(), 1);
    QVERIFY(!store.missingDiagnostics().isEmpty());
}

void TestProjectResourceStore::jsonRoundTrip()
{
    Selt::ProjectResourceStore store;
    Selt::ProjectResource resource;
    resource.id = QStringLiteral("abc");
    resource.kind = QStringLiteral("template");
    resource.displayName = QStringLiteral("demo");
    resource.relativePath = QStringLiteral("assets/templates/a.png");
    store.addOrUpdate(resource);

    Selt::ProjectResourceStore loaded;
    loaded.fromJson(store.toJson());
    QVERIFY(loaded.contains(QStringLiteral("abc")));
    QCOMPARE(loaded.resource(QStringLiteral("abc")).relativePath,
             QStringLiteral("assets/templates/a.png"));
}

void TestProjectResourceStore::moveProjectRootResolvesRelative()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    QDir().mkpath(temp.filePath(QStringLiteral("assets/templates")));
    const QString abs = temp.filePath(QStringLiteral("assets/templates/t.png"));
    {
        QFile f(abs);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("png");
    }

    Selt::ProjectResourceStore store;
    store.setProjectRoot(temp.path());
    Selt::ProjectResource resource;
    resource.kind = QStringLiteral("template");
    resource.displayName = QStringLiteral("t");
    resource.relativePath = QStringLiteral("assets/templates/t.png");
    const QString id = store.addOrUpdate(resource);
    QVERIFY(store.resource(id).exists);
    QCOMPARE(QFileInfo(store.absolutePathFor(id)).fileName(), QStringLiteral("t.png"));
}

QTEST_MAIN(TestProjectResourceStore)
#include "test_project_resource_store.moc"
