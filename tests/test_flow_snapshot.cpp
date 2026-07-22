#include "core/model/document.h"
#include "vision/registry/visionnoderegistry.h"
#include "vision/runtime/flowexecutionsnapshot.h"

#include <QtTest>

class TestFlowSnapshot : public QObject
{
    Q_OBJECT

private slots:
    void capturesTemplateResourceReference();
    void capturesExternalPluginDependency();
};

void TestFlowSnapshot::capturesTemplateResourceReference()
{
    Document document;
    NodeModel node = VisionNodeRegistry::create(QStringLiteral("vision.templateMatch"));
    node.id = QStringLiteral("match");
    node.parameters.insert(QStringLiteral("templateResourceId"), QStringLiteral("tpl-1"));
    document.addNode(node);

    const Selt::FlowExecutionSnapshot snapshot =
        Selt::FlowExecutionSnapshot::create(document, Selt::ExecutionScope::fullFlow());
    QCOMPARE(snapshot.resourceIds(), QStringList{QStringLiteral("tpl-1")});
    QVERIFY(!snapshot.snapshotId().isEmpty());
}

void TestFlowSnapshot::capturesExternalPluginDependency()
{
    Document document;
    NodeModel node;
    node.id = QStringLiteral("plugin-node");
    node.type = QStringLiteral("plugin.test");
    node.text = QStringLiteral("Plugin");
    document.addNode(node);
    VisionNodeRegistry::setPluginIdForType(node.type, QStringLiteral("plugin.demo"));

    const Selt::FlowExecutionSnapshot snapshot =
        Selt::FlowExecutionSnapshot::create(document, Selt::ExecutionScope::fullFlow());
    QVERIFY(!snapshot.pluginDependencies().isEmpty());
}

QTEST_MAIN(TestFlowSnapshot)
#include "test_flow_snapshot.moc"
