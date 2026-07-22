#include "vision/algorithms/imagefilteralgorithms.h"
#include "vision/model/extendedroi.h"
#include "vision/registry/visionnodeids.h"
#include "vision/registry/visionnoderegistry.h"
#include "vision/runtime/flowscheduler.h"
#include "vision/runtime/inputsourceservice.h"
#include "vision/runtime/nodeexecutorregistry.h"
#include "vision/storage/projectcontainer.h"
#include "vision/nodes/builtinexecutors.h"
#include "vision/domain/projectvariables.h"
#include "vision/domain/workingmode.h"
#include "vision/runtime/diagnosticcodes.h"

#include <QImage>
#include <QTemporaryDir>
#include <QtTest>
#include <opencv2/core.hpp>

class Phase3ProductionTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void extendedRoiClampAndLegacy();
    void blurCannyMorphologyNodesRegistered();
    void blurExecutorWorks();
    void sequentialSchedulerWrapsRuntime();
    void imageFileInputSource();
    void projectContainerManifestValidation();
    void subflowInterfaceScopeValidation();
    void subflowInterfaceRoundTrip();
    void barcodeContractReportsUnavailableBackend();
    void workingModePolicy();
};

void Phase3ProductionTest::initTestCase()
{
    Selt::registerBuiltInOpenCvExecutors();
}

void Phase3ProductionTest::extendedRoiClampAndLegacy()
{
    Selt::ExtendedRoi roi = Selt::ExtendedRoi::fromLegacyRect(
        RoiRect{QStringLiteral("r1"), QRectF(-10, -5, 50, 40), true, false});
    roi.clampToImage(QSize(30, 30));
    QVERIFY(roi.rect.left() >= 0);
    QVERIFY(roi.rect.top() >= 0);
    QVERIFY(roi.rect.right() <= 30);
    QVERIFY(roi.rect.bottom() <= 30);
    const RoiRect legacy = roi.toLegacyRect();
    QCOMPARE(legacy.enabled, true);
}

void Phase3ProductionTest::blurCannyMorphologyNodesRegistered()
{
    QVERIFY(VisionNodeRegistry::supportedTypes().contains(VisionNodeIds::blur()));
    QVERIFY(VisionNodeRegistry::supportedTypes().contains(VisionNodeIds::canny()));
    QVERIFY(VisionNodeRegistry::supportedTypes().contains(VisionNodeIds::morphology()));
    QVERIFY(VisionNodeRegistry::supportedTypes().contains(VisionNodeIds::resize()));
    QVERIFY(VisionNodeRegistry::supportedTypes().contains(VisionNodeIds::templateMatch()));
    QVERIFY(Selt::NodeExecutorRegistry::instance().create(VisionNodeIds::blur()));
    QVERIFY(Selt::NodeExecutorRegistry::instance().create(VisionNodeIds::templateMatch()));
}

void Phase3ProductionTest::blurExecutorWorks()
{
    cv::Mat mat(32, 32, CV_8UC1, cv::Scalar(128));
    VisionImage input(mat);
    auto exec = Selt::NodeExecutorRegistry::instance().create(VisionNodeIds::blur());
    QVERIFY(exec);
    Selt::ExecutionRequest req;
    req.typeId = VisionNodeIds::blur();
    req.parameters.insert(QStringLiteral("ksize"), 5);
    req.inputs.insert(QStringLiteral("image"), Selt::DataValue(input));
    Selt::ExecutionContext ctx;
    const Selt::ExecutionResult result = exec->execute(req, ctx);
    QCOMPARE(result.status, ModuleStatus::Success);
    QVERIFY(result.outputs.contains(QStringLiteral("image")));
    QVERIFY(!result.outputs.value(QStringLiteral("image")).toImage().isEmpty());
}

void Phase3ProductionTest::sequentialSchedulerWrapsRuntime()
{
    Document doc;
    NodeModel loader = VisionNodeRegistry::create(VisionNodeIds::imageLoader(), {});
    loader.id = QStringLiteral("l");
    loader.enabled = false;
    doc.addNode(loader);
    VisionContext ctx;
    Selt::ProjectVariableStore vars;
    Selt::SequentialScheduler scheduler;
    QVERIFY(scheduler.execute(doc, ctx, vars, {}));
    QCOMPARE(ctx.moduleResults.value(loader.id).status, ModuleStatus::Disabled);
}

void Phase3ProductionTest::imageFileInputSource()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    // Create a tiny PNG via OpenCV
    const QString path = dir.filePath(QStringLiteral("t.png"));
    QImage image(8, 8, QImage::Format_RGB888);
    image.fill(QColor(10, 20, 30));
    QVERIFY(image.save(path));

    Selt::InputSourceService service;
    const Selt::Error err = service.openImageFile(path);
    QVERIFY(err.ok());
    VisionImage frame;
    QVERIFY(service.grab(frame).ok());
    QVERIFY(!frame.isEmpty());
    service.close();
    QVERIFY(!service.isOpen());
}

void Phase3ProductionTest::projectContainerManifestValidation()
{
    Selt::ProjectContainerManifest m;
    m.projectId = QStringLiteral("p1");
    QVERIFY(Selt::ProjectContainerFormat::validateManifest(m));
    m.formatVersion = 99;
    QString error;
    QVERIFY(!Selt::ProjectContainerFormat::validateManifest(m, &error));
    QVERIFY(!error.isEmpty());
}

void Phase3ProductionTest::subflowInterfaceScopeValidation()
{
    Selt::SubflowInterface iface;
    QString error;
    QVERIFY(!iface.validateScope(&error));
    iface.flowId = QStringLiteral("f1");
    QVERIFY(iface.validateScope(&error));
    iface.inputs.append({QStringLiteral("in"), QStringLiteral("输入"), true,
                         Selt::DataTypeId::Image, true, {}});
    iface.inputMappings.append({QStringLiteral("in"), QStringLiteral("n1"), QStringLiteral("image")});
    QVERIFY(iface.validateScope());
}

void Phase3ProductionTest::subflowInterfaceRoundTrip()
{
    Selt::SubflowInterface source;
    source.flowId = QStringLiteral("f1");
    source.displayName = QStringLiteral("采集宏");
    source.description = QStringLiteral("可复用采集步骤");
    source.inputs.append({QStringLiteral("image"), QStringLiteral("图像"), true,
                          Selt::DataTypeId::Image, true, QStringLiteral("输入")});
    source.outputs.append({QStringLiteral("result"), QStringLiteral("结果"), false,
                           Selt::DataTypeId::String, false, QStringLiteral("输出")});
    source.inputMappings.append({QStringLiteral("image"), QStringLiteral("n1"), QStringLiteral("out")});
    source.outputMappings.append({QStringLiteral("result"), QStringLiteral("n2"), QStringLiteral("text")});

    const Selt::SubflowInterface restored = Selt::SubflowInterface::fromJson(source.toJson());
    QCOMPARE(restored.schemaVersion, 1);
    QCOMPARE(restored.description, source.description);
    QCOMPARE(restored.inputs.size(), 1);
    QCOMPARE(restored.outputs.first().dataType, Selt::DataTypeId::String);
    QCOMPARE(restored.inputMappings.first().innerNodeId, QStringLiteral("n1"));
    QVERIFY(restored.validateScope());
}

void Phase3ProductionTest::barcodeContractReportsUnavailableBackend()
{
    const ModuleDescriptor descriptor = VisionNodeRegistry::descriptor(VisionNodeIds::barcodeDecode());
    QVERIFY(!descriptor.typeId.isEmpty());
    QVERIFY(descriptor.findInputPort(QStringLiteral("in")));
    QVERIFY(descriptor.findOutputPort(QStringLiteral("text")));
    QVERIFY(descriptor.findOutputPort(QStringLiteral("found")));

    const auto executor = Selt::NodeExecutorRegistry::instance().create(VisionNodeIds::barcodeDecode());
    QVERIFY(executor);
    Selt::ExecutionRequest request;
    request.typeId = VisionNodeIds::barcodeDecode();
    Selt::ExecutionContext context;
    const Selt::ExecutionResult result = executor->execute(request, context);
    QCOMPARE(result.status, ModuleStatus::Failed);
    QCOMPARE(result.diagnosticCode, Selt::DiagnosticCodes::imageEmpty());
}

void Phase3ProductionTest::workingModePolicy()
{
    Selt::WorkingModePolicy engineer;
    QVERIFY(engineer.canEditTopology());
    QVERIFY(engineer.canManagePlugins());

    Selt::WorkingModePolicy operatorMode{Selt::WorkingMode::Operator};
    QVERIFY(!operatorMode.canEditTopology());
    QVERIFY(!operatorMode.canManagePlugins());
    QVERIFY(operatorMode.canRun());
    QCOMPARE(Selt::workingModeFromString(QStringLiteral("operator")), Selt::WorkingMode::Operator);
}

QTEST_MAIN(Phase3ProductionTest)
#include "test_phase3_production.moc"
