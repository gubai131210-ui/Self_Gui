#include "core/model/document.h"
#include "vision/execution/visionpipeline.h"
#include "vision/model/moduleparamdef.h"
#include "vision/model/overlayitems.h"
#include "vision/model/roi.h"
#include "vision/registry/visionnodeids.h"
#include "vision/registry/visionnoderegistry.h"

#include <QCoreApplication>
#include <QDir>
#include <QImage>
#include <QTemporaryDir>
#include <QTest>
#include <opencv2/imgproc.hpp>

class TestVisionIntegration : public QObject
{
    Q_OBJECT

private slots:
    void registryProvidesVisionCategories();
    void fixedNodeSizeIsUnified();
    void parameterValidationRejectsOutOfRange();
    void roiRoundTripPreservesRect();
    void overlayRoundTripPreservesType();
    void topologyOrdersDependencies();
    void onceExecutionProducesMeasurement();
};

void TestVisionIntegration::registryProvidesVisionCategories()
{
    const QStringList cats = VisionNodeRegistry::categories();
    QVERIFY(cats.contains(QStringLiteral("输入")));
    QVERIFY(cats.contains(QStringLiteral("预处理")));
    QVERIFY(cats.contains(QStringLiteral("测量")));
    QVERIFY(VisionNodeRegistry::supportedTypes().contains(VisionNodeIds::imageLoader()));
    QVERIFY(VisionNodeRegistry::supportedTypes().contains(VisionNodeIds::threshold()));
}

void TestVisionIntegration::fixedNodeSizeIsUnified()
{
    const QSizeF size = VisionNodeRegistry::fixedNodeSize();
    QCOMPARE(size, QSizeF(160, 72));
    for (const ModuleDescriptor &desc : VisionNodeRegistry::allDescriptors())
        QCOMPARE(desc.fixedSize, size);
}

void TestVisionIntegration::parameterValidationRejectsOutOfRange()
{
    QJsonObject params = VisionNodeRegistry::defaultParameters(VisionNodeIds::threshold());
    params.insert(QStringLiteral("threshold"), 999);
    QString error;
    QVERIFY(!VisionNodeRegistry::validateParameters(VisionNodeIds::threshold(), params, &error));
    QVERIFY(!error.isEmpty());

    params.insert(QStringLiteral("threshold"), 128);
    QVERIFY(VisionNodeRegistry::validateParameters(VisionNodeIds::threshold(), params, &error));
}

void TestVisionIntegration::roiRoundTripPreservesRect()
{
    RoiRect roi;
    roi.enabled = true;
    roi.locked = false;
    roi.rect = QRectF(10, 20, 30, 40);
    const RoiRect restored = RoiRect::fromJson(roi.toJson());
    QCOMPARE(restored.enabled, true);
    QCOMPARE(restored.rect, QRectF(10, 20, 30, 40));
    QVERIFY(restored.isValid());
}

void TestVisionIntegration::overlayRoundTripPreservesType()
{
    OverlayItem item;
    item.type = OverlayItemType::Rectangle;
    item.rect = QRectF(1, 2, 3, 4);
    item.text = QStringLiteral("demo");
    item.color = QColor(255, 0, 0);
    const OverlayItem restored = OverlayItem::fromJson(item.toJson());
    QCOMPARE(restored.type, OverlayItemType::Rectangle);
    QCOMPARE(restored.rect, QRectF(1, 2, 3, 4));
    QCOMPARE(restored.text, QStringLiteral("demo"));
}

void TestVisionIntegration::topologyOrdersDependencies()
{
    Document doc;
    NodeModel a = VisionNodeRegistry::create(VisionNodeIds::imageLoader(), QPointF(0, 0));
    NodeModel b = VisionNodeRegistry::create(VisionNodeIds::grayscale(), QPointF(200, 0));
    NodeModel c = VisionNodeRegistry::create(VisionNodeIds::threshold(), QPointF(400, 0));
    a.id = QStringLiteral("a");
    b.id = QStringLiteral("b");
    c.id = QStringLiteral("c");
    doc.addNode(a);
    doc.addNode(b);
    doc.addNode(c);

    ConnectionModel c1;
    c1.id = QStringLiteral("c1");
    c1.sourceNodeId = a.id;
    c1.sourcePortId = QStringLiteral("out");
    c1.targetNodeId = b.id;
    c1.targetPortId = QStringLiteral("in");
    doc.addConnection(c1);

    ConnectionModel c2 = c1;
    c2.id = QStringLiteral("c2");
    c2.sourceNodeId = b.id;
    c2.targetNodeId = c.id;
    doc.addConnection(c2);

    const QStringList order = VisionPipeline::buildExecutionOrder(doc);
    QCOMPARE(order, (QStringList{QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")}));
}

void TestVisionIntegration::onceExecutionProducesMeasurement()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString imagePath = temp.filePath(QStringLiteral("rect.png"));

    QImage image(80, 80, QImage::Format_RGB888);
    image.fill(Qt::black);
    for (int y = 20; y < 50; ++y) {
        for (int x = 20; x < 60; ++x)
            image.setPixel(x, y, qRgb(255, 255, 255));
    }
    QVERIFY(image.save(imagePath));

    Document doc;
    NodeModel loader = VisionNodeRegistry::create(VisionNodeIds::imageLoader(), QPointF(0, 0));
    NodeModel gray = VisionNodeRegistry::create(VisionNodeIds::grayscale(), QPointF(200, 0));
    NodeModel threshold = VisionNodeRegistry::create(VisionNodeIds::threshold(), QPointF(400, 0));
    NodeModel measure = VisionNodeRegistry::create(VisionNodeIds::rectangleMeasure(), QPointF(600, 0));
    loader.id = QStringLiteral("loader");
    gray.id = QStringLiteral("gray");
    threshold.id = QStringLiteral("threshold");
    measure.id = QStringLiteral("measure");
    loader.parameters.insert(QStringLiteral("path"), imagePath);
    loader.imagePath = imagePath;
    doc.addNode(loader);
    doc.addNode(gray);
    doc.addNode(threshold);
    doc.addNode(measure);

    auto connectNodes = [&](const QString &from, const QString &to, const QString &id) {
        ConnectionModel c;
        c.id = id;
        c.sourceNodeId = from;
        c.sourcePortId = QStringLiteral("out");
        c.targetNodeId = to;
        c.targetPortId = QStringLiteral("in");
        doc.addConnection(c);
    };
    connectNodes(loader.id, gray.id, QStringLiteral("c1"));
    connectNodes(gray.id, threshold.id, QStringLiteral("c2"));
    connectNodes(threshold.id, measure.id, QStringLiteral("c3"));

    VisionContext context;
    QVERIFY(VisionPipeline::execute(doc, context, ExecutionMode::Once));
    QVERIFY(!context.hasError());
    QVERIFY(context.measurement.valid);
    QVERIFY(context.moduleResults.contains(measure.id));
    QCOMPARE(context.moduleResult(measure.id).status, ModuleStatus::Success);
    QVERIFY(!context.moduleResult(measure.id).overlays.isEmpty());
    QCOMPARE(context.mode, ExecutionMode::Once);
}

QTEST_MAIN(TestVisionIntegration)
#include "test_vision_integration.moc"
