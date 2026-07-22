#include "ui/bindingeditor.h"
#include "vision/data/datatype.h"
#include "vision/domain/projectvariables.h"
#include "vision/model/moduleparamdef.h"
#include "vision/registry/visionnodeids.h"
#include "vision/registry/visionnoderegistry.h"
#include "vision/runtime/bindingresolver.h"
#include "vision/runtime/portvaluestore.h"

#include <QApplication>
#include <QLabel>
#include <QtTest>

class BindingEditorLogicTest : public QObject
{
    Q_OBJECT
private slots:
    void legacyParametersBecomeConstantBindings();
    void upstreamBindingRoundTripOnNode();
    void restoreConstantClearsUpstream();
    void projectVariableBindingResolves();
    void diagnosticReportsUndefinedVariable();
    void diagnosticReportsMissingUpstreamPort();
};

void BindingEditorLogicTest::legacyParametersBecomeConstantBindings()
{
    NodeModel node = VisionNodeRegistry::create(VisionNodeIds::threshold(), {});
    QVERIFY(node.parameterBindings.isEmpty());
    const ModuleParamDef *param = VisionNodeRegistry::descriptor(node.type).findParam(QStringLiteral("threshold"));
    QVERIFY(param);
    const Selt::ParameterBinding binding = Selt::BindingResolver::bindingForParameter(node, *param);
    QCOMPARE(binding.kind, Selt::ParameterSourceKind::Constant);
    QVERIFY(binding.isValid());
    QCOMPARE(binding.targetType, Selt::DataTypeId::Int);
}

void BindingEditorLogicTest::upstreamBindingRoundTripOnNode()
{
    NodeModel node = VisionNodeRegistry::create(VisionNodeIds::threshold(), {});
    node.id = QStringLiteral("thr");
    auto binding = Selt::ParameterBinding::upstream(QStringLiteral("measure"), QStringLiteral("width"),
                                                    Selt::DataTypeId::Int);
    node.parameterBindings.insert(QStringLiteral("threshold"), binding.toJson());

    const ModuleParamDef *param = VisionNodeRegistry::descriptor(node.type).findParam(QStringLiteral("threshold"));
    QVERIFY(param);
    const Selt::ParameterBinding loaded = Selt::BindingResolver::bindingForParameter(node, *param);
    QCOMPARE(loaded.kind, Selt::ParameterSourceKind::UpstreamBinding);
    QCOMPARE(loaded.upstreamNodeId, QStringLiteral("measure"));
    QCOMPARE(loaded.upstreamPortId, QStringLiteral("width"));
}

void BindingEditorLogicTest::restoreConstantClearsUpstream()
{
    NodeModel node = VisionNodeRegistry::create(VisionNodeIds::threshold(), {});
    node.parameters.insert(QStringLiteral("threshold"), 64);
    auto binding = Selt::ParameterBinding::upstream(QStringLiteral("x"), QStringLiteral("out"),
                                                    Selt::DataTypeId::Int);
    node.parameterBindings.insert(QStringLiteral("threshold"), binding.toJson());

    const auto restored = Selt::ParameterBinding::legacyConstantBinding(
        node.parameters.value(QStringLiteral("threshold")), Selt::DataTypeId::Int);
    QCOMPARE(restored.kind, Selt::ParameterSourceKind::Constant);
    node.parameterBindings.insert(QStringLiteral("threshold"), restored.toJson());

    const ModuleParamDef *param = VisionNodeRegistry::descriptor(node.type).findParam(QStringLiteral("threshold"));
    const Selt::ParameterBinding loaded = Selt::BindingResolver::bindingForParameter(node, *param);
    QCOMPARE(loaded.kind, Selt::ParameterSourceKind::Constant);
}

void BindingEditorLogicTest::projectVariableBindingResolves()
{
    Selt::ProjectVariableStore vars;
    const QString id = vars.add(QStringLiteral("maxV"), Selt::DataTypeId::Int, Selt::DataValue(200));
    NodeModel node = VisionNodeRegistry::create(VisionNodeIds::threshold(), {});
    node.parameterBindings.insert(
        QStringLiteral("maxValue"),
        Selt::ParameterBinding::projectVariable(id, Selt::DataTypeId::Int).toJson());

    Selt::PortValueStore ports;
    QJsonObject legacy;
    QHash<QString, Selt::DataValue> typed;
    QString error;
    QVERIFY(Selt::BindingResolver::resolveNodeParameters(
        node, VisionNodeRegistry::descriptor(node.type).parameters, ports, vars, &legacy, &typed, &error));
    QCOMPARE(legacy.value(QStringLiteral("maxValue")).toInt(), 200);
    QCOMPARE(typed.value(QStringLiteral("maxValue")).toInt(), 200);
}

void BindingEditorLogicTest::diagnosticReportsUndefinedVariable()
{
    const ModuleParamDef *param =
        VisionNodeRegistry::descriptor(VisionNodeIds::threshold()).findParam(QStringLiteral("threshold"));
    QVERIFY(param);
    BindingEditor editor;
    editor.setVariables(nullptr);
    editor.configure(*param, Selt::ParameterBinding::projectVariable(
                                  QStringLiteral("missing"), Selt::DataTypeId::Int),
                     new QWidget);
    QVERIFY(editor.diagnosticText().contains(QStringLiteral("未定义变量")));
    QVERIFY(editor.findChild<QLabel *>(QStringLiteral("bindingDiagnostic")) != nullptr);
}

void BindingEditorLogicTest::diagnosticReportsMissingUpstreamPort()
{
    Document document;
    NodeModel source = VisionNodeRegistry::create(VisionNodeIds::threshold(), {});
    source.id = QStringLiteral("source");
    document.addNode(source);

    const ModuleParamDef *param =
        VisionNodeRegistry::descriptor(VisionNodeIds::threshold()).findParam(QStringLiteral("threshold"));
    QVERIFY(param);
    BindingEditor editor;
    editor.setDocument(&document);
    editor.configure(*param, Selt::ParameterBinding::upstream(
                                  QStringLiteral("source"), QStringLiteral("missing"),
                                  Selt::DataTypeId::Int),
                     new QWidget);
    QVERIFY(editor.diagnosticText().contains(QStringLiteral("来源：")));
    QVERIFY(editor.diagnosticText().contains(QStringLiteral("目标类型：")));
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    BindingEditorLogicTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "test_binding_editor.moc"
