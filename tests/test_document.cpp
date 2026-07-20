#include "core/command/documentcommands.h"
#include "core/model/document.h"
#include "core/registry/nodefactory.h"
#include "core/serialization/documentserializer.h"

#include <QCoreApplication>
#include <QTemporaryDir>
#include <QTest>
#include <QUndoStack>

class TestDocument : public QObject
{
    Q_OBJECT

private slots:
    void createAndAddNode();
    void uniqueIds();
    void connectionValidation();
    void removeNodeRemovesConnections();
    void jsonRoundTrip();
    void jsonRejectsBadFormat();
    void undoRedoAddDelete();
    void undoMove();
};

void TestDocument::createAndAddNode()
{
    Document doc;
    NodeModel node = NodeFactory::create(QStringLiteral("rectangle"), QPointF(10, 20));
    const QString id = doc.addNode(node);
    QVERIFY(!id.isEmpty());
    QCOMPARE(doc.nodes().size(), 1);
    QCOMPARE(doc.node(id).position, QPointF(10, 20));
}

void TestDocument::uniqueIds()
{
    Document doc;
    NodeModel a = NodeFactory::create(QStringLiteral("ellipse"));
    NodeModel b = a;
    QVERIFY(!doc.addNode(a).isEmpty());
    QVERIFY(doc.addNode(b).isEmpty());
    QCOMPARE(doc.nodes().size(), 1);
}

void TestDocument::connectionValidation()
{
    Document doc;
    NodeModel a = NodeFactory::create(QStringLiteral("rectangle"));
    NodeModel b = NodeFactory::create(QStringLiteral("diamond"));
    doc.addNode(a);
    doc.addNode(b);

    ConnectionModel bad;
    bad.sourceNodeId = QStringLiteral("missing");
    bad.targetNodeId = b.id;
    QVERIFY(doc.addConnection(bad).isEmpty());

    ConnectionModel good;
    good.sourceNodeId = a.id;
    good.sourcePortId = QStringLiteral("out");
    good.targetNodeId = b.id;
    good.targetPortId = QStringLiteral("in");
    QVERIFY(!doc.addConnection(good).isEmpty());
    QCOMPARE(doc.connections().size(), 1);
}

void TestDocument::removeNodeRemovesConnections()
{
    Document doc;
    NodeModel a = NodeFactory::create(QStringLiteral("rectangle"));
    NodeModel b = NodeFactory::create(QStringLiteral("rectangle"));
    doc.addNode(a);
    doc.addNode(b);

    ConnectionModel c;
    c.sourceNodeId = a.id;
    c.sourcePortId = QStringLiteral("out");
    c.targetNodeId = b.id;
    c.targetPortId = QStringLiteral("in");
    doc.addConnection(c);

    QVERIFY(doc.removeNode(a.id));
    QCOMPARE(doc.connections().size(), 0);
    QCOMPARE(doc.nodes().size(), 1);
}

void TestDocument::jsonRoundTrip()
{
    Document doc;
    doc.setTitle(QStringLiteral("中文标题测试"));
    NodeModel a = NodeFactory::create(QStringLiteral("roundedRectangle"), QPointF(5, 6));
    a.text = QStringLiteral("你好世界");
    NodeModel b = NodeFactory::create(QStringLiteral("text"), QPointF(100, 80));
    doc.addNode(a);
    doc.addNode(b);

    ConnectionModel c;
    c.sourceNodeId = a.id;
    c.sourcePortId = QStringLiteral("out");
    c.targetNodeId = b.id;
    c.targetPortId = QStringLiteral("in");
    c.label = QStringLiteral("关联");
    doc.addConnection(c);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("demo.selt"));
    QString error;
    QVERIFY(DocumentSerializer::saveToFile(doc, path, &error));

    Document loaded;
    QVERIFY(DocumentSerializer::loadFromFile(loaded, path, &error));
    QCOMPARE(loaded.title(), QStringLiteral("中文标题测试"));
    QCOMPARE(loaded.nodes().size(), 2);
    QCOMPARE(loaded.connections().size(), 1);
    QCOMPARE(loaded.node(a.id).text, QStringLiteral("你好世界"));
    QCOMPARE(loaded.connection(c.id).label, QStringLiteral("关联"));
}

void TestDocument::jsonRejectsBadFormat()
{
    Document doc;
    QString error;
    QVERIFY(!DocumentSerializer::fromJson(doc, QByteArrayLiteral("{\"format\":\"nope\",\"version\":1}"), &error));
    QVERIFY(!error.isEmpty());
}

void TestDocument::undoRedoAddDelete()
{
    Document doc;
    QUndoStack stack;
    NodeModel node = NodeFactory::create(QStringLiteral("rectangle"));
    stack.push(new AddNodeCommand(&doc, node));
    QCOMPARE(doc.nodes().size(), 1);

    stack.push(new DeleteNodesCommand(&doc, QStringList{node.id}));
    QCOMPARE(doc.nodes().size(), 0);

    stack.undo();
    QCOMPARE(doc.nodes().size(), 1);
    QVERIFY(doc.hasNode(node.id));

    stack.redo();
    QCOMPARE(doc.nodes().size(), 0);
}

void TestDocument::undoMove()
{
    Document doc;
    QUndoStack stack;
    NodeModel node = NodeFactory::create(QStringLiteral("rectangle"), QPointF(0, 0));
    doc.addNode(node);

    QVector<MoveNodesCommand::Item> items;
    MoveNodesCommand::Item item;
    item.id = node.id;
    item.oldPos = QPointF(0, 0);
    item.newPos = QPointF(40, 50);
    items.append(item);
    stack.push(new MoveNodesCommand(&doc, items));
    QCOMPARE(doc.node(node.id).position, QPointF(40, 50));
    stack.undo();
    QCOMPARE(doc.node(node.id).position, QPointF(0, 0));
}

QTEST_MAIN(TestDocument)
#include "test_document.moc"
