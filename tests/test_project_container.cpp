#include "vision/domain/visionproject.h"
#include "vision/storage/projectcontainer.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

class TestProjectContainer : public QObject
{
    Q_OBJECT

private slots:
    void roundTripManifestAndResources();
};

void TestProjectContainer::roundTripManifestAndResources()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString sourceAsset = QDir(temp.path()).filePath(QStringLiteral("source.png"));
    QFile asset(sourceAsset);
    QVERIFY(asset.open(QIODevice::WriteOnly));
    QVERIFY(asset.write("asset") > 0);
    asset.close();

    VisionProject project;
    project.setTitle(QStringLiteral("容器测试"));
    Selt::ProjectResource resource;
    resource.id = QStringLiteral("template-1");
    resource.kind = QStringLiteral("template");
    resource.displayName = QStringLiteral("模板");
    resource.relativePath = QStringLiteral("source.png");
    project.resources().setProjectRoot(temp.path());
    project.resources().addOrUpdate(resource);

    const QString container = QDir(temp.path()).filePath(QStringLiteral("demo.seltproject"));
    QString error;
    QVERIFY2(Selt::ProjectContainerStorage::save(container, project, &error),
             qPrintable(error));
    QVERIFY(QFileInfo::exists(QDir(container).filePath(QStringLiteral("manifest.json"))));

    VisionProject loaded;
    Selt::ProjectContainerReport report;
    QVERIFY2(Selt::ProjectContainerStorage::load(container, loaded, &report),
             qPrintable(report.error));
    QCOMPARE(loaded.title(), QStringLiteral("容器测试"));
    QVERIFY(QFileInfo::exists(QDir(container).filePath(QStringLiteral("source.png"))));
    QVERIFY(!loaded.resources().resources().isEmpty());
}

QTEST_MAIN(TestProjectContainer)
#include "test_project_container.moc"
