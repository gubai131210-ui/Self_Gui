#include "core/model/document.h"
#include "vision/domain/projectvariables.h"
#include "vision/nodes/builtinexecutors.h"
#include "vision/registry/visionnodeids.h"
#include "vision/registry/visionnoderegistry.h"
#include "vision/runtime/diagnosticcodes.h"
#include "vision/runtime/flowruntime.h"
#include "vision/runtime/inputsourceservice.h"
#include "vision/runtime/runcontroller.h"
#include "vision/runtime/runtimediagnostic.h"
#include "vision/validation/graphvalidator.h"
#include "vision/workflow/workflowtemplates.h"
#include "plugin_host/pluginhost.h"

#include <QSignalSpy>
#include <QtTest>

class TestProductionHardening : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void publishElevatesMissingRequiredInput();
    void workflowTemplatesHaveValidPorts();
    void stopEmitsFinishedAndClearsBusy();
    void inputSourceClearsDiagnosticsOnClose();
    void pluginDeploymentRequirementsNonEmptyShape();
    void nodeFailurePreservesDiagnosticCode();
};

void TestProductionHardening::initTestCase()
{
    Selt::registerBuiltInOpenCvExecutors();
}

void TestProductionHardening::publishElevatesMissingRequiredInput()
{
    Document doc;
    NodeModel gray = VisionNodeRegistry::create(VisionNodeIds::grayscale(), {});
    gray.id = QStringLiteral("g");
    QVERIFY(!doc.addNode(gray).isEmpty());

    const QVector<Selt::GraphDiagnostic> editDiags = Selt::GraphValidator::validateDocument(doc);
    bool sawWarning = false;
    for (const Selt::GraphDiagnostic &d : editDiags) {
        if (d.code == QLatin1String("missing_required_input")) {
            QCOMPARE(d.severity, Selt::GraphDiagnosticSeverity::Warning);
            sawWarning = true;
        }
    }
    QVERIFY(sawWarning);

    const QVector<Selt::GraphDiagnostic> publishDiags =
        Selt::GraphValidator::validateForPublish(doc, {});
    bool sawError = false;
    bool sawCalibHint = false;
    for (const Selt::GraphDiagnostic &d : publishDiags) {
        if (d.code == QLatin1String("missing_required_input")) {
            QCOMPARE(d.severity, Selt::GraphDiagnosticSeverity::Error);
            sawError = true;
        }
        if (d.code == QLatin1String("calibration_hint"))
            sawCalibHint = true;
    }
    QVERIFY(sawError);
    QVERIFY(Selt::GraphValidator::hasErrors(publishDiags));
    // grayscale is preprocess, not measure — hint may be absent
    Q_UNUSED(sawCalibHint);
}

void TestProductionHardening::workflowTemplatesHaveValidPorts()
{
    for (const QString &id : Selt::WorkflowTemplates::templateIds()) {
        const Selt::WorkflowTemplateSpec spec = Selt::WorkflowTemplates::build(id);
        Document doc;
        for (const NodeModel &n : spec.nodes)
            QVERIFY2(!doc.addNode(n).isEmpty(), qPrintable(id));
        for (const ConnectionModel &c : spec.connections)
            QVERIFY2(!doc.addConnection(c).isEmpty(), qPrintable(id + QLatin1Char(':') + c.id));

        const QVector<Selt::GraphDiagnostic> diags = Selt::GraphValidator::validateDocument(doc);
        for (const Selt::GraphDiagnostic &d : diags) {
            QVERIFY2(d.severity != Selt::GraphDiagnosticSeverity::Error,
                     qPrintable(QStringLiteral("%1: %2 %3").arg(id, d.code, d.message)));
        }
    }
}

void TestProductionHardening::stopEmitsFinishedAndClearsBusy()
{
    Document doc;
    NodeModel loader = VisionNodeRegistry::create(VisionNodeIds::imageLoader(), {});
    loader.id = QStringLiteral("l");
    loader.enabled = false;
    doc.addNode(loader);

    Selt::RunController controller;
    controller.setDocument(&doc);
    QSignalSpy finishedSpy(&controller, &Selt::RunController::runFinished);
    QVERIFY(controller.startLoop(100));
    QVERIFY(controller.isBusy());
    controller.stop();
    QVERIFY(!controller.isBusy());
    QVERIFY(finishedSpy.count() >= 1);
    QVERIFY(controller.runOnce());
}

void TestProductionHardening::inputSourceClearsDiagnosticsOnClose()
{
    Selt::InputSourceService service;
    QSignalSpy spy(&service, &Selt::InputSourceService::diagnosticsChanged);
    QVERIFY(!service.openImageFile(QStringLiteral("Z:/no/such/image_for_selt_test.png")).ok());
    QVERIFY(!service.diagnostics().isEmpty());
    service.close();
    QVERIFY(service.diagnostics().isEmpty());
    QVERIFY(spy.count() >= 2);
}

void TestProductionHardening::pluginDeploymentRequirementsNonEmptyShape()
{
    const QStringList reqs = Selt::PluginHost::instance().deploymentRequirements();
    QVERIFY(reqs.size() >= 2);
    QVERIFY(reqs.first().contains(QStringLiteral("Qt6")));
    QVERIFY(reqs.at(1).contains(QStringLiteral("plugins/nodes")));
}

void TestProductionHardening::nodeFailurePreservesDiagnosticCode()
{
    Document doc;
    NodeModel loader = VisionNodeRegistry::create(VisionNodeIds::imageLoader(), {});
    loader.id = QStringLiteral("l");
    // Empty path → load fails with image_empty / execution diagnostic
    loader.parameters.insert(QStringLiteral("path"), QStringLiteral(""));
    QVERIFY(!doc.addNode(loader).isEmpty());

    VisionContext ctx;
    Selt::ProjectVariableStore vars;
    Selt::RuntimeExecuteOptions options;
    QVERIFY(!Selt::FlowRuntime::executeOnce(doc, ctx, vars, options));
    QVERIFY(!ctx.runtimeDiagnostics.isEmpty());
    bool hasCode = false;
    for (const Selt::RuntimeDiagnostic &d : ctx.runtimeDiagnostics) {
        if (!d.code.isEmpty() && d.code != QLatin1String("execution"))
            hasCode = true;
    }
    // Prefer a specific code; at minimum failureKind taxonomy must be execution.
    const ModuleRunResult mr = ctx.moduleResult(QStringLiteral("l"));
    QCOMPARE(mr.failureKind, QStringLiteral("execution"));
    QVERIFY(!mr.diagnosticCode.isEmpty() || hasCode);
    Q_UNUSED(hasCode);
}

QTEST_MAIN(TestProductionHardening)
#include "test_production_hardening.moc"
