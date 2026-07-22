#include "vision/nodes/builtinexecutors.h"
#include "vision/execution/visionpipeline.h"
#include "vision/runtime/flowruntime.h"
#include "vision/runtime/nodeexecutorregistry.h"
#include "vision/model/calibrationmodel.h"
#include "core/model/document.h"
#include "vision/model/moduleparamdef.h"
#include "vision/model/overlayitems.h"
#include "vision/model/roi.h"
#include "vision/registry/visionnodeids.h"
#include "vision/registry/visionnoderegistry.h"
#include "vision/domain/projectvariables.h"

#include <QCoreApplication>
#include <QDir>
#include <QImage>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>
#include <algorithm>
#include <vector>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

static bool imwriteUnicode(const QString &path, const cv::Mat &mat)
{
    if (path.trimmed().isEmpty() || mat.empty())
        return false;
    const QFileInfo fi(path);
    QString ext = fi.suffix().trimmed().toLower();
    if (ext.isEmpty())
        ext = QStringLiteral("png");
    std::vector<uchar> buffer;
    bool encoded = false;
    try {
        // OpenCV may accept both "png" and ".png" depending on build.
        // Try multiple fallbacks to keep tests stable across environments.
        const std::string fmt = ext.toStdString();
        if (cv::imencode(fmt, mat, buffer)) {
            encoded = true;
        } else if (cv::imencode((std::string(".") + fmt), mat, buffer)) {
            encoded = true;
        } else if (mat.channels() == 1) {
            cv::Mat bgr;
            cv::cvtColor(mat, bgr, cv::COLOR_GRAY2BGR);
            if (cv::imencode(fmt, bgr, buffer))
                encoded = true;
        }
    } catch (const cv::Exception &) {
        encoded = false;
    } catch (const std::exception &) {
        encoded = false;
    }

    if (encoded) {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly))
            return false;
        return file.write(reinterpret_cast<const char *>(buffer.data()),
                           static_cast<qint64>(buffer.size()))
                   == static_cast<qint64>(buffer.size());
    }

    // Qt fallback: unicode path support is better.
    QImage qimg;
    if (mat.channels() == 1) {
        qimg = QImage(mat.data, mat.cols, mat.rows, static_cast<int>(mat.step),
                      QImage::Format_Grayscale8)
                   .copy();
    } else if (mat.channels() == 3) {
        cv::Mat rgb;
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
        qimg = QImage(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step),
                       QImage::Format_RGB888)
                   .copy();
    } else if (mat.channels() == 4) {
        cv::Mat bgra;
        cv::cvtColor(mat, bgra, cv::COLOR_BGRA2RGBA);
        qimg = QImage(bgra.data, bgra.cols, bgra.rows, static_cast<int>(bgra.step),
                       QImage::Format_RGBA8888)
                   .copy();
    } else {
        return false;
    }
    return qimg.save(path);
}

class TestVisionIntegration : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void registryProvidesVisionCategories();
    void fixedNodeSizeIsUnified();
    void parameterValidationRejectsOutOfRange();
    void roiRoundTripPreservesRect();
    void overlayRoundTripPreservesType();
    void topologyOrdersDependencies();
    void onceExecutionProducesMeasurement();
    void allBuiltinNodesHaveExecutor();
    void templateMatchPipelineSucceeds();
    void templateSourceToMatchPipelineSucceeds();
    void templateMatchInputPortsAreSeparated();
    void legacyTemplateMatchPortsAreUpgraded();
    void miswiredTemplateMatchImageConnectionStillRuns();
    void preprocessPipelineSucceeds();
    void regionBlobPipelineSucceeds();
    void logicCombineDecisionSucceeds();
    void circleMeasureTolerancePipelineSucceeds();
    void findContoursFitCirclePipelineSucceeds();
    void calibratedMeasureDecisionPipeline();
};

void TestVisionIntegration::initTestCase()
{
    Selt::registerBuiltInOpenCvExecutors();
}

void TestVisionIntegration::registryProvidesVisionCategories()
{
    const QStringList cats = VisionNodeRegistry::categories();
    QVERIFY(cats.contains(QStringLiteral("输入")));
    QVERIFY(cats.contains(QStringLiteral("预处理")));
    QVERIFY(cats.contains(QStringLiteral("区域")));
    QVERIFY(cats.contains(QStringLiteral("定位")));
    QVERIFY(cats.contains(QStringLiteral("测量")));
    QVERIFY(cats.contains(QStringLiteral("逻辑")));
    QVERIFY(cats.contains(QStringLiteral("输出")));
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

void TestVisionIntegration::allBuiltinNodesHaveExecutor()
{
    for (const QString &type : VisionNodeRegistry::supportedTypes()) {
        QVERIFY2(Selt::NodeExecutorRegistry::instance().contains(type), qPrintable(type));
        QVERIFY2(Selt::NodeExecutorRegistry::instance().create(type), qPrintable(type));
        const ModuleDescriptor d = VisionNodeRegistry::descriptor(type);
        QVERIFY2(!d.typeId.isEmpty(), qPrintable(type));
        QVERIFY2(!d.displayName.isEmpty(), qPrintable(type));
    }
    QVERIFY(VisionNodeRegistry::supportedTypes().contains(VisionNodeIds::templateMatch()));
}

void TestVisionIntegration::templateMatchPipelineSucceeds()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString imagePath = temp.filePath(QStringLiteral("scene.png"));
    const QString templPath = temp.filePath(QStringLiteral("templ.png"));

    cv::Mat scene(64, 64, CV_8UC1, cv::Scalar(0));
    cv::rectangle(scene, cv::Rect(20, 18, 16, 12), cv::Scalar(255), cv::FILLED);
    cv::Mat templ = scene(cv::Rect(20, 18, 16, 12)).clone();
    QVERIFY(imwriteUnicode(imagePath, scene));
    QVERIFY(imwriteUnicode(templPath, templ));

    Document doc;
    NodeModel loader = VisionNodeRegistry::create(VisionNodeIds::imageLoader(), {});
    NodeModel match = VisionNodeRegistry::create(VisionNodeIds::templateMatch(), QPointF(220, 0));
    loader.id = QStringLiteral("loader");
    match.id = QStringLiteral("match");
    loader.parameters.insert(QStringLiteral("path"), imagePath);
    match.parameters.insert(QStringLiteral("templatePath"), templPath);
    match.parameters.insert(QStringLiteral("minScore"), 0.8);
    doc.addNode(loader);
    doc.addNode(match);

    ConnectionModel c;
    c.id = QStringLiteral("c1");
    c.sourceNodeId = loader.id;
    c.sourcePortId = QStringLiteral("out");
    c.targetNodeId = match.id;
    c.targetPortId = QStringLiteral("in");
    doc.addConnection(c);

    VisionContext ctx;
    Selt::ProjectVariableStore vars;
    QVERIFY(Selt::FlowRuntime::executeOnce(doc, ctx, vars, nullptr));
    QVERIFY(!ctx.hasError());
    QCOMPARE(ctx.moduleResult(match.id).status, ModuleStatus::Success);
    QVERIFY(!ctx.moduleResult(match.id).overlays.isEmpty());
}

void TestVisionIntegration::templateSourceToMatchPipelineSucceeds()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString imagePath = temp.filePath(QStringLiteral("scene.png"));
    const QString templPath = temp.filePath(QStringLiteral("templ.png"));

    cv::Mat scene(64, 64, CV_8UC1, cv::Scalar(0));
    cv::rectangle(scene, cv::Rect(20, 18, 16, 12), cv::Scalar(255), cv::FILLED);
    cv::Mat templ = scene(cv::Rect(20, 18, 16, 12)).clone();
    QVERIFY(imwriteUnicode(imagePath, scene));
    QVERIFY(imwriteUnicode(templPath, templ));

    Document doc;
    NodeModel loader = VisionNodeRegistry::create(VisionNodeIds::imageLoader(), {});
    NodeModel source = VisionNodeRegistry::create(VisionNodeIds::templateSource(), QPointF(0, 120));
    NodeModel match = VisionNodeRegistry::create(VisionNodeIds::templateMatch(), QPointF(220, 0));
    loader.id = QStringLiteral("loader");
    source.id = QStringLiteral("tpl");
    match.id = QStringLiteral("match");
    loader.parameters.insert(QStringLiteral("path"), imagePath);
    source.parameters.insert(QStringLiteral("templatePath"), templPath);
    match.parameters.insert(QStringLiteral("minScore"), 0.8);
    doc.addNode(loader);
    doc.addNode(source);
    doc.addNode(match);

    ConnectionModel c1;
    c1.id = QStringLiteral("c1");
    c1.sourceNodeId = loader.id;
    c1.sourcePortId = QStringLiteral("out");
    c1.targetNodeId = match.id;
    c1.targetPortId = QStringLiteral("in");
    doc.addConnection(c1);

    ConnectionModel c2;
    c2.id = QStringLiteral("c2");
    c2.sourceNodeId = source.id;
    c2.sourcePortId = QStringLiteral("out");
    c2.targetNodeId = match.id;
    c2.targetPortId = QStringLiteral("template");
    doc.addConnection(c2);

    VisionContext ctx;
    Selt::ProjectVariableStore vars;
    QVERIFY(Selt::FlowRuntime::executeOnce(doc, ctx, vars, nullptr));
    QVERIFY(!ctx.hasError());
    QCOMPARE(ctx.moduleResult(source.id).status, ModuleStatus::Success);
    QCOMPARE(ctx.moduleResult(match.id).status, ModuleStatus::Success);
    QVERIFY(VisionNodeRegistry::supportedTypes().contains(VisionNodeIds::templateSource()));
}

void TestVisionIntegration::templateMatchInputPortsAreSeparated()
{
    const NodeModel match = VisionNodeRegistry::create(VisionNodeIds::templateMatch(), QPointF(0, 0));
    qreal inY = -1.0;
    qreal templateY = -1.0;
    for (const PortModel &port : match.ports) {
        if (port.id == QStringLiteral("in"))
            inY = port.relativeY;
        if (port.id == QStringLiteral("template"))
            templateY = port.relativeY;
    }
    QVERIFY(inY >= 0.0);
    QVERIFY(templateY >= 0.0);
    QVERIFY(!qFuzzyCompare(inY, templateY));
}

void TestVisionIntegration::legacyTemplateMatchPortsAreUpgraded()
{
    NodeModel match = VisionNodeRegistry::create(VisionNodeIds::templateMatch(), {});
    match.ports = {match.ports.first()};
    QVector<NodeModel> nodes{match};
    QVector<ConnectionModel> connections;

    QVERIFY(VisionNodeRegistry::upgradeGraph(nodes, connections));
    QCOMPARE(nodes.first().ports.size(), VisionNodeRegistry::descriptor(
        VisionNodeIds::templateMatch()).ports.size());
    QVERIFY(std::any_of(nodes.first().ports.cbegin(), nodes.first().ports.cend(),
                        [](const PortModel &port) {
                            return port.id == QStringLiteral("template");
                        }));
}

void TestVisionIntegration::miswiredTemplateMatchImageConnectionStillRuns()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString imagePath = temp.filePath(QStringLiteral("scene.png"));
    const QString templPath = temp.filePath(QStringLiteral("templ.png"));

    cv::Mat scene(64, 64, CV_8UC1, cv::Scalar(0));
    cv::rectangle(scene, cv::Rect(20, 18, 16, 12), cv::Scalar(255), cv::FILLED);
    cv::Mat templ = scene(cv::Rect(20, 18, 16, 12)).clone();
    QVERIFY(imwriteUnicode(imagePath, scene));
    QVERIFY(imwriteUnicode(templPath, templ));

    Document doc;
    NodeModel loader = VisionNodeRegistry::create(VisionNodeIds::imageLoader(), {});
    NodeModel match = VisionNodeRegistry::create(VisionNodeIds::templateMatch(), QPointF(220, 0));
    loader.id = QStringLiteral("loader");
    match.id = QStringLiteral("match");
    loader.parameters.insert(QStringLiteral("path"), imagePath);
    match.parameters.insert(QStringLiteral("templatePath"), templPath);
    match.parameters.insert(QStringLiteral("minScore"), 0.8);
    doc.addNode(loader);
    doc.addNode(match);

    ConnectionModel c;
    c.id = QStringLiteral("c1");
    c.sourceNodeId = loader.id;
    c.sourcePortId = QStringLiteral("out");
    c.targetNodeId = match.id;
    c.targetPortId = QStringLiteral("template");
    doc.addConnection(c);

    VisionContext ctx;
    Selt::ProjectVariableStore vars;
    QVERIFY(Selt::FlowRuntime::executeOnce(doc, ctx, vars, nullptr));
    QVERIFY(!ctx.hasError());
    QCOMPARE(ctx.moduleResult(match.id).status, ModuleStatus::Success);
}

void TestVisionIntegration::preprocessPipelineSucceeds()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString imagePath = temp.filePath(QStringLiteral("pre.png"));
    cv::Mat scene(64, 64, CV_8UC3, cv::Scalar(20, 30, 40));
    cv::rectangle(scene, cv::Rect(16, 16, 32, 32), cv::Scalar(200, 200, 200), cv::FILLED);
    QVERIFY(imwriteUnicode(imagePath, scene));

    Document doc;
    NodeModel loader = VisionNodeRegistry::create(VisionNodeIds::imageLoader(), {});
    NodeModel color = VisionNodeRegistry::create(VisionNodeIds::colorConvert(), QPointF(180, 0));
    NodeModel blur = VisionNodeRegistry::create(VisionNodeIds::gaussianBlur(), QPointF(360, 0));
    NodeModel thr = VisionNodeRegistry::create(VisionNodeIds::otsuThreshold(), QPointF(540, 0));
    NodeModel preview = VisionNodeRegistry::create(VisionNodeIds::resultPreview(), QPointF(720, 0));
    loader.id = QStringLiteral("loader");
    color.id = QStringLiteral("color");
    blur.id = QStringLiteral("blur");
    thr.id = QStringLiteral("thr");
    preview.id = QStringLiteral("preview");
    loader.parameters.insert(QStringLiteral("path"), imagePath);
    color.parameters.insert(QStringLiteral("mode"), QStringLiteral("gray"));
    doc.addNode(loader);
    doc.addNode(color);
    doc.addNode(blur);
    doc.addNode(thr);
    doc.addNode(preview);

    auto link = [&](const QString &id, const QString &src, const QString &dst) {
        ConnectionModel c;
        c.id = id;
        c.sourceNodeId = src;
        c.sourcePortId = QStringLiteral("out");
        c.targetNodeId = dst;
        c.targetPortId = QStringLiteral("in");
        doc.addConnection(c);
    };
    link(QStringLiteral("c1"), loader.id, color.id);
    link(QStringLiteral("c2"), color.id, blur.id);
    link(QStringLiteral("c3"), blur.id, thr.id);
    link(QStringLiteral("c4"), thr.id, preview.id);

    VisionContext ctx;
    Selt::ProjectVariableStore vars;
    QVERIFY(Selt::FlowRuntime::executeOnce(doc, ctx, vars, nullptr));
    QVERIFY(!ctx.hasError());
    QCOMPARE(ctx.moduleResult(thr.id).status, ModuleStatus::Success);
    QCOMPARE(ctx.moduleResult(preview.id).status, ModuleStatus::Success);
}

void TestVisionIntegration::regionBlobPipelineSucceeds()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString imagePath = temp.filePath(QStringLiteral("blob.png"));
    cv::Mat scene(80, 80, CV_8UC1, cv::Scalar(0));
    cv::circle(scene, cv::Point(40, 40), 18, cv::Scalar(255), cv::FILLED);
    QVERIFY(imwriteUnicode(imagePath, scene));

    Document doc;
    NodeModel loader = VisionNodeRegistry::create(VisionNodeIds::imageLoader(), {});
    NodeModel contours = VisionNodeRegistry::create(VisionNodeIds::findContours(), QPointF(200, 0));
    NodeModel blob = VisionNodeRegistry::create(VisionNodeIds::blobAnalyze(), QPointF(400, 0));
    loader.id = QStringLiteral("loader");
    contours.id = QStringLiteral("contours");
    blob.id = QStringLiteral("blob");
    loader.parameters.insert(QStringLiteral("path"), imagePath);
    blob.parameters.insert(QStringLiteral("minArea"), 50.0);
    doc.addNode(loader);
    doc.addNode(contours);
    doc.addNode(blob);

    ConnectionModel c1;
    c1.id = QStringLiteral("c1");
    c1.sourceNodeId = loader.id;
    c1.sourcePortId = QStringLiteral("out");
    c1.targetNodeId = contours.id;
    c1.targetPortId = QStringLiteral("in");
    doc.addConnection(c1);
    ConnectionModel c2;
    c2.id = QStringLiteral("c2");
    c2.sourceNodeId = loader.id;
    c2.sourcePortId = QStringLiteral("out");
    c2.targetNodeId = blob.id;
    c2.targetPortId = QStringLiteral("in");
    doc.addConnection(c2);

    VisionContext ctx;
    Selt::ProjectVariableStore vars;
    QVERIFY(Selt::FlowRuntime::executeOnce(doc, ctx, vars, nullptr));
    QVERIFY(!ctx.hasError());
    QCOMPARE(ctx.moduleResult(contours.id).status, ModuleStatus::Success);
    QCOMPARE(ctx.moduleResult(blob.id).status, ModuleStatus::Success);
}

void TestVisionIntegration::logicCombineDecisionSucceeds()
{
    Document doc;
    NodeModel cmp = VisionNodeRegistry::create(VisionNodeIds::numericCompare(), {});
    cmp.id = QStringLiteral("cmp");
    cmp.parameters.insert(QStringLiteral("value"), 10.0);
    cmp.parameters.insert(QStringLiteral("reference"), 5.0);
    cmp.parameters.insert(QStringLiteral("op"), QStringLiteral("gt"));
    doc.addNode(cmp);

    VisionContext ctx;
    Selt::ProjectVariableStore vars;
    QVERIFY(Selt::FlowRuntime::executeOnce(doc, ctx, vars, nullptr));
    QVERIFY(!ctx.hasError());
    QCOMPARE(ctx.moduleResult(cmp.id).status, ModuleStatus::Success);
}

void TestVisionIntegration::circleMeasureTolerancePipelineSucceeds()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString imagePath = temp.filePath(QStringLiteral("circle_tol.png"));
    cv::Mat scene(120, 120, CV_8UC1, cv::Scalar(0));
    cv::circle(scene, cv::Point(60, 60), 25, cv::Scalar(255), cv::FILLED);
    QVERIFY(imwriteUnicode(imagePath, scene));

    Document doc;
    NodeModel loader = VisionNodeRegistry::create(VisionNodeIds::imageLoader(), {});
    NodeModel measure = VisionNodeRegistry::create(VisionNodeIds::circleMeasure(), QPointF(220, 0));
    NodeModel decide = VisionNodeRegistry::create(VisionNodeIds::toleranceDecision(), QPointF(440, 0));
    loader.id = QStringLiteral("loader");
    measure.id = QStringLiteral("measure");
    decide.id = QStringLiteral("decide");
    loader.parameters.insert(QStringLiteral("path"), imagePath);
    measure.parameters.insert(QStringLiteral("minRadius"), 5.0);
    decide.parameters.insert(QStringLiteral("lower"), 40.0);
    decide.parameters.insert(QStringLiteral("upper"), 60.0);
    decide.parameters.insert(QStringLiteral("nominal"), 50.0);
    doc.addNode(loader);
    doc.addNode(measure);
    doc.addNode(decide);

    ConnectionModel c1;
    c1.id = QStringLiteral("c1");
    c1.sourceNodeId = loader.id;
    c1.sourcePortId = QStringLiteral("out");
    c1.targetNodeId = measure.id;
    c1.targetPortId = QStringLiteral("in");
    doc.addConnection(c1);

    ConnectionModel c2;
    c2.id = QStringLiteral("c2");
    c2.sourceNodeId = measure.id;
    c2.sourcePortId = QStringLiteral("diameter");
    c2.targetNodeId = decide.id;
    c2.targetPortId = QStringLiteral("value");
    doc.addConnection(c2);

    VisionContext ctx;
    Selt::ProjectVariableStore vars;
    QVERIFY(Selt::FlowRuntime::executeOnce(doc, ctx, vars, nullptr));
    QVERIFY(!ctx.hasError());
    QCOMPARE(ctx.moduleResult(measure.id).status, ModuleStatus::Success);
    QVERIFY(ctx.moduleResult(measure.id).measurement.valid);
    QCOMPARE(ctx.moduleResult(measure.id).measurement.unit, QStringLiteral("px"));
    QCOMPARE(ctx.moduleResult(decide.id).status, ModuleStatus::Success);
    // Diameter ~50 px falls in [40, 60] => ok.
    QVERIFY(ctx.moduleResult(measure.id).measurement.width > 40.0);
    QVERIFY(ctx.moduleResult(measure.id).measurement.width < 60.0);
}

void TestVisionIntegration::findContoursFitCirclePipelineSucceeds()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString imagePath = temp.filePath(QStringLiteral("fit_circle.png"));
    cv::Mat scene(100, 100, CV_8UC1, cv::Scalar(0));
    cv::circle(scene, cv::Point(50, 50), 20, cv::Scalar(255), cv::FILLED);
    QVERIFY(imwriteUnicode(imagePath, scene));

    Document doc;
    NodeModel loader = VisionNodeRegistry::create(VisionNodeIds::imageLoader(), {});
    NodeModel contours = VisionNodeRegistry::create(VisionNodeIds::findContours(), QPointF(200, 0));
    NodeModel fit = VisionNodeRegistry::create(VisionNodeIds::fitCircle(), QPointF(400, 0));
    NodeModel preview = VisionNodeRegistry::create(VisionNodeIds::resultPreview(), QPointF(600, 0));
    loader.id = QStringLiteral("loader");
    contours.id = QStringLiteral("contours");
    fit.id = QStringLiteral("fit");
    preview.id = QStringLiteral("preview");
    loader.parameters.insert(QStringLiteral("path"), imagePath);
    doc.addNode(loader);
    doc.addNode(contours);
    doc.addNode(fit);
    doc.addNode(preview);

    ConnectionModel c1;
    c1.id = QStringLiteral("c1");
    c1.sourceNodeId = loader.id;
    c1.sourcePortId = QStringLiteral("out");
    c1.targetNodeId = contours.id;
    c1.targetPortId = QStringLiteral("in");
    doc.addConnection(c1);

    ConnectionModel c2;
    c2.id = QStringLiteral("c2");
    c2.sourceNodeId = contours.id;
    c2.sourcePortId = QStringLiteral("contours");
    c2.targetNodeId = fit.id;
    c2.targetPortId = QStringLiteral("contours");
    doc.addConnection(c2);

    ConnectionModel c3;
    c3.id = QStringLiteral("c3");
    c3.sourceNodeId = loader.id;
    c3.sourcePortId = QStringLiteral("out");
    c3.targetNodeId = preview.id;
    c3.targetPortId = QStringLiteral("in");
    doc.addConnection(c3);

    VisionContext ctx;
    Selt::ProjectVariableStore vars;
    QVERIFY(Selt::FlowRuntime::executeOnce(doc, ctx, vars, nullptr));
    QVERIFY(!ctx.hasError());
    QCOMPARE(ctx.moduleResult(contours.id).status, ModuleStatus::Success);
    QCOMPARE(ctx.moduleResult(fit.id).status, ModuleStatus::Success);
    QVERIFY(ctx.moduleResult(fit.id).measurement.valid);
    QVERIFY(ctx.moduleResult(fit.id).measurement.width > 30.0);
    QCOMPARE(ctx.moduleResult(preview.id).status, ModuleStatus::Success);
}

void TestVisionIntegration::calibratedMeasureDecisionPipeline()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString imagePath = temp.filePath(QStringLiteral("cal_tol.png"));
    cv::Mat scene(120, 120, CV_8UC1, cv::Scalar(0));
    cv::circle(scene, cv::Point(60, 60), 25, cv::Scalar(255), cv::FILLED);
    QVERIFY(imwriteUnicode(imagePath, scene));

    Document doc;
    NodeModel loader = VisionNodeRegistry::create(VisionNodeIds::imageLoader(), {});
    NodeModel measure = VisionNodeRegistry::create(VisionNodeIds::circleMeasure(), QPointF(220, 0));
    NodeModel decide = VisionNodeRegistry::create(VisionNodeIds::toleranceDecision(), QPointF(440, 0));
    loader.id = QStringLiteral("loader");
    measure.id = QStringLiteral("measure");
    decide.id = QStringLiteral("decide");
    loader.parameters.insert(QStringLiteral("path"), imagePath);
    measure.parameters.insert(QStringLiteral("minRadius"), 5.0);
    // Physical diameter ~25 mm with 0.5 mm/px.
    decide.parameters.insert(QStringLiteral("lower"), 20.0);
    decide.parameters.insert(QStringLiteral("upper"), 30.0);
    decide.parameters.insert(QStringLiteral("nominal"), 25.0);
    doc.addNode(loader);
    doc.addNode(measure);
    doc.addNode(decide);

    ConnectionModel c1;
    c1.id = QStringLiteral("c1");
    c1.sourceNodeId = loader.id;
    c1.sourcePortId = QStringLiteral("out");
    c1.targetNodeId = measure.id;
    c1.targetPortId = QStringLiteral("in");
    doc.addConnection(c1);
    ConnectionModel c2;
    c2.id = QStringLiteral("c2");
    c2.sourceNodeId = measure.id;
    c2.sourcePortId = QStringLiteral("diameter");
    c2.targetNodeId = decide.id;
    c2.targetPortId = QStringLiteral("value");
    doc.addConnection(c2);

    CalibrationModel cal;
    cal.calibrationId = QStringLiteral("integration-cal");
    cal.unit = QStringLiteral("mm");
    cal.unitPerPixel = 0.5;
    cal.valid = true;
    Selt::RuntimeExecuteOptions options;
    options.hasCalibration = true;
    options.calibration = cal;

    VisionContext ctx;
    Selt::ProjectVariableStore vars;
    QVERIFY(Selt::FlowRuntime::executeOnce(doc, ctx, vars, options));
    QVERIFY(!ctx.hasError());
    QCOMPARE(ctx.moduleResult(measure.id).measurement.unit, QStringLiteral("mm"));
    QCOMPARE(ctx.moduleResult(measure.id).measurement.calibrationId, QStringLiteral("integration-cal"));
    QVERIFY(qAbs(ctx.moduleResult(measure.id).measurement.width - 25.0) < 1.0);
    QCOMPARE(ctx.moduleResult(decide.id).status, ModuleStatus::Success);
}

QTEST_MAIN(TestVisionIntegration)
#include "test_vision_integration.moc"
