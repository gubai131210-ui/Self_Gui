#include "propertypanel.h"

#include "app/nodebuilder.h"
#include "core/command/documentcommands.h"
#include "core/model/document.h"
#if SELT_HAS_OPENCV
#include "vision/model/moduleparamdef.h"
#include "vision/model/roi.h"
#include "vision/registry/visionnodeids.h"
#include "vision/registry/visionnoderegistry.h"
#endif

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVariant>
#include <QVBoxLayout>

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

void PropertyPanel::setSelectedNode(const QString &nodeId)
{
    // 自身写回触发的 nodeUpdated 不要同步重建，否则会销毁正在发信号的控件并崩溃
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
        && newNode.text == oldNode.text
        && newNode.locked == oldNode.locked
        && newNode.imagePath == oldNode.imagePath) {
        return;
    }
    ApplyingGuard guard(this);
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
    pushNodeChange(oldNode, newNode);
#else
    Q_UNUSED(key);
    Q_UNUSED(roiJson);
#endif
}

void PropertyPanel::rebuildUi()
{
    auto *layout = new QVBoxLayout(this);

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

    m_dynamicWidget = new QWidget(this);
    m_dynamicForm = new QFormLayout(m_dynamicWidget);
    layout->addWidget(m_dynamicWidget);
    layout->addStretch();

    connect(m_textEdit, &QLineEdit::editingFinished, this, &PropertyPanel::applyText);
    connect(m_lockedCheck, &QCheckBox::toggled, this, &PropertyPanel::applyCommon);
}

void PropertyPanel::clearDynamicWidgets()
{
    if (!m_dynamicForm)
        return;
    while (m_dynamicForm->rowCount() > 0)
        m_dynamicForm->removeRow(0);
    m_paramWidgets.clear();
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

    for (const ModuleParamDef &param : params) {
        QWidget *editor = nullptr;
        const QJsonValue value = node.parameters.contains(param.key)
            ? node.parameters.value(param.key)
            : param.defaultJsonValue();

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
            editor = row;
            m_paramWidgets.insert(param.key, edit);
            break;
        }
        case ModuleParamType::Int: {
            auto *spin = new QSpinBox(m_dynamicWidget);
            spin->setRange(param.minimum.isValid() ? param.minimum.toInt() : -1000000,
                           param.maximum.isValid() ? param.maximum.toInt() : 1000000);
            spin->setValue(value.toInt(param.defaultValue.toInt()));
            connect(spin, &QSpinBox::editingFinished, this, &PropertyPanel::applyDynamicParameters);
            editor = spin;
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
            editor = spin;
            m_paramWidgets.insert(param.key, spin);
            break;
        }
        case ModuleParamType::Bool: {
            auto *check = new QCheckBox(param.label, m_dynamicWidget);
            check->setChecked(value.toBool(param.defaultValue.toBool()));
            connect(check, &QCheckBox::toggled, this, &PropertyPanel::applyDynamicParameters);
            editor = check;
            m_paramWidgets.insert(param.key, check);
            m_dynamicForm->addRow(QString(), check);
            continue;
        }
        case ModuleParamType::Enum: {
            auto *combo = new QComboBox(m_dynamicWidget);
            for (const ModuleParamOption &opt : param.enumOptions)
                combo->addItem(opt.label, opt.value);
            const int idx = combo->findData(value.toString(param.defaultValue.toString()));
            combo->setCurrentIndex(idx >= 0 ? idx : 0);
            connect(combo, &QComboBox::currentIndexChanged, this, &PropertyPanel::applyDynamicParameters);
            editor = combo;
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
            editor = row;
            m_paramWidgets.insert(param.key, enabled);
            m_paramWidgets.insert(param.key + QStringLiteral(".info"), info);
            updateRoiInfoLabel(param.key, roiObj);
            break;
        }
        case ModuleParamType::String:
        case ModuleParamType::Color:
        default: {
            auto *edit = new QLineEdit(m_dynamicWidget);
            edit->setText(value.toString());
            connect(edit, &QLineEdit::editingFinished, this, &PropertyPanel::applyDynamicParameters);
            editor = edit;
            m_paramWidgets.insert(param.key, edit);
            break;
        }
        }

        if (editor) {
            if (!param.tooltip.isEmpty())
                editor->setToolTip(param.tooltip);
            m_dynamicForm->addRow(param.label, editor);
        }
    }
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
#if SELT_HAS_OPENCV
    if (oldNode.type == VisionNodeIds::imageLoader())
        newNode.imagePath = newNode.parameters.value(QStringLiteral("path")).toString();

    // 同步 ROI 提示文字（不重建控件）
    const ModuleDescriptor desc = VisionNodeRegistry::descriptor(oldNode.type);
    for (const ModuleParamDef &param : desc.parameters) {
        if (param.type == ModuleParamType::RoiRect)
            updateRoiInfoLabel(param.key, newNode.parameters.value(param.key).toObject());
    }
#endif
    pushNodeChange(oldNode, newNode);
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
