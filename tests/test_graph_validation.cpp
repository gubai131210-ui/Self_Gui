#include "core/model/document.h"
#include "vision/domain/visionflow.h"
#include "vision/registry/visionnodeids.h"
#include "vision/registry/visionnoderegistry.h"
#include "vision/validation/graphvalidator.h"

#include <QtTest>

class GraphValidationTest : public QObject
{
    Q_OBJECT
private slots:
    void acceptCompatibleImageLink();
    void rejectWrongDirection();
    void rejectTypeMismatch();
    void rejectMultiInputAndSelfLoop();
    void rejectMissingBindingSource();
};

void GraphValidationTest::acceptCompatibleImageLink()
{
    Document doc;
    NodeModel a = VisionNodeRegistry::create(VisionNodeIds::imageLoader(), QPointF(0, 0));
    NodeModel b = VisionNodeRegistry::create(VisionNodeIds::grayscale(), QPointF(200, 0));
    a.id = QStringLiteral("a");
    b.id = QStringLiteral("b");
    doc.addNode(a);
    doc.addNode(b);
    QString err;
    QVERIFY(Selt::GraphValidator::canConnect(doc, a.id, QStringLiteral("out"), b.id, QStringLiteral("in"), &err));

    ConnectionModel c;
    c.id = QStringLiteral("c1");
    c.sourceNodeId = a.id;
    c.sourcePortId = QStringLiteral("out");
    c.targetNodeId = b.id;
    c.targetPortId = QStringLiteral("in");
    doc.addConnection(c);

    const auto diags = Selt::GraphValidator::validateDocument(doc);
    QVERIFY(!Selt::GraphValidator::hasErrors(diags));
}

void GraphValidationTest::rejectWrongDirection()
{
    Document doc;
    NodeModel a = VisionNodeRegistry::create(VisionNodeIds::grayscale(), QPointF(0, 0));
    NodeModel b = VisionNodeRegistry::create(VisionNodeIds::threshold(), QPointF(200, 0));
    a.id = QStringLiteral("a");
    b.id = QStringLiteral("b");
    doc.addNode(a);
    doc.addNode(b);
    QString err;
    QVERIFY(!Selt::GraphValidator::canConnect(doc, a.id, QStringLiteral("in"), b.id, QStringLiteral("out"), &err));
    QVERIFY(!err.isEmpty());
}

void GraphValidationTest::rejectTypeMismatch()
{
    Document doc;
    NodeModel measure = VisionNodeRegistry::create(VisionNodeIds::rectangleMeasure(), QPointF(0, 0));
    NodeModel gray = VisionNodeRegistry::create(VisionNodeIds::grayscale(), QPointF(200, 0));
    measure.id = QStringLiteral("m");
    gray.id = QStringLiteral("g");
    doc.addNode(measure);
    doc.addNode(gray);
    QString err;
    QVERIFY(!Selt::GraphValidator::canConnect(doc, measure.id, QStringLiteral("width"),
                                              gray.id, QStringLiteral("in"), &err));
    QVERIFY(err.contains(QStringLiteral("不兼容")));
}

void GraphValidationTest::rejectMultiInputAndSelfLoop()
{
    Document doc;
    NodeModel a = VisionNodeRegistry::create(VisionNodeIds::imageLoader(), QPointF(0, 0));
    NodeModel b = VisionNodeRegistry::create(VisionNodeIds::grayscale(), QPointF(200, 0));
    a.id = QStringLiteral("a");
    b.id = QStringLiteral("b");
    doc.addNode(a);
    doc.addNode(b);

    ConnectionModel c1;
    c1.id = QStringLiteral("c1");
    c1.sourceNodeId = a.id;
    c1.sourcePortId = QStringLiteral("out");
    c1.targetNodeId = b.id;
    c1.targetPortId = QStringLiteral("in");
    doc.addConnection(c1);

    QString err;
    QVERIFY(!Selt::GraphValidator::canConnect(doc, a.id, QStringLiteral("out"), b.id, QStringLiteral("in"), &err));

    NodeModel g = VisionNodeRegistry::create(VisionNodeIds::grayscale(), {});
    g.id = QStringLiteral("g");
    Document d2;
    d2.addNode(g);
    ConnectionModel self;
    self.id = QStringLiteral("s");
    self.sourceNodeId = g.id;
    self.sourcePortId = QStringLiteral("out");
    self.targetNodeId = g.id;
    self.targetPortId = QStringLiteral("in");
    d2.addConnection(self);
    const auto diags = Selt::GraphValidator::validateDocument(d2);
    QVERIFY(Selt::GraphValidator::hasErrors(diags));
    bool foundSelf = false;
    for (const auto &d : diags) {
        if (d.code == QLatin1String("self_loop"))
            foundSelf = true;
    }
    QVERIFY(foundSelf);
}

void GraphValidationTest::rejectMissingBindingSource()
{
    NodeModel node = VisionNodeRegistry::create(VisionNodeIds::threshold(), {});
    node.id = QStringLiteral("t1");
    Selt::ParameterBinding binding =
        Selt::ParameterBinding::upstream(QStringLiteral("missing"), QStringLiteral("out"),
                                         Selt::DataTypeId::Int);
    node.parameterBindings.insert(QStringLiteral("threshold"), binding.toJson());

    Document doc;
    doc.addNode(node);
    const auto diags = Selt::GraphValidator::validateDocument(doc);
    QVERIFY(Selt::GraphValidator::hasErrors(diags));
    bool found = false;
    for (const auto &d : diags) {
        if (d.code == QLatin1String("binding_missing_node") && d.parameterKey == QLatin1String("threshold"))
            found = true;
    }
    QVERIFY(found);
}

QTEST_MAIN(GraphValidationTest)
#include "test_graph_validation.moc"
