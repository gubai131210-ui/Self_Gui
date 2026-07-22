#include "ui/bindingeditor.h"

#include "vision/model/portexposure.h"
#include "vision/registry/visionnodeids.h"
#include "vision/registry/visionnoderegistry.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QStringList>
#include <QVBoxLayout>

BindingEditor::BindingEditor(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    auto *top = new QHBoxLayout;
    m_kindCombo = new QComboBox(this);
    m_kindCombo->addItem(QStringLiteral("常量"), static_cast<int>(Selt::ParameterSourceKind::Constant));
    m_kindCombo->addItem(QStringLiteral("上游输出"), static_cast<int>(Selt::ParameterSourceKind::UpstreamBinding));
    m_kindCombo->addItem(QStringLiteral("项目变量"), static_cast<int>(Selt::ParameterSourceKind::ProjectVariable));
    m_typeHint = new QLabel(this);
    m_typeHint->setStyleSheet(QStringLiteral("color: gray;"));
    top->addWidget(new QLabel(QStringLiteral("来源"), this));
    top->addWidget(m_kindCombo, 1);
    top->addWidget(m_typeHint);
    layout->addLayout(top);

    m_stack = new QStackedWidget(this);
    m_constantPage = new QWidget(m_stack);
    m_constantPage->setLayout(new QVBoxLayout);
    m_constantPage->layout()->setContentsMargins(0, 0, 0, 0);

    m_upstreamPage = new QWidget(m_stack);
    auto *upForm = new QVBoxLayout(m_upstreamPage);
    upForm->setContentsMargins(0, 0, 0, 0);
    m_upstreamNodeCombo = new QComboBox(m_upstreamPage);
    m_upstreamPortCombo = new QComboBox(m_upstreamPage);
    upForm->addWidget(m_upstreamNodeCombo);
    upForm->addWidget(m_upstreamPortCombo);

    m_variablePage = new QWidget(m_stack);
    auto *varForm = new QVBoxLayout(m_variablePage);
    varForm->setContentsMargins(0, 0, 0, 0);
    m_variableCombo = new QComboBox(m_variablePage);
    varForm->addWidget(m_variableCombo);

    m_stack->addWidget(m_constantPage);
    m_stack->addWidget(m_upstreamPage);
    m_stack->addWidget(m_variablePage);
    layout->addWidget(m_stack);

    m_diagLabel = new QLabel(this);
    m_diagLabel->setObjectName(QStringLiteral("bindingDiagnostic"));
    m_diagLabel->setWordWrap(true);
    m_diagLabel->setStyleSheet(QStringLiteral("color: #2c3e50;"));
    layout->addWidget(m_diagLabel);

    m_restoreButton = new QPushButton(QStringLiteral("恢复为常量"), this);
    m_restoreButton->hide();
    layout->addWidget(m_restoreButton);

    connect(m_kindCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BindingEditor::onKindChanged);
    connect(m_upstreamNodeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BindingEditor::onUpstreamChanged);
    connect(m_upstreamPortCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BindingEditor::onUpstreamChanged);
    connect(m_variableCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BindingEditor::onVariableChanged);
    connect(m_restoreButton, &QPushButton::clicked, this, [this]() {
        emit restoreConstantRequested(m_param.key);
    });
}

void BindingEditor::setDocument(Document *document)
{
    m_document = document;
    if (!m_block && m_param.key.isEmpty())
        return;
    rebuildSourceUi();
    updateDiagnostics();
}

void BindingEditor::setVariables(const Selt::ProjectVariableStore *variables)
{
    m_variables = variables;
    if (!m_block && m_param.key.isEmpty())
        return;
    rebuildSourceUi();
    updateDiagnostics();
}

void BindingEditor::setCurrentNodeId(const QString &nodeId)
{
    m_nodeId = nodeId;
}

void BindingEditor::configure(const ModuleParamDef &param, const Selt::ParameterBinding &binding,
                              QWidget *constantEditor)
{
    m_block = true;
    m_param = param;
    m_binding = binding;
    if (m_binding.targetType == Selt::DataTypeId::None)
        m_binding.targetType = Selt::dataTypeIdFromModuleParamType(static_cast<int>(param.type));

    m_typeHint->setText(Selt::dataTypeIdDisplayName(m_binding.targetType));

    if (m_constantEditor && m_constantEditor->parent() == m_constantPage)
        m_constantEditor->setParent(nullptr);
    m_constantEditor = constantEditor;
    if (m_constantEditor) {
        m_constantPage->layout()->addWidget(m_constantEditor);
    }

    const int kindIndex = m_kindCombo->findData(static_cast<int>(m_binding.kind));
    m_kindCombo->setCurrentIndex(kindIndex >= 0 ? kindIndex : 0);
    rebuildSourceUi();
    m_block = false;
    updateDiagnostics();
}

Selt::ParameterBinding BindingEditor::currentBinding() const
{
    Selt::ParameterBinding b = m_binding;
    b.kind = static_cast<Selt::ParameterSourceKind>(m_kindCombo->currentData().toInt());
    b.targetType = Selt::dataTypeIdFromModuleParamType(static_cast<int>(m_param.type));
    if (b.kind == Selt::ParameterSourceKind::UpstreamBinding) {
        const QString nodeId = m_upstreamNodeCombo->currentData().toString();
        const QString portId = m_upstreamPortCombo->currentData().toString();
        b.upstreamNodeId = nodeId.isEmpty() ? m_binding.upstreamNodeId : nodeId;
        b.upstreamPortId = portId.isEmpty() ? m_binding.upstreamPortId : portId;
    } else if (b.kind == Selt::ParameterSourceKind::ProjectVariable) {
        const QString variableId = m_variableCombo->currentData().toString();
        b.projectVariableId = variableId.isEmpty() ? m_binding.projectVariableId : variableId;
    }
    return b;
}

QString BindingEditor::diagnosticText() const
{
    const Selt::ParameterBinding b = currentBinding();
    const Selt::DataTypeId target = Selt::dataTypeIdFromModuleParamType(
        static_cast<int>(m_param.type));
    const QString targetName = Selt::dataTypeIdDisplayName(target);
    QStringList lines;
    lines.append(QStringLiteral("来源：%1").arg(
        b.kind == Selt::ParameterSourceKind::Constant
            ? QStringLiteral("常量")
            : b.kind == Selt::ParameterSourceKind::UpstreamBinding
                ? QStringLiteral("上游输出")
                : QStringLiteral("项目变量")));
    lines.append(QStringLiteral("目标类型：%1").arg(targetName));

    if (b.kind == Selt::ParameterSourceKind::Constant) {
        lines.append(QStringLiteral("状态：已配置"));
        return lines.join(QLatin1Char('\n'));
    }

    if (b.kind == Selt::ParameterSourceKind::ProjectVariable) {
        if (b.projectVariableId.isEmpty()) {
            lines.append(QStringLiteral("状态：未选择项目变量"));
            return lines.join(QLatin1Char('\n'));
        }
        if (!m_variables || !m_variables->contains(b.projectVariableId)) {
            lines.append(QStringLiteral("状态：未定义变量“%1”").arg(b.projectVariableId));
            return lines.join(QLatin1Char('\n'));
        }
        const Selt::ProjectVariableRecord record = m_variables->record(b.projectVariableId);
        lines.append(QStringLiteral("来源值：%1 (%2)")
                         .arg(record.name, Selt::dataTypeIdDisplayName(record.dataType)));
        if (record.dataType == target) {
            lines.append(QStringLiteral("兼容状态：完全匹配"));
        } else if (Selt::dataTypesCompatible(record.dataType, target)) {
            lines.append(QStringLiteral("转换状态：可转换（%1 → %2）")
                             .arg(Selt::dataTypeIdDisplayName(record.dataType), targetName));
        } else {
            lines.append(QStringLiteral("兼容状态：类型不兼容（%1 → %2）")
                             .arg(Selt::dataTypeIdDisplayName(record.dataType), targetName));
        }
        return lines.join(QLatin1Char('\n'));
    }

    if (b.upstreamNodeId.isEmpty() || b.upstreamPortId.isEmpty()) {
        lines.append(QStringLiteral("状态：未选择上游节点或输出端口"));
        return lines.join(QLatin1Char('\n'));
    }
    if (!m_document || !m_document->hasNode(b.upstreamNodeId)) {
        lines.append(QStringLiteral("状态：上游节点不存在“%1”").arg(b.upstreamNodeId));
        return lines.join(QLatin1Char('\n'));
    }

    const NodeModel node = m_document->node(b.upstreamNodeId);
    QString sourceName = node.text.isEmpty() ? node.id : node.text;
    Selt::DataTypeId sourceType = Selt::DataTypeId::None;
    bool foundPort = false;
    for (const PortModel &port : node.ports) {
        if (port.id != b.upstreamPortId
            || (port.direction != PortDirection::Output && port.direction != PortDirection::Both)) {
            continue;
        }
        bool ok = false;
        sourceType = Selt::dataTypeIdFromString(port.dataType, &ok);
        foundPort = !port.dataType.isEmpty() && ok;
        break;
    }
#if SELT_HAS_OPENCV
    const ModuleDescriptor descriptor = VisionNodeRegistry::descriptor(node.type);
    const ModulePortDef *declaredPort = descriptor.resolvePortAlias(b.upstreamPortId, false);
    if (declaredPort) {
        sourceType = declaredPort->dataType;
        foundPort = true;
    }
#endif
    if (!foundPort) {
        lines.append(QStringLiteral("来源：%1.%2").arg(sourceName, b.upstreamPortId));
        lines.append(QStringLiteral("状态：上游输出端口不存在或类型未定义"));
        return lines.join(QLatin1Char('\n'));
    }

    lines.append(QStringLiteral("来源：%1.%2 (%3)")
                     .arg(sourceName, b.upstreamPortId, Selt::dataTypeIdDisplayName(sourceType)));
    if (sourceType == target) {
        lines.append(QStringLiteral("兼容状态：完全匹配"));
    } else if (Selt::dataTypesCompatible(sourceType, target)) {
        lines.append(QStringLiteral("转换状态：可转换（%1 → %2）")
                         .arg(Selt::dataTypeIdDisplayName(sourceType), targetName));
    } else {
        lines.append(QStringLiteral("兼容状态：类型不兼容（%1 → %2）")
                         .arg(Selt::dataTypeIdDisplayName(sourceType), targetName));
    }
    return lines.join(QLatin1Char('\n'));
}

void BindingEditor::rebuildSourceUi()
{
    const auto kind = static_cast<Selt::ParameterSourceKind>(m_kindCombo->currentData().toInt());
    switch (kind) {
    case Selt::ParameterSourceKind::UpstreamBinding:
        refreshUpstreamPorts();
        m_stack->setCurrentWidget(m_upstreamPage);
        break;
    case Selt::ParameterSourceKind::ProjectVariable:
        refreshVariables();
        m_stack->setCurrentWidget(m_variablePage);
        break;
    case Selt::ParameterSourceKind::Constant:
    default:
        m_stack->setCurrentWidget(m_constantPage);
        break;
    }
}

void BindingEditor::onKindChanged(int)
{
    if (m_block)
        return;
    rebuildSourceUi();
    emitChanged();
}

void BindingEditor::onUpstreamChanged()
{
    if (m_block)
        return;
    emitChanged();
}

void BindingEditor::onVariableChanged()
{
    if (m_block)
        return;
    emitChanged();
}

void BindingEditor::emitChanged()
{
    m_binding = currentBinding();
    updateDiagnostics();
    emit bindingChanged(m_param.key, m_binding);
}

void BindingEditor::updateDiagnostics()
{
    const Selt::ParameterBinding b = currentBinding();
    const QString text = diagnosticText();
    const bool invalid = text.contains(QStringLiteral("不存在"))
        || text.contains(QStringLiteral("未定义"))
        || text.contains(QStringLiteral("未选择"))
        || text.contains(QStringLiteral("不兼容"))
        || text.contains(QStringLiteral("未定义"));
    m_diagLabel->setText(text);
    m_diagLabel->setStyleSheet(QStringLiteral("color: %1;")
                                   .arg(invalid ? QStringLiteral("#c0392b")
                                                : QStringLiteral("#2c3e50")));
    m_diagLabel->setVisible(!text.isEmpty());
    m_restoreButton->setVisible(invalid && b.kind != Selt::ParameterSourceKind::Constant);
}

void BindingEditor::refreshUpstreamPorts()
{
    m_block = true;
    m_upstreamNodeCombo->clear();
    m_upstreamPortCombo->clear();
    if (!m_document) {
        m_block = false;
        return;
    }

    const Selt::DataTypeId expected = Selt::dataTypeIdFromModuleParamType(static_cast<int>(m_param.type));
    for (const NodeModel &node : m_document->nodes()) {
        if (node.id == m_nodeId)
            continue;
#if SELT_HAS_OPENCV
        if (!VisionNodeIds::isVisionType(node.type))
            continue;
        const ModuleDescriptor desc = VisionNodeRegistry::descriptor(node.type);
        bool hasCompatible = false;
        for (const ModulePortDef &port : desc.ports) {
            if (port.isInput)
                continue;
            if (Selt::dataTypesCompatible(port.dataType, expected) || port.dataType == expected)
                hasCompatible = true;
        }
        // Also allow legacy out port as Real/Image etc. via measurement fields stored at runtime.
        if (!hasCompatible) {
            for (const PortModel &port : node.ports) {
                if (port.direction == PortDirection::Output || port.direction == PortDirection::Both)
                    hasCompatible = true;
            }
        }
        if (!hasCompatible)
            continue;
#endif
        m_upstreamNodeCombo->addItem(node.text.isEmpty() ? node.id : node.text, node.id);
    }

    int nodeIdx = m_upstreamNodeCombo->findData(m_binding.upstreamNodeId);
    if (nodeIdx < 0 && m_upstreamNodeCombo->count() > 0)
        nodeIdx = 0;
    m_upstreamNodeCombo->setCurrentIndex(nodeIdx);

    const QString selectedNodeId = m_upstreamNodeCombo->currentData().toString();
    if (!selectedNodeId.isEmpty() && m_document->hasNode(selectedNodeId)) {
        const NodeModel node = m_document->node(selectedNodeId);
#if SELT_HAS_OPENCV
        const ModuleDescriptor desc = VisionNodeRegistry::descriptor(node.type);
        const QStringList exposed = Selt::resolveExposedPortIds(node, desc, m_document->connections());
        for (const ModulePortDef &port : desc.ports) {
            if (port.isInput)
                continue;
            if (!(Selt::dataTypesCompatible(port.dataType, expected) || port.dataType == expected))
                continue;
            const QString group = port.group.isEmpty()
                ? Selt::defaultPortGroup(port.dataType, false)
                : port.group;
            QString label = QStringLiteral("[%1] %2 (%3)")
                                .arg(group, port.name, Selt::dataTypeIdDisplayName(port.dataType));
            if (!exposed.contains(port.id))
                label += QStringLiteral(" · 已隐藏");
            m_upstreamPortCombo->addItem(label, port.id);
        }
#endif
        if (m_upstreamPortCombo->count() == 0) {
            for (const PortModel &port : node.ports) {
                if (port.direction == PortDirection::Output || port.direction == PortDirection::Both)
                    m_upstreamPortCombo->addItem(port.name.isEmpty() ? port.id : port.name, port.id);
            }
        }
    }
    int portIdx = m_upstreamPortCombo->findData(m_binding.upstreamPortId);
    if (portIdx < 0 && m_upstreamPortCombo->count() > 0)
        portIdx = 0;
    m_upstreamPortCombo->setCurrentIndex(portIdx);
    m_block = false;
}

void BindingEditor::refreshVariables()
{
    m_block = true;
    m_variableCombo->clear();
    if (!m_variables) {
        m_block = false;
        return;
    }
    const Selt::DataTypeId expected = Selt::dataTypeIdFromModuleParamType(static_cast<int>(m_param.type));
    for (const Selt::ProjectVariableRecord &rec : m_variables->records()) {
        if (!(Selt::dataTypesCompatible(rec.dataType, expected) || rec.dataType == expected))
            continue;
        m_variableCombo->addItem(
            QStringLiteral("%1 (%2)").arg(rec.name, Selt::dataTypeIdDisplayName(rec.dataType)),
            rec.id);
    }
    int idx = m_variableCombo->findData(m_binding.projectVariableId);
    if (idx < 0 && m_variableCombo->count() > 0)
        idx = 0;
    m_variableCombo->setCurrentIndex(idx);
    m_block = false;
}
