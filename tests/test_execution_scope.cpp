#include "core/model/document.h"
#include "vision/registry/visionnodeids.h"
#include "vision/registry/visionnoderegistry.h"
#include "vision/runtime/executionscope.h"
#include "vision/runtime/flowexecutionsnapshot.h"

#include <QTest>

class TestExecutionScope : public QObject
{
    Q_OBJECT

private slots:
    void upstreamClosureIncludesDependencies();
    void snapshotDoesNotChangeAfterDocumentEdit();
};

void TestExecutionScope::upstreamClosureIncludesDependencies()
{
    Document document;
    NodeModel input = VisionNodeRegistry::create(VisionNodeIds::imageLoader());
    NodeModel gray = VisionNodeRegistry::create(VisionNodeIds::grayscale());
    NodeModel threshold = VisionNodeRegistry::create(VisionNodeIds::threshold());
    input.id = QStringLiteral("input");
    gray.id = QStringLiteral("gray");
    threshold.id = QStringLiteral("threshold");
    document.addNode(input);
    document.addNode(gray);
    document.addNode(threshold);

    ConnectionModel first;
    first.id = QStringLiteral("first");
    first.sourceNodeId = input.id;
    first.sourcePortId = QStringLiteral("out");
    first.targetNodeId = gray.id;
    first.targetPortId = QStringLiteral("in");
    document.addConnection(first);
    ConnectionModel second;
    second.id = QStringLiteral("second");
    second.sourceNodeId = gray.id;
    second.sourcePortId = QStringLiteral("out");
    second.targetNodeId = threshold.id;
    second.targetPortId = QStringLiteral("in");
    document.addConnection(second);

    const Selt::ExecutionScope scope = Selt::ExecutionScope::withUpstream(threshold.id);
    const QSet<QString> ids = scope.resolveNodeIds(document);
    QCOMPARE(ids.size(), 3);
    QVERIFY(ids.contains(input.id));
    QVERIFY(ids.contains(gray.id));
    QVERIFY(ids.contains(threshold.id));
}

void TestExecutionScope::snapshotDoesNotChangeAfterDocumentEdit()
{
    Document document;
    NodeModel node = VisionNodeRegistry::create(VisionNodeIds::grayscale());
    node.id = QStringLiteral("gray");
    const QString originalText = node.text;
    document.addNode(node);
    const Selt::FlowExecutionSnapshot snapshot =
        Selt::FlowExecutionSnapshot::create(
            document, Selt::ExecutionScope::fullFlow());
    node.text = QStringLiteral("changed");
    document.updateNode(node);
    QCOMPARE(snapshot.document().node(node.id).text, originalText);
    QCOMPARE(document.node(node.id).text, QStringLiteral("changed"));
}

QTEST_MAIN(TestExecutionScope)
#include "test_execution_scope.moc"
