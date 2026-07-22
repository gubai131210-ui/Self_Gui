#include "propertypanel.h"

#include "app/nodebuilder.h"
#include "core/command/documentcommands.h"
#include "core/model/document.h"
#include "ui/theme/uistyle.h"
#include "ui/widgets/collapsiblesection.h"
#include "ui/widgets/compactparamrow.h"
#include "ui/widgets/inspectorheader.h"
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

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTextEdit>
#include <QToolButton>
#include <QVariant>
#include <QVBoxLayout>
#include <QMap>

#include <algorithm>

namespace {

#if SELT_HAS_OPENCV
QString classifyParamSection(const ModuleParamDef &param)
{
    const QString group = param.group;
    if (param.type == ModuleParamType::RoiRect
        || group.contains(QStringLiteral("ROI"), Qt::CaseInsensitive)
        || group.contains(QStringLiteral("区域"))) {
        return QStringLiteral("ROI");
    }
    if (param.displayOrder >= 100)
        return QStringLiteral("调试");
    if (param.displayOrder >= 80)
        return QStringLiteral("高级");
    return QStringLiteral("基本");
}
#endif

} // namespace

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
        if (m_header)
            m_header->setEmpty();
        clearResultThumbnail();
        if (m_textEdit)
            m_textEdit->clear();
        return;
    }
    loadFromNode(m_document->node(nodeId));
}

void PropertyPanel::setModuleStatusText(const QString &text)
{
    NodeRunVisualStatus status = NodeRunVisualStatus::Idle;
    const QString lower = text.toLower();
    if (lower.contains(QStringLiteral("fail")) || text.contains(QStringLiteral("失败"))
        || text.contains(QStringLiteral("错误"))) {
        status = NodeRunVisualStatus::Failed;
    } else if (lower.contains(QStringLiteral("run")) || text.contains(QStringLiteral("运行中"))) {
        status = NodeRunVisualStatus::Running;
    } else if (text.contains(QStringLiteral("警告"))
               || lower.contains(QStringLiteral("warning"))) {
        status = NodeRunVisualStatus::Warning;
    } else if (lower.contains(QStringLiteral("ok")) || text.contains(QStringLiteral("成功"))
               || text.contains(QStringLiteral("完成"))) {
        status = NodeRunVisualStatus::Success;
    }
    setModuleRunStatus(status, text);
}

void PropertyPanel::setModuleRunStatus(NodeRunVisualStatus status, const QString &text)
{
    if (m_header)
        m_header->setStatus(status, text);
}

void PropertyPanel::setResultThumbnail(const QImage &image)
{
    if (!m_thumbnail)
        return;
    if (image.isNull()) {
        clearResultThumbnail();
        return;
    }
    const QImage scaled = image.scaled(m_thumbnail->width() > 0 ? m_thumbnail->width() : 280,
                                       100, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_thumbnail->setPixmap(QPixmap::fromImage(scaled));
    m_thumbnail->setVisible(true);
}

void PropertyPanel::clearResultThumbnail()
{
    if (!m_thumbnail)
        return;
    m_thumbnail->clear();
    m_thumbnail->setVisible(false);
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
    layout->setContentsMargins(Selt::UiStyle::panelMargin,
                               Selt::UiStyle::panelMargin,
                               Selt::UiStyle::panelMargin,
                               Selt::UiStyle::panelMargin);
    layout->setSpacing(Selt::UiStyle::compactSpacing);

    m_header = new InspectorHeader(this);
    layout->addWidget(m_header);
    connect(m_header, &InspectorHeader::runNodeRequested, this, [this]() {
        if (!m_nodeId.isEmpty())
            emit runNodeRequested(m_nodeId);
    });
    connect(m_header, &InspectorHeader::copyParamsRequested, this,
            &PropertyPanel::copyParametersToClipboard);

    m_thumbnail = new QLabel(this);
    m_thumbnail->setFixedHeight(100);
    m_thumbnail->setAlignment(Qt::AlignCenter);
    m_thumbnail->setStyleSheet(
        QStringLiteral("background-color: %1; border: 1px solid %2; border-radius: 4px;")
            .arg(Selt::UiStyle::panelAltBackground().name(),
                 Selt::UiStyle::border().name()));
    m_thumbnail->hide();
    layout->addWidget(m_thumbnail);

    auto *commonRow = new QHBoxLayout;
    m_textEdit = new QLineEdit(this);
    m_textEdit->setPlaceholderText(QStringLiteral("显示名称"));
    m_lockedCheck = new QCheckBox(QStringLiteral("锁定"), this);
    commonRow->addWidget(m_textEdit, 1);
    commonRow->addWidget(m_lockedCheck);
    layout->addLayout(commonRow);

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
    m_paramRows.clear();
    m_sections.clear();
    m_dynamicLayout->addStretch(1);
}

void PropertyPanel::applyParamFilter()
{
#if SELT_HAS_OPENCV
    const QString filter = m_paramFilterEdit ? m_paramFilterEdit->text().trimmed() : QString();
    const bool boundOnly = m_boundOnlyCheck && m_boundOnlyCheck->isChecked();
    for (auto it = m_paramRows.cbegin(); it != m_paramRows.cend(); ++it) {
        CompactParamRow *row = it.value();
        if (!row)
            continue;
        bool visible = true;
        if (!filter.isEmpty()) {
            const QString hay = QStringLiteral("%1 %2").arg(it.key(), row->paramLabel());
            visible = hay.contains(filter, Qt::CaseInsensitive);
        }
        if (visible && boundOnly) {
            BindingEditor *editor = m_bindingEditors.value(it.key());
            if (editor) {
                const Selt::ParameterBinding b = editor->currentBinding();
                visible = b.kind != Selt::ParameterSourceKind::Constant;
            }
        }
        row->setVisible(visible);
        if (visible && !filter.isEmpty()) {
            QWidget *p = row->parentWidget();
            while (p) {
                if (auto *section = qobject_cast<CollapsibleSection *>(p)) {
                    if (!section->isExpanded())
                        section->setExpanded(true);
                    break;
                }
                p = p->parentWidget();
            }
        }
    }
    for (auto it = m_sections.begin(); it != m_sections.end(); ++it) {
        CollapsibleSection *section = it.value();
        if (!section)
            continue;
        bool any = false;
        for (auto rit = m_paramRows.cbegin(); rit != m_paramRows.cend(); ++rit) {
            CompactParamRow *row = rit.value();
            if (!row || !row->isVisible())
                continue;
            // row parent is body widget inside section
            QWidget *p = row->parentWidget();
            while (p && p != section)
                p = p->parentWidget();
            if (p == section)
                any = true;
        }
        // Port / subflow sections keep their own widgets; leave visible.
        if (it.key() == QStringLiteral("端口暴露") || it.key() == QStringLiteral("子流程"))
            continue;
        section->setVisible(any || (filter.isEmpty() && !boundOnly));
    }
#endif
}

#if SELT_HAS_OPENCV
CollapsibleSection *PropertyPanel::ensureSection(const QString &title, bool defaultExpanded)
{
    if (m_sections.contains(title))
        return m_sections.value(title);

    const QString settingsKey = QStringLiteral("inspector/section/%1").arg(title);
    auto *section = new CollapsibleSection(title, settingsKey, defaultExpanded, m_dynamicWidget);
    const int insertAt = qMax(0, m_dynamicLayout->count() - 1);
    m_dynamicLayout->insertWidget(insertAt, section);
    m_sections.insert(title, section);
    return section;
}
#endif

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

    // ---- 基本 / ROI / 高级 / 调试（按需创建 Section）----
    for (const ModuleParamDef &param : params) {
        const QJsonValue value = node.parameters.contains(param.key)
            ? node.parameters.value(param.key)
            : param.defaultJsonValue();

        QWidget *constantEditor = nullptr;
        bool hasSlider = false;
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
            hasSlider = true;
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
            hasSlider = true;
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

        auto *compact = new CompactParamRow(param.label, bindingEditor, hasSlider, m_dynamicWidget);
        compact->setProperty("paramKey", param.key);
        compact->setBindingIndicator(binding.kind);
        if (hasSlider)
            connect(compact, &CompactParamRow::sliderEdited, this, &PropertyPanel::applyDynamicParameters);
        m_paramRows.insert(param.key, compact);

        const QString sectionName = classifyParamSection(param);
        const bool defaultExpanded =
            sectionName == QStringLiteral("基本") || sectionName == QStringLiteral("ROI");
        CollapsibleSection *section = ensureSection(sectionName, defaultExpanded);
        if (QVBoxLayout *bodyLayout = section->contentLayout())
            bodyLayout->addWidget(compact);
    }

    // ---- 子流程 ----
    if (node.type == VisionNodeIds::subflow()) {
        CollapsibleSection *subSection = ensureSection(QStringLiteral("子流程"), true);
        QVBoxLayout *bodyLayout = subSection->contentLayout();
        auto *subBox = new QWidget(subSection);
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
                        setModuleStatusText(QStringLiteral("子流程接口 JSON 无效"));
                        return;
                    }
                    QString ifaceError;
                    Selt::SubflowInterface iface =
                        Selt::SubflowInterface::fromJson(doc.object(), &ifaceError);
                    if (!ifaceError.isEmpty()) {
                        setModuleStatusText(ifaceError);
                        return;
                    }
                    if (iface.flowId.isEmpty())
                        iface.flowId = flowEdit->text().trimmed();
                    if (iface.displayName.isEmpty())
                        iface.displayName = nameEdit->text().trimmed();
                    if (!Selt::applySubflowInterfaceToNode(newNode, iface,
                                                          m_document->connections())) {
                        setModuleStatusText(QStringLiteral("子流程接口校验失败"));
                        return;
                    }
                    if (!iface.displayName.isEmpty())
                        newNode.text = iface.displayName;
                    pushNodeChange(oldNode, newNode);
                    loadFromNode(newNode);
                });
        if (bodyLayout)
            bodyLayout->addWidget(subBox);
    }

    // ---- 端口暴露（底部，默认折叠）----
    {
        CollapsibleSection *ioSection = ensureSection(QStringLiteral("端口暴露"), false);
        QVBoxLayout *bodyLayout = ioSection->contentLayout();
        auto *ioBox = new QWidget(ioSection);
        auto *ioForm = new QFormLayout(ioBox);
        ioForm->setContentsMargins(0, 0, 0, 0);
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
            QStringLiteral("勾选后在画布显示。已连接端口会强制保留。"),
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
                for (const ConnectionModel &conn : connections) {
                    if ((conn.sourceNodeId == node.id && conn.sourcePortId == port.id)
                        || (conn.targetNodeId == node.id && conn.targetPortId == port.id)) {
                        wired = true;
                        break;
                    }
                }
                if (wired) {
                    check->setText(check->text() + QStringLiteral("（已连接）"));
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
        if (bodyLayout)
            bodyLayout->addWidget(ioBox);

        // Keep port section at bottom.
        m_dynamicLayout->removeWidget(ioSection);
        m_dynamicLayout->insertWidget(qMax(0, m_dynamicLayout->count() - 1), ioSection);
    }

    // Hide empty optional sections (created only when needed, but keep safe).
    for (const QString &name : {QStringLiteral("ROI"), QStringLiteral("高级"), QStringLiteral("调试")}) {
        CollapsibleSection *section = m_sections.value(name);
        if (!section)
            continue;
        QVBoxLayout *bodyLayout = section->contentLayout();
        if (!bodyLayout || bodyLayout->count() == 0)
            section->hide();
    }

    applyParamFilter();
#else
    Q_UNUSED(node);
#endif
}

void PropertyPanel::loadFromNode(const NodeModel &node)
{
    blockSignalsRecursive(true);
#if SELT_HAS_OPENCV
    if (VisionNodeIds::isVisionType(node.type)) {
        const ModuleDescriptor desc = VisionNodeRegistry::descriptor(node.type);
        if (m_header) {
            const QString title = node.text.isEmpty()
                ? NodeBuilder::displayName(node.type)
                : node.text;
            m_header->setNodeInfo(title,
                                  desc.category,
                                  desc.iconKey.isEmpty() ? desc.category : desc.iconKey);
        }
    } else if (m_header) {
        const QString title = node.text.isEmpty()
            ? NodeBuilder::displayName(node.type)
            : node.text;
        m_header->setNodeInfo(title,
                              QStringLiteral("其他"),
                              QString());
    }
#else
    if (m_header) {
        const QString title = node.text.isEmpty()
            ? NodeBuilder::displayName(node.type)
            : node.text;
        m_header->setNodeInfo(title, QStringLiteral("-"), QString());
    }
#endif
    // 不在每次参数重建时清零运行状态，由 MainWindow::syncSelectedModuleStatus 维护。
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
                continue;
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

    for (auto it = m_bindingEditors.cbegin(); it != m_bindingEditors.cend(); ++it) {
        if (CompactParamRow *row = m_paramRows.value(it.key()))
            row->setBindingIndicator(it.value()->currentBinding().kind);
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
    editor->setCompactMode(true);
    if (CompactParamRow *row = m_paramRows.value(key))
        row->setBindingIndicator(Selt::ParameterSourceKind::Constant);
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

void PropertyPanel::copyParametersToClipboard()
{
    NodeModel node = currentNode();
    if (node.id.isEmpty())
        return;
    const QByteArray bytes = QJsonDocument(node.parameters).toJson(QJsonDocument::Indented);
    if (QClipboard *clipboard = QApplication::clipboard())
        clipboard->setText(QString::fromUtf8(bytes));
    emit copyParamsRequested(node.id);
}
