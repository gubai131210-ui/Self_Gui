#include "core/model/document.h"
#include "core/model/nodemodel.h"
#include "core/registry/nodefactory.h"
#include "ui/theme/operatorpreferences.h"
#include "vision/validation/graphvalidator.h"

#include <QCoreApplication>
#include <QSettings>
#include <QTest>

class TestEditorRoadmap : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void breakpointSerializes();
    void publishValidationReportsBreakpointAndDisabled();
    void canConnectAllowsReplaceWhenRequested();
    void operatorFavoritesRoundTrip();
};

void TestEditorRoadmap::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("SeltTest"));
    QCoreApplication::setApplicationName(QStringLiteral("EditorRoadmap"));
    QSettings().clear();
}

void TestEditorRoadmap::breakpointSerializes()
{
    NodeModel node = NodeFactory::create(QStringLiteral("rectangle"));
    QVERIFY(!node.breakpoint);
    node.breakpoint = true;
    const NodeModel restored = NodeModel::fromJson(node.toJson());
    QVERIFY(restored.breakpoint);
    QJsonObject legacy = node.toJson();
    legacy.remove(QStringLiteral("breakpoint"));
    QCOMPARE(NodeModel::fromJson(legacy).breakpoint, false);
}

void TestEditorRoadmap::publishValidationReportsBreakpointAndDisabled()
{
    Document doc;
    NodeModel a = NodeFactory::create(QStringLiteral("rectangle"));
    a.text = QStringLiteral("A");
    a.breakpoint = true;
    NodeModel b = NodeFactory::create(QStringLiteral("rectangle"));
    b.text = QStringLiteral("B");
    b.enabled = false;
    QVERIFY(!doc.addNode(a).isEmpty());
    QVERIFY(!doc.addNode(b).isEmpty());

    const QVector<Selt::GraphDiagnostic> diags =
        Selt::GraphValidator::validateForPublish(doc, {QStringLiteral("missing:template/x.png")});
    bool sawBreakpoint = false;
    bool sawDisabled = false;
    bool sawResource = false;
    for (const Selt::GraphDiagnostic &d : diags) {
        if (d.code == QLatin1String("breakpoint_set"))
            sawBreakpoint = true;
        if (d.code == QLatin1String("node_disabled"))
            sawDisabled = true;
        if (d.code == QLatin1String("resource_missing"))
            sawResource = true;
    }
    QVERIFY(sawBreakpoint);
    QVERIFY(sawDisabled);
    QVERIFY(sawResource);
    QVERIFY(Selt::GraphValidator::hasErrors(diags));
}

void TestEditorRoadmap::canConnectAllowsReplaceWhenRequested()
{
    Document doc;
    NodeModel src = NodeFactory::create(QStringLiteral("rectangle"));
    NodeModel mid = NodeFactory::create(QStringLiteral("rectangle"));
    NodeModel dst = NodeFactory::create(QStringLiteral("rectangle"));
    QVERIFY(!doc.addNode(src).isEmpty());
    QVERIFY(!doc.addNode(mid).isEmpty());
    QVERIFY(!doc.addNode(dst).isEmpty());

    ConnectionModel existing;
    existing.id = Document::createId(QStringLiteral("c"));
    existing.sourceNodeId = mid.id;
    existing.sourcePortId = QStringLiteral("out");
    existing.targetNodeId = dst.id;
    existing.targetPortId = QStringLiteral("in");
    QVERIFY(!doc.addConnection(existing).isEmpty());

    QString error;
    QVERIFY(!Selt::GraphValidator::canConnect(doc, src.id, QStringLiteral("out"),
                                              dst.id, QStringLiteral("in"), &error, false));
    QVERIFY(error.contains(QStringLiteral("已有连接")));
    QVERIFY(Selt::GraphValidator::canConnect(doc, src.id, QStringLiteral("out"),
                                             dst.id, QStringLiteral("in"), &error, true));
}

void TestEditorRoadmap::operatorFavoritesRoundTrip()
{
    Selt::OperatorPreferences::setFavorites({});
    QCOMPARE(Selt::OperatorPreferences::favorites().size(), 0);
    Selt::OperatorPreferences::toggleFavorite(QStringLiteral("vision.grayscale"));
    QVERIFY(Selt::OperatorPreferences::isFavorite(QStringLiteral("vision.grayscale")));
    Selt::OperatorPreferences::pushRecent(QStringLiteral("vision.canny"));
    Selt::OperatorPreferences::pushRecent(QStringLiteral("vision.blur"));
    QCOMPARE(Selt::OperatorPreferences::recent().first(), QStringLiteral("vision.blur"));
    Selt::OperatorPreferences::toggleFavorite(QStringLiteral("vision.grayscale"));
    QVERIFY(!Selt::OperatorPreferences::isFavorite(QStringLiteral("vision.grayscale")));
}

QTEST_MAIN(TestEditorRoadmap)
#include "test_editor_roadmap.moc"
