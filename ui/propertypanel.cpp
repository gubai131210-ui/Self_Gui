#include "propertypanel.h"

#include "app/nodebuilder.h"
#include "core/command/documentcommands.h"
#include "core/model/document.h"
#if SELT_HAS_OPENCV
#include "ui/bindingeditor.h"
#include "vision/data/datatype.h"
#include "vision/model/moduleparamdef.h"
#include "vision/model/portexposure.h"
#include "vision/model/roi.h"
#include "vision/model/subflowsupport.h"
#include "vision/registry/visionnodeids.h"
#include "vision/registry/visionnoderegistry.h"
#endif

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTextEdit>
#include <QToolButton>
#include <QVariant>
#include <QVBoxLayout>
#include <QMap>

#include <algorithm>

PropertyPanel::PropertyPanel(QWidget *parent)
    : QWidget(parent)
{
    rebuildUi();
    setSelectedNode(QString());
}

void PropertyPanel::setDocument(Document *document)
{
    m_document = document;
}

void PropertyPanel::setUndoStack(QUndoStack *undoStack)
{
    m_undoStack = undoStack;
}

void PropertyPanel::setProjectVariables(const Selt::ProjectVariableStore *variables)
{
    m_variables = variables;
}

void PropertyPanel::setSelectedNode(const QString &nodeId)
{
    if (m_applyingChanges > 0 && nodeId == m_nodeId)
        return;

    m_nodeId = nodeId;
    const bool enabled = m_document && m_document->hasNode(nodeId);
    setEnabled(enabled);
    if (!enabled) {
        clearDynamicWidgets();
        m_typeLabel->setText(QStringLiteral("-"));
        m_categoryLabel->setText(QStringLiteral("-"));
        m_descLabel->setText(QStringLiteral("请选择一个视觉模块"));
        m_statusLabel->setText(QStringLiteral("-"));
        m_textEdit->clear();
        return;
    }
    loadFromNode(m_document->node(nodeId));
}

void PropertyPanel::setModuleStatusText(const QString &text)
{
    m_statusLabel->setText(text.isEmpty() ? QStringLiteral("未执行") : text);
}

void PropertyPanel::updateRoiInfoLabel(const QString &key, const QJsonObject &roiJson)
{
#if SELT_HAS_OPENCV
    auto *info = qobject_cast<QLabel *>(m_paramWidgets.value(key + QStringLiteral(".info")));
    if (!info)
        return;

    const RoiRect roi = RoiRect::fromJson(roiJson);
    info->setProperty("roiJson", QVariant::fromValue(roiJson));
    if (roi.enabled && roi.rect.isValid() && roi.rect.width() > 0 && roi.rect.height() > 0) {
        info->setText(QStringLiteral("(%1,%2) %3x%4")
                          .arg(roi.rect.x(), 0, 'f', 0)
                          .arg(roi.rect.y(), 0, 'f', 0)
                          .arg(roi.rect.width(), 0, 'f', 0)
                          .arg(roi.rect.height(), 0, 'f', 0));
    } else if (roi.enabled) {
        info->setText(QStringLiteral("已启用（请在结果图 Ctrl+拖拽绘制）"));
    } else {
        info->setText(QStringLiteral("未设置"));
    }
#else
    Q_UNUSED(key);
    Q_UNUSED(roiJson);
#endif
}

void PropertyPanel::pushNodeChange(const NodeModel &oldNode, const NodeModel &newNode)
{
    if (!m_undoStack || oldNode.id.isEmpty())
        return;
    if (newNode.parameters == oldNode.parameters
        && newNode.parameterBindings == oldNode.parameterBindings
        && newNode.text == oldNode.text
        && newNode.locked == oldNode.locked
        && newNode.imagePath == oldNode.imagePath
        && newNode.exposedPortIds == oldNode.exposedPortIds
        && newNode.portExposureCustomized == oldNode.portExposureCustomized) {
        return;
    }
    ApplyingGuard guard(this);
    // Port-exposure-only edits use a dedicated command for clearer undo history.
    const bool exposureOnly =
        newNode.parameters == oldNode.parameters
        && newNode.parameterBindings == oldNode.parameterBindings
        && newNode.text == oldNode.text
        && newNode.locked == oldNode.locked
        && newNode.imagePath == oldNode.imagePath
        && (newNode.exposedPortIds != oldNode.exposedPortIds
            || newNode.portExposureCustomized != oldNode.portExposureCustomized);
    if (exposureOnly)
        m_undoStack->push(new ChangeNodePortExposureCommand(m_document, oldNode, newNode));
    else
        m_undoStack->push(new ChangeNodePropertyCommand(m_document, oldNode, newNode));
}

void PropertyPanel::applyRoiParameter(const QString &key, const QJsonObject &roiJson)
{
#if SELT_HAS_OPENCV
    NodeModel oldNode = currentNode();
    if (oldNode.id.isEmpty() || !m_undoStack)
        return;

    updateRoiInfoLabel(key, roiJson);
    if (auto *check = qobject_cast<QCheckBox *>(m_paramWidgets.value(key))) {
        check->blockSignals(true);
        check->setChecked(roiJson.value(QStringLiteral("enabled")).toBool(false));
        check->blockSignals(false);
    }

    NodeModel newNode = oldNode;
    newNode.parameters = oldNode.parameters;
    newNode.parameters.insert(key, roiJson);
    // ROI edits force constant binding for that key.
    Selt::ParameterBinding binding = Selt::ParameterBinding::legacyConstantBinding(
        roiJson, Selt::DataTypeId::Roi);
    newNode.parameterBindings = oldNode.parameterBindings;
    newNode.parameterBindings.insert(key, binding.toJson());
    pushNodeChange(oldNode, newNode);
#else
    Q_UNUSED(key);
    Q_UNUSED(roiJson);
#endif
}

void PropertyPanel::rebuildUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    auto *infoForm = new QFormLayout;
    m_typeLabel = new QLabel(this);
    m_categoryLabel = new QLabel(this);
    m_descLabel = new QLabel(this);
    m_descLabel->setWordWrap(true);
    m_statusLabel = new QLabel(this);
    infoForm->addRow(QStringLiteral("模块"), m_typeLabel);
    infoForm->addRow(QStringLiteral("分类"), m_categoryLabel);
    infoForm->addRow(QStringLiteral("说明"), m_descLabel);
    infoForm->addRow(QStringLiteral("状态"), m_statusLabel);
    layout->addLayout(infoForm);

    m_commonWidget = new QWidget(this);
    auto *commonForm = new QFormLayout(m_commonWidget);
    m_textEdit = new QLineEdit(m_commonWidget);
    m_lockedCheck = new QCheckBox(QStringLiteral("锁定位置"), m_commonWidget);
    commonForm->addRow(QStringLiteral("显示名称"), m_textEdit);
    commonForm->addRow(QString(), m_lockedCheck);
    layout->addWidget(m_commonWidget);

    auto *filterRow = new QHBoxLayout;
    m_paramFilterEdit = new QLineEdit(this);
    m_paramFilterEdit->setPlaceholderText(QStringLiteral("过滤参数..."));
    m_paramFilterEdit->setClearButtonEnabled(true);
    m_boundOnlyCheck = new QCheckBox(QStringLiteral("仅已绑定"), this);
    m_boundOnlyCheck->setToolTip(QStringLiteral("只显示非常量绑定的参数"));
    filterRow->addWidget(m_paramFilterEdit, 1);
    filterRow->addWidget(m_boundOnlyCheck);
    layout->addLayout(filterRow);

    m_dynamicScroll = new QScrollArea(this);
    m_dynamicScroll->setWidgetResizable(true);
    m_dynamicScroll->setFrameShape(QFrame::NoFrame);
    m_dynamicScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_dynamicWidget = new QWidget(m_dynamicScroll);
    m_dynamicLayout = new QVBoxLayout(m_dynamicWidget);
    m_dynamicLayout->setContentsMargins(0, 0, 4, 0);
    m_dynamicLayout->setSpacing(8);
    m_dynamicLayout->addStretch(1);
    m_dynamicScroll->setWidget(m_dynamicWidget);
    layout->addWidget(m_dynamicScroll, 1);

    connect(m_textEdit, &QLineEdit::editingFinished, this, &PropertyPanel::applyText);
    connect(m_lockedCheck, &QCheckBox::toggled, this, &PropertyPanel::applyCommon);
    connect(m_paramFilterEdit, &QLineEdit::textChanged, this, &PropertyPanel::applyParamFilter);
    connect(m_boundOnlyCheck, &QCheckBox::toggled, this, &PropertyPanel::applyParamFilter);
}

void PropertyPanel::clearDynamicWidgets()
{
    if (!m_dynamicLayout)
        return;
    while (m_dynamicLayout->count() > 0) {
        QLayoutItem *item = m_dynamicLayout->takeAt(0);
        if (!item)
            break;
        if (QWidget *w = item->widget())
            w->deleteLater();
        delete item;
    }
    m_paramWidgets.clear();
    m_bindingEditors.clear();
    m_paramGroups.clear();
    m_dynamicLayout->addStretch(1);
}

void PropertyPanel::applyParamFilter()
{
#if SELT_HAS_OPENCV
    const QString filter = m_paramFilterEdit ? m_paramFilterEdit->text().trimmed() : QString();
    const bool boundOnly = m_boundOnlyCheck && m_boundOnlyCheck->isChecked();
    for (auto it = m_bindingEditors.cbegin(); it != m_bindingEditors.cend(); ++it) {
        BindingEditor *editor = it.value();
        if (!editor)
            continue;
        QWidget *row = editor->parentWidget();
        if (!row)
            continue;
        bool visible = true;
        if (!filter.isEmpty()) {
            const QString label = row->property("paramLabel").toString();
            const QString hay = QStringLiteral("%1 %2 %3").arg(it.key(), label, editor->toolTip());
            visible = hay.contains(filter, Qt::CaseInsensitive);
        }
        if (visible && boundOnly) {
            const Selt::ParameterBinding b = editor->currentBinding();
            visible = b.kind != Selt::ParameterSourceKind::Constant;
        }
        row->setVisible(visible);
        if (QWidget *group = row->parentWidget()) {
            if (auto *form = qobject_cast<QFormLayout *>(group->layout())) {
                for (int r = 0; r < form->rowCount(); ++r) {
                    QLayoutItem *field = form->itemAt(r, QFormLayout::FieldRole);
                    if (field && field->widget() == row) {
                        if (QLayoutItem *labelItem = form->itemAt(r, QFormLayout::LabelRole)) {
                            if (labelItem->widget())
                                labelItem->widget()->setVisible(visible);
                        }
                        break;
                    }
                }
            }
        }
    }
    for (auto it = m_paramGroups.begin(); it != m_paramGroups.end(); ++it) {
        QGroupBox *box = it.value();
        if (!box)
            continue;
        bool any = false;
        if (auto *form = qobject_cast<QFormLayout *>(box->layout())) {
            for (int r = 0; r < form->rowCount(); ++r) {
                QLayoutItem *fi = form->itemAt(r, QFormLayout::FieldRole);
                if (fi && fi->widget() && fi->widget()->isVisibleTo(box))
                    any = true;
            }
        }
        const bool showBox = any || (filter.isEmpty() && !boundOnly);
        box->setVisible(showBox);
        if (showBox && any && !filter.isEmpty() && !box->isChecked())
            box->setChecked(true);
    }
#endif
}

void PropertyPanel::rebuildDynamicParameters(const NodeModel &node)
{
    clearDynamicWidgets();
#if SELT_HAS_OPENCV
    if (!VisionNodeIds::isVisionType(node.type))
        return;

    const ModuleDescriptor desc = VisionNodeRegistry::descriptor(node.type);
    QVector<ModuleParamDef> params = desc.parameters;
    std::sort(params.begin(), params.end(), [](const ModuleParamDef &a, const ModuleParamDef &b) {
        return a.displayOrder < b.displayOrder;
    });

    QMap<QString, QFormLayout *> groupForms;
    auto formForGroup = [&](const QString &group) -> QFormLayout * {
        const QString title = group.isEmpty() ? QStringLiteral("参数") : group;
        if (groupForms.contains(title))
            return groupForms.value(title);
        auto *box = new QGroupBox(title, m_dynamicWidget);
        box->setCheckable(true);
        box->setChecked(true);
        box->setFlat(false);
        auto *form = new QFormLayout(box);
        form->setContentsMargins(6, 8, 6, 6);
        form->setSpacing(6);
        // Insert before trailing stretch.
        const int insertAt = qMax(0, m_dynamicLayout->count() - 1);
        m_dynamicLayout->insertWidget(insertAt, box);
        m_paramGroups.insert(title, box);
        groupForms.insert(title, form);
        connect(box, &QGroupBox::toggled, box, [box](bool on) {
            if (auto *fl = qobject_cast<QFormLayout *>(box->layout())) {
                for (int r = 0; r < fl->rowCount(); ++r) {
                    if (QLayoutItem *li = fl->itemAt(r, QFormLayout::FieldRole)) {
                        if (li->widget())
                            li->widget()->setVisible(on);
                    }
                    if (QLayoutItem *li = fl->itemAt(r, QFormLayout::LabelRole)) {
                        if (li->widget())
                            li->widget()->setVisible(on);
                    }
                }
            }
        });
        return form;
    };

    auto *toolbar = new QWidget(m_dynamicWidget);
    auto *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(0, 0, 0, 4);
    auto *resetBtn = new QPushButton(QStringLiteral("复位默认参数"), toolbar);
    resetBtn->setToolTip(QStringLiteral("将当前节点参数恢复为算子默认值（可撤销）"));
    toolbarLayout->addWidget(resetBtn);
    toolbarLayout->addStretch(1);
    m_dynamicLayout->insertWidget(0, toolbar);
    connect(resetBtn, &QPushButton::clicked, this, [this]() {
        if (m_nodeId.isEmpty() || !m_document || !m_document->hasNode(m_nodeId))
            return;
        NodeModel oldNode = m_document->node(m_nodeId);
        NodeModel newNode = oldNode;
        newNode.parameters = VisionNodeRegistry::defaultParameters(oldNode.type);
        pushNodeChange(oldNode, newNode);
        loadFromNode(newNode);
    });

    // ---- 输入与输出（画布暴露）----
    {
        auto *ioBox = new QGroupBox(QStringLiteral("输入与输出"), m_dynamicWidget);
        ioBox->setCheckable(true);
        ioBox->setChecked(true);
        auto *ioForm = new QFormLayout(ioBox);
        ioForm->setContentsMargins(6, 8, 6, 6);
        ioForm->setSpacing(4);

        const QVector<ConnectionModel> connections = m_document ? m_document->connections()
                                                               : QVector<ConnectionModel>{};
        const QStringList exposed = Selt::resolveExposedPortIds(node, desc, connections);
        QVector<ModulePortDef> portsForUi = desc.ports;
        if (node.type == VisionNodeIds::subflow()) {
            const Selt::SubflowInterface iface =
                Selt::subflowInterfaceFromParameters(node.parameters);
            const QVector<ModulePortDef> dynamicPorts =
                Selt::modulePortsFromSubflowInterface(iface);
            if (!dynamicPorts.isEmpty())
                portsForUi = dynamicPorts;
        }
        auto *hint = new QLabel(
            QStringLiteral("勾选后在画布显示。已连接端口会强制保留。画布可见性不影响运行契约。"),
            ioBox);
        hint->setWordWrap(true);
        hint->setStyleSheet(QStringLiteral("color: gray;"));
        ioForm->addRow(hint);

        auto *resetPortsBtn = new QPushButton(QStringLiteral("恢复默认端口布局"), ioBox);
        ioForm->addRow(resetPortsBtn);
        connect(resetPortsBtn, &QPushButton::clicked, this, [this]() {
            if (m_nodeId.isEmpty() || !m_document || !m_document->hasNode(m_nodeId))
                return;
            NodeModel oldNode = m_document->node(m_nodeId);
            NodeModel newNode = oldNode;
            newNode.portExposureCustomized = false;
            ModuleDescriptor d = VisionNodeRegistry::descriptor(oldNode.type);
            if (oldNode.type == VisionNodeIds::subflow()) {
                d.ports = Selt::modulePortsFromSubflowInterface(
                    Selt::subflowInterfaceFromParameters(oldNode.parameters));
                d.hasExposureHints = true;
            }
            Selt::syncNodePortExposure(newNode, d, m_document->connections());
            Selt::layoutExposedPorts(d, newNode, m_document->connections());
            pushNodeChange(oldNode, newNode);
            loadFromNode(newNode);
        });

        const QList<Selt::PortRole> roleOrder = {Selt::PortRole::Primary, Selt::PortRole::Result,
                                                Selt::PortRole::Debug};
        for (Selt::PortRole role : roleOrder) {
            QVector<ModulePortDef> rolePorts;
            for (const ModulePortDef &port : portsForUi) {
                if (port.role == role)
                    rolePorts.append(port);
            }
            if (rolePorts.isEmpty())
                continue;

            auto *roleLabel = new QLabel(Selt::portRoleDisplayName(role), ioBox);
            QFont roleFont = roleLabel->font();
            roleFont.setBold(true);
            roleLabel->setFont(roleFont);
            ioForm->addRow(roleLabel);

            for (const ModulePortDef &port : rolePorts) {
                auto *check = new QCheckBox(ioBox);
                const QString dir = port.isInput ? QStringLiteral("输入") : QStringLiteral("输出");
                const QString typeName = Selt::dataTypeIdDisplayName(port.dataType);
                const QString groupName = port.group.isEmpty()
                    ? Selt::defaultPortGroup(port.dataType, port.isInput)
                    : port.group;
                check->setText(QStringLiteral("%1 · %2 · %3").arg(dir, typeName, groupName));
                check->setToolTip(port.description.isEmpty()
                                      ? port.name
                                      : QStringLiteral("%1\n%2").arg(port.name, port.description));
                const bool isExposed = exposed.contains(port.id);
                check->setChecked(isExposed);
                bool wired = false;
                bool bound = false;
                for (const ConnectionModel &conn : connections) {
                    if ((conn.sourceNodeId == node.id && conn.sourcePortId == port.id)
                        || (conn.targetNodeId == node.id && conn.targetPortId == port.id)) {
                        wired = true;
                        break;
                    }
                }
                for (auto it = node.parameterBindings.constBegin();
                     it != node.parameterBindings.constEnd(); ++it) {
                    const QJsonObject binding = it.value().toObject();
                    if (binding.value(QStringLiteral("sourceNodeId")).toString() == node.id
                        && binding.value(QStringLiteral("sourcePortId")).toString() == port.id) {
                        bound = true;
                        break;
                    }
                }
                if (wired)
                    check->setText(check->text() + QStringLiteral("（已连接）"));
                if (bound)
                    check->setText(check->text() + QStringLiteral("（已绑定）"));
                if (wired) {
                    check->setEnabled(false);
                    check->setChecked(true);
                }
                if (!port.exposable && !wired) {
                    check->setEnabled(false);
                    check->setToolTip(check->toolTip()
                                      + QStringLiteral("\n该端口不可由用户隐藏"));
                }
                const QString portId = port.id;
                connect(check, &QCheckBox::toggled, this, [this, portId](bool on) {
                    if (m_nodeId.isEmpty() || !m_document || !m_document->hasNode(m_nodeId))
                        return;
                    NodeModel oldNode = m_document->node(m_nodeId);
                    NodeModel newNode = oldNode;
                    ModuleDescriptor d = VisionNodeRegistry::descriptor(oldNode.type);
                    if (oldNode.type == VisionNodeIds::subflow()) {
                        d.ports = Selt::modulePortsFromSubflowInterface(
                            Selt::subflowInterfaceFromParameters(oldNode.parameters));
                        d.hasExposureHints = true;
                    }
                    QStringList ids = oldNode.portExposureCustomized
                        ? oldNode.exposedPortIds
                        : Selt::defaultExposedPortIds(d);
                    if (on && !ids.contains(portId))
                        ids.append(portId);
                    if (!on)
                        ids.removeAll(portId);
                    newNode.portExposureCustomized = true;
                    newNode.exposedPortIds = ids;
                    Selt::layoutExposedPorts(d, newNode, m_document->connections());
                    const int visibleCount = qMax(ids.size(), 2);
                    const qreal neededHeight = 48.0 + visibleCount * 18.0;
                    if (neededHeight > newNode.size.height())
                        newNode.size.setHeight(neededHeight);
                    pushNodeChange(oldNode, newNode);
                });
                const QString label = port.name.isEmpty() ? port.id : port.name;
                ioForm->addRow(label, check);
            }
        }

        const int insertAt = qMax(0, m_dynamicLayout->count() - 1);
        m_dynamicLayout->insertWidget(qMin(1, insertAt), ioBox);
        m_paramGroups.insert(QStringLiteral("输入与输出"), ioBox);
    }

#if SELT_HAS_OPENCV
    if (node.type == VisionNodeIds::subflow()) {
        auto *subBox = new QGroupBox(QStringLiteral("子流程 Terminal"), m_dynamicWidget);
        auto *subForm = new QFormLayout(subBox);
        auto *flowEdit = new QLineEdit(subBox);
        flowEdit->setText(node.parameters.value(QStringLiteral("flowId")).toString());
        auto *nameEdit = new QLineEdit(subBox);
        nameEdit->setText(node.parameters.value(QStringLiteral("displayName")).toString(
            QStringLiteral("子流程")));
        auto *ifaceEdit = new QTextEdit(subBox);
        ifaceEdit->setPlaceholderText(
            QStringLiteral("subflowInterface JSON（inputs/outputs/mappings）"));
        ifaceEdit->setMaximumHeight(120);
        const QJsonObject ifaceObj =
            node.parameters.value(QStringLiteral("subflowInterface")).toObject();
        ifaceEdit->setPlainText(
            QString::fromUtf8(QJsonDocument(ifaceObj).toJson(QJsonDocument::Compact)));
        subForm->addRow(QStringLiteral("内部流程ID"), flowEdit);
        subForm->addRow(QStringLiteral("显示名"), nameEdit);
        subForm->addRow(QStringLiteral("接口 JSON"), ifaceEdit);
        auto *applyIfaceBtn = new QPushButton(QStringLiteral("应用接口并刷新端口"), subBox);
        subForm->addRow(applyIfaceBtn);
        connect(applyIfaceBtn, &QPushButton::clicked, this,
                [this, flowEdit, nameEdit, ifaceEdit]() {
                    if (m_nodeId.isEmpty() || !m_document || !m_document->hasNode(m_nodeId))
                        return;
                    NodeModel oldNode = m_document->node(m_nodeId);
                    NodeModel newNode = oldNode;
                    newNode.parameters.insert(QStringLiteral("flowId"), flowEdit->text().trimmed());
                    newNode.parameters.insert(QStringLiteral("displayName"),
                                             nameEdit->text().trimmed());
                    QJsonParseError parseError;
                    const QJsonDocument doc =
                        QJsonDocument::fromJson(ifaceEdit->toPlainText().toUtf8(), &parseError);
                    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
                        m_statusLabel->setText(QStringLiteral("子流程接口 JSON 无效"));
                        return;
                    }
                    QString ifaceError;
                    Selt::SubflowInterface iface =
                        Selt::SubflowInterface::fromJson(doc.object(), &ifaceError);
                    if (!ifaceError.isEmpty()) {
                        m_statusLabel->setText(ifaceError);
                        return;
                    }
                    if (iface.flowId.isEmpty())
                        iface.flowId = flowEdit->text().trimmed();
                    if (iface.displayName.isEmpty())
                        iface.displayName = nameEdit->text().trimmed();
                    if (!Selt::applySubflowInterfaceToNode(newNode, iface,
                                                          m_document->connections())) {
                        m_statusLabel->setText(QStringLiteral("子流程接口校验失败"));
                        return;
                    }
                    if (!iface.displayName.isEmpty())
                        newNode.text = iface.displayName;
                    pushNodeChange(oldNode, newNode);
                    loadFromNode(newNode);
                });
        m_dynamicLayout->insertWidget(qMin(2, m_dynamicLayout->count()), subBox);
        m_paramGroups.insert(QStringLiteral("子流程 Terminal"), subBox);
    }
#endif

    for (const ModuleParamDef &param : params) {
        const QJsonValue value = node.parameters.contains(param.key)
            ? node.parameters.value(param.key)
            : param.defaultJsonValue();

        QWidget *constantEditor = nullptr;
        switch (param.type) {
        case ModuleParamType::FilePath: {
            auto *row = new QWidget(m_dynamicWidget);
            auto *rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(0, 0, 0, 0);
            auto *edit = new QLineEdit(row);
            edit->setText(value.toString());
            auto *browse = new QPushButton(QStringLiteral("浏览..."), row);
            rowLayout->addWidget(edit, 1);
            rowLayout->addWidget(browse);
            connect(edit, &QLineEdit::editingFinished, this, &PropertyPanel::applyDynamicParameters);
            connect(browse, &QPushButton::clicked, this, [this, key = param.key]() {
                browseFilePath(key);
            });
            constantEditor = row;
            m_paramWidgets.insert(param.key, edit);
            break;
        }
        case ModuleParamType::Int: {
            auto *spin = new QSpinBox(m_dynamicWidget);
            spin->setRange(param.minimum.isValid() ? param.minimum.toInt() : -1000000,
                           param.maximum.isValid() ? param.maximum.toInt() : 1000000);
            spin->setValue(value.toInt(param.defaultValue.toInt()));
            connect(spin, &QSpinBox::editingFinished, this, &PropertyPanel::applyDynamicParameters);
            constantEditor = spin;
            m_paramWidgets.insert(param.key, spin);
            break;
        }
        case ModuleParamType::Double: {
            auto *spin = new QDoubleSpinBox(m_dynamicWidget);
            spin->setDecimals(param.decimals);
            spin->setRange(param.minimum.isValid() ? param.minimum.toDouble() : -1e12,
                           param.maximum.isValid() ? param.maximum.toDouble() : 1e12);
            spin->setValue(value.toDouble(param.defaultValue.toDouble()));
            connect(spin, &QDoubleSpinBox::editingFinished, this, &PropertyPanel::applyDynamicParameters);
            constantEditor = spin;
            m_paramWidgets.insert(param.key, spin);
            break;
        }
        case ModuleParamType::Bool: {
            auto *check = new QCheckBox(param.label, m_dynamicWidget);
            check->setChecked(value.toBool(param.defaultValue.toBool()));
            connect(check, &QCheckBox::toggled, this, &PropertyPanel::applyDynamicParameters);
            constantEditor = check;
            m_paramWidgets.insert(param.key, check);
            break;
        }
        case ModuleParamType::Enum: {
            auto *combo = new QComboBox(m_dynamicWidget);
            for (const ModuleParamOption &opt : param.enumOptions)
                combo->addItem(opt.label, opt.value);
            const int idx = combo->findData(value.toString(param.defaultValue.toString()));
            combo->setCurrentIndex(idx >= 0 ? idx : 0);
            connect(combo, &QComboBox::currentIndexChanged, this, &PropertyPanel::applyDynamicParameters);
            constantEditor = combo;
            m_paramWidgets.insert(param.key, combo);
            break;
        }
        case ModuleParamType::RoiRect: {
            auto *row = new QWidget(m_dynamicWidget);
            auto *rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(0, 0, 0, 0);
            auto *enabled = new QCheckBox(QStringLiteral("启用"), row);
            auto *info = new QLabel(row);
            QJsonObject roiObj = value.toObject();
            if (roiObj.isEmpty())
                roiObj = param.defaultJsonValue().toObject();
            const RoiRect roi = RoiRect::fromJson(roiObj);
            enabled->setChecked(roi.enabled);
            rowLayout->addWidget(enabled);
            rowLayout->addWidget(info, 1);
            connect(enabled, &QCheckBox::toggled, this, &PropertyPanel::applyDynamicParameters);
            constantEditor = row;
            m_paramWidgets.insert(param.key, enabled);
            m_paramWidgets.insert(param.key + QStringLiteral(".info"), info);
            updateRoiInfoLabel(param.key, roiObj);
            break;
        }
        case ModuleParamType::Color: {
            auto *row = new QWidget(m_dynamicWidget);
            auto *rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(0, 0, 0, 0);
            auto *edit = new QLineEdit(row);
            edit->setText(value.toString(param.defaultValue.toString()));
            auto *btn = new QToolButton(row);
            btn->setText(QStringLiteral("…"));
            btn->setToolTip(QStringLiteral("选择颜色"));
            rowLayout->addWidget(edit, 1);
            rowLayout->addWidget(btn);
            connect(edit, &QLineEdit::editingFinished, this, &PropertyPanel::applyDynamicParameters);
            connect(btn, &QToolButton::clicked, this, [this, edit]() {
                const QColor current = QColor(edit->text());
                const QColor chosen = QColorDialog::getColor(
                    current.isValid() ? current : Qt::white, this, QStringLiteral("选择颜色"));
                if (!chosen.isValid())
                    return;
                edit->setText(chosen.name(QColor::HexRgb));
                applyDynamicParameters();
            });
            constantEditor = row;
            m_paramWidgets.insert(param.key, edit);
            break;
        }
        case ModuleParamType::String:
        default: {
            auto *edit = new QLineEdit(m_dynamicWidget);
            edit->setText(value.toString());
            connect(edit, &QLineEdit::editingFinished, this, &PropertyPanel::applyDynamicParameters);
            constantEditor = edit;
            m_paramWidgets.insert(param.key, edit);
            break;
        }
        }

        if (!constantEditor)
            continue;
        if (!param.tooltip.isEmpty())
            constantEditor->setToolTip(param.tooltip);

        const Selt::DataTypeId expected =
            Selt::dataTypeIdFromModuleParamType(static_cast<int>(param.type));
        Selt::ParameterBinding binding;
        if (node.parameterBindings.contains(param.key))
            binding = Selt::ParameterBinding::fromJson(node.parameterBindings.value(param.key).toObject());
        else
            binding = Selt::ParameterBinding::legacyConstantBinding(value, expected);

        auto *bindingEditor = new BindingEditor(m_dynamicWidget);
        bindingEditor->setDocument(m_document);
        bindingEditor->setVariables(m_variables);
        bindingEditor->setCurrentNodeId(node.id);
        bindingEditor->configure(param, binding, constantEditor);
        connect(bindingEditor, &BindingEditor::bindingChanged, this,
                [this](const QString &, const Selt::ParameterBinding &) {
                    applyDynamicParameters();
                });
        connect(bindingEditor, &BindingEditor::restoreConstantRequested, this,
                [this](const QString &key) { restoreConstantBinding(key); });
        m_bindingEditors.insert(param.key, bindingEditor);

        auto *wrap = new QWidget(m_dynamicWidget);
        wrap->setProperty("paramLabel", param.label);
        wrap->setProperty("paramKey", param.key);
        auto *wrapLayout = new QVBoxLayout(wrap);
        wrapLayout->setContentsMargins(0, 0, 0, 0);
        wrapLayout->setSpacing(2);
        wrapLayout->addWidget(bindingEditor);
        auto *resolved = new QLabel(wrap);
        resolved->setObjectName(QStringLiteral("ResolvedValueLabel"));
        QString stateText;
        switch (binding.kind) {
        case Selt::ParameterSourceKind::UpstreamBinding:
            stateText = QStringLiteral("绑定态: 上游 %1.%2")
                            .arg(binding.upstreamNodeId, binding.upstreamPortId);
            break;
        case Selt::ParameterSourceKind::ProjectVariable:
            stateText = QStringLiteral("绑定态: 项目变量 %1").arg(binding.projectVariableId);
            break;
        case Selt::ParameterSourceKind::Constant:
        default:
            stateText = QStringLiteral("绑定态: 常量参数");
            break;
        }
        if (!param.tooltip.isEmpty())
            stateText += QStringLiteral(" | %1").arg(param.tooltip);
        resolved->setText(stateText);
        resolved->setWordWrap(true);
        resolved->setStyleSheet(QStringLiteral("color: #9aa0a6; font-size: 11px;"));
        wrapLayout->addWidget(resolved);

        formForGroup(param.group)->addRow(param.label, wrap);
    }
    applyParamFilter();
#else
    Q_UNUSED(node);
#endif
}

void PropertyPanel::loadFromNode(const NodeModel &node)
{
    blockSignalsRecursive(true);
    m_typeLabel->setText(NodeBuilder::displayName(node.type));
#if SELT_HAS_OPENCV
    if (VisionNodeIds::isVisionType(node.type)) {
        const ModuleDescriptor desc = VisionNodeRegistry::descriptor(node.type);
        m_categoryLabel->setText(desc.category);
        m_descLabel->setText(desc.description);
    } else {
        m_categoryLabel->setText(QStringLiteral("其他"));
        m_descLabel->setText(QStringLiteral("非视觉模块（兼容旧文档）"));
    }
#else
    m_categoryLabel->setText(QStringLiteral("-"));
    m_descLabel->setText(QStringLiteral("OpenCV 未启用"));
#endif
    m_statusLabel->setText(QStringLiteral("未执行"));
    m_textEdit->setText(node.text);
    m_lockedCheck->setChecked(node.locked);
    rebuildDynamicParameters(node);
    blockSignalsRecursive(false);
}

void PropertyPanel::blockSignalsRecursive(bool block)
{
    for (QWidget *w : findChildren<QWidget *>())
        w->blockSignals(block);
}

NodeModel PropertyPanel::currentNode() const
{
    if (!m_document || !m_document->hasNode(m_nodeId))
        return {};
    return m_document->node(m_nodeId);
}

QJsonObject PropertyPanel::collectDynamicParameters() const
{
    NodeModel node = currentNode();
    QJsonObject params = node.parameters;
#if SELT_HAS_OPENCV
    if (!VisionNodeIds::isVisionType(node.type))
        return params;

    const ModuleDescriptor desc = VisionNodeRegistry::descriptor(node.type);
    for (const ModuleParamDef &param : desc.parameters) {
        BindingEditor *bindingEditor = m_bindingEditors.value(param.key, nullptr);
        if (bindingEditor) {
            const Selt::ParameterBinding binding = bindingEditor->currentBinding();
            if (binding.kind != Selt::ParameterSourceKind::Constant)
                continue; // keep previous constant snapshot for non-constant sources
        }

        QWidget *editor = m_paramWidgets.value(param.key, nullptr);
        if (!editor)
            continue;
        switch (param.type) {
        case ModuleParamType::FilePath:
        case ModuleParamType::String:
        case ModuleParamType::Color:
            if (auto *edit = qobject_cast<QLineEdit *>(editor))
                params.insert(param.key, edit->text());
            break;
        case ModuleParamType::Int:
            if (auto *spin = qobject_cast<QSpinBox *>(editor))
                params.insert(param.key, spin->value());
            break;
        case ModuleParamType::Double:
            if (auto *spin = qobject_cast<QDoubleSpinBox *>(editor))
                params.insert(param.key, spin->value());
            break;
        case ModuleParamType::Bool:
            if (auto *check = qobject_cast<QCheckBox *>(editor))
                params.insert(param.key, check->isChecked());
            break;
        case ModuleParamType::Enum:
            if (auto *combo = qobject_cast<QComboBox *>(editor))
                params.insert(param.key, combo->currentData().toString());
            break;
        case ModuleParamType::RoiRect: {
            if (auto *check = qobject_cast<QCheckBox *>(editor)) {
                QLabel *info = qobject_cast<QLabel *>(m_paramWidgets.value(param.key + QStringLiteral(".info")));
                QJsonObject roiObj;
                if (info) {
                    const QVariant stored = info->property("roiJson");
                    if (stored.canConvert<QJsonObject>())
                        roiObj = stored.toJsonObject();
                    else if (stored.typeId() == QMetaType::QVariantMap)
                        roiObj = QJsonObject::fromVariantMap(stored.toMap());
                }
                if (roiObj.isEmpty())
                    roiObj = param.defaultJsonValue().toObject();
                roiObj.insert(QStringLiteral("enabled"), check->isChecked());
                params.insert(param.key, roiObj);
            }
            break;
        }
        }
    }
#else
    Q_UNUSED(node);
#endif
    return params;
}

QJsonObject PropertyPanel::collectParameterBindings() const
{
    QJsonObject bindings;
#if SELT_HAS_OPENCV
    for (auto it = m_bindingEditors.cbegin(); it != m_bindingEditors.cend(); ++it) {
        BindingEditor *editor = it.value();
        if (!editor)
            continue;
        Selt::ParameterBinding binding = editor->currentBinding();
        if (binding.kind == Selt::ParameterSourceKind::Constant) {
            // Refresh constant payload from current editors.
            const QJsonObject params = collectDynamicParameters();
            const Selt::DataTypeId expected = binding.targetType;
            binding = Selt::ParameterBinding::legacyConstantBinding(params.value(it.key()), expected);
        }
        bindings.insert(it.key(), binding.toJson());
    }
#endif
    return bindings;
}

void PropertyPanel::applyText()
{
    NodeModel oldNode = currentNode();
    if (oldNode.id.isEmpty() || !m_undoStack)
        return;
    NodeModel newNode = oldNode;
    newNode.text = m_textEdit->text();
    pushNodeChange(oldNode, newNode);
}

void PropertyPanel::applyCommon()
{
    NodeModel oldNode = currentNode();
    if (oldNode.id.isEmpty() || !m_undoStack)
        return;
    NodeModel newNode = oldNode;
    newNode.locked = m_lockedCheck->isChecked();
    pushNodeChange(oldNode, newNode);
}

void PropertyPanel::applyDynamicParameters()
{
    NodeModel oldNode = currentNode();
    if (oldNode.id.isEmpty() || !m_undoStack)
        return;

    NodeModel newNode = oldNode;
    newNode.parameters = collectDynamicParameters();
    newNode.parameterBindings = collectParameterBindings();
#if SELT_HAS_OPENCV
    if (oldNode.type == VisionNodeIds::imageLoader())
        newNode.imagePath = newNode.parameters.value(QStringLiteral("path")).toString();

    const ModuleDescriptor desc = VisionNodeRegistry::descriptor(oldNode.type);
    for (const ModuleParamDef &param : desc.parameters) {
        if (param.type == ModuleParamType::RoiRect)
            updateRoiInfoLabel(param.key, newNode.parameters.value(param.key).toObject());
    }
#endif
    pushNodeChange(oldNode, newNode);
}

void PropertyPanel::restoreConstantBinding(const QString &key)
{
#if SELT_HAS_OPENCV
    BindingEditor *editor = m_bindingEditors.value(key, nullptr);
    if (!editor)
        return;
    NodeModel oldNode = currentNode();
    if (oldNode.id.isEmpty())
        return;
    const ModuleParamDef *param = VisionNodeRegistry::descriptor(oldNode.type).findParam(key);
    if (!param)
        return;
    const Selt::DataTypeId expected = Selt::dataTypeIdFromModuleParamType(static_cast<int>(param->type));
    const QJsonValue value = oldNode.parameters.contains(key)
        ? oldNode.parameters.value(key)
        : param->defaultJsonValue();
    editor->configure(*param, Selt::ParameterBinding::legacyConstantBinding(value, expected),
                      editor->constantEditor());
    applyDynamicParameters();
#else
    Q_UNUSED(key);
#endif
}

void PropertyPanel::browseFilePath(const QString &key)
{
#if SELT_HAS_OPENCV
    NodeModel node = currentNode();
    if (node.id.isEmpty())
        return;
    const ModuleParamDef *param = VisionNodeRegistry::descriptor(node.type).findParam(key);
    const QString filter = param ? param->fileFilter
                                 : QStringLiteral("所有文件 (*.*)");
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择文件"), QString(), filter);
    if (path.isEmpty())
        return;
    if (auto *edit = qobject_cast<QLineEdit *>(m_paramWidgets.value(key))) {
        edit->setText(path);
        applyDynamicParameters();
    }
#else
    Q_UNUSED(key);
#endif
}
