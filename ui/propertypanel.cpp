#include "propertypanel.h"

#include "core/command/documentcommands.h"
#include "core/model/document.h"
#include "core/registry/nodefactory.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QEvent>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QVBoxLayout>

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
    if (!m_nodeId.isEmpty() && m_nodeId != nodeId && m_noteEdit && m_noteEdit->hasFocus())
        applyNote();

    m_nodeId = nodeId;
    const bool enabled = m_document && m_document->hasNode(nodeId);
    setEnabled(enabled);
    if (!enabled)
        return;
    loadFromNode(m_document->node(nodeId));
}

void PropertyPanel::rebuildUi()
{
    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout;

    m_typeLabel = new QLabel(this);
    m_textEdit = new QLineEdit(this);
    m_noteEdit = new QPlainTextEdit(this);
    m_noteEdit->setMaximumHeight(80);
    m_urlEdit = new QLineEdit(this);
    m_xSpin = new QDoubleSpinBox(this);
    m_ySpin = new QDoubleSpinBox(this);
    m_wSpin = new QDoubleSpinBox(this);
    m_hSpin = new QDoubleSpinBox(this);
    m_fontSpin = new QSpinBox(this);
    m_borderSpin = new QDoubleSpinBox(this);
    m_radiusSpin = new QDoubleSpinBox(this);
    m_fillBtn = new QPushButton(this);
    m_borderBtn = new QPushButton(this);
    m_textColorBtn = new QPushButton(this);
    m_lockedCheck = new QCheckBox(QStringLiteral("锁定位置"), this);

    for (QDoubleSpinBox *spin : {m_xSpin, m_ySpin}) {
        spin->setRange(-100000, 100000);
        spin->setDecimals(1);
    }
    for (QDoubleSpinBox *spin : {m_wSpin, m_hSpin}) {
        spin->setRange(20, 2000);
        spin->setDecimals(1);
    }
    m_fontSpin->setRange(8, 72);
    m_borderSpin->setRange(0, 20);
    m_borderSpin->setDecimals(1);
    m_radiusSpin->setRange(0, 100);
    m_radiusSpin->setDecimals(1);

    form->addRow(QStringLiteral("类型"), m_typeLabel);
    form->addRow(QStringLiteral("文字"), m_textEdit);
    form->addRow(QStringLiteral("备注"), m_noteEdit);
    form->addRow(QStringLiteral("链接"), m_urlEdit);
    form->addRow(QStringLiteral("X"), m_xSpin);
    form->addRow(QStringLiteral("Y"), m_ySpin);
    form->addRow(QStringLiteral("宽度"), m_wSpin);
    form->addRow(QStringLiteral("高度"), m_hSpin);
    form->addRow(QStringLiteral("字号"), m_fontSpin);
    form->addRow(QStringLiteral("边框宽度"), m_borderSpin);
    form->addRow(QStringLiteral("圆角"), m_radiusSpin);
    form->addRow(QStringLiteral("填充色"), m_fillBtn);
    form->addRow(QStringLiteral("边框色"), m_borderBtn);
    form->addRow(QStringLiteral("文字色"), m_textColorBtn);
    form->addRow(QString(), m_lockedCheck);

    layout->addLayout(form);
    layout->addStretch();

    m_noteEdit->installEventFilter(this);

    connect(m_textEdit, &QLineEdit::editingFinished, this, &PropertyPanel::applyText);
    connect(m_urlEdit, &QLineEdit::editingFinished, this, &PropertyPanel::applyUrl);
    connect(m_xSpin, &QDoubleSpinBox::editingFinished, this, &PropertyPanel::applyGeometry);
    connect(m_ySpin, &QDoubleSpinBox::editingFinished, this, &PropertyPanel::applyGeometry);
    connect(m_wSpin, &QDoubleSpinBox::editingFinished, this, &PropertyPanel::applyGeometry);
    connect(m_hSpin, &QDoubleSpinBox::editingFinished, this, &PropertyPanel::applyGeometry);
    connect(m_fontSpin, &QSpinBox::editingFinished, this, &PropertyPanel::applyStyle);
    connect(m_borderSpin, &QDoubleSpinBox::editingFinished, this, &PropertyPanel::applyStyle);
    connect(m_radiusSpin, &QDoubleSpinBox::editingFinished, this, &PropertyPanel::applyStyle);
    connect(m_fillBtn, &QPushButton::clicked, this, &PropertyPanel::chooseFillColor);
    connect(m_borderBtn, &QPushButton::clicked, this, &PropertyPanel::chooseBorderColor);
    connect(m_textColorBtn, &QPushButton::clicked, this, &PropertyPanel::chooseTextColor);
    connect(m_lockedCheck, &QCheckBox::toggled, this, &PropertyPanel::applyLocked);
}

void PropertyPanel::loadFromNode(const NodeModel &node)
{
    blockSignalsRecursive(true);
    m_typeLabel->setText(NodeFactory::displayName(node.type));
    m_textEdit->setText(node.text);
    m_noteEdit->setPlainText(node.note);
    m_urlEdit->setText(node.url);
    m_xSpin->setValue(node.position.x());
    m_ySpin->setValue(node.position.y());
    m_wSpin->setValue(node.size.width());
    m_hSpin->setValue(node.size.height());
    m_fontSpin->setValue(node.style.fontSize);
    m_borderSpin->setValue(node.style.borderWidth);
    m_radiusSpin->setValue(node.style.cornerRadius);
    m_fillColor = node.style.fillColor;
    m_borderColor = node.style.borderColor;
    m_textColor = node.style.textColor;
    m_fillBtn->setStyleSheet(QStringLiteral("background:%1").arg(m_fillColor.name()));
    m_borderBtn->setStyleSheet(QStringLiteral("background:%1").arg(m_borderColor.name()));
    m_textColorBtn->setStyleSheet(QStringLiteral("background:%1").arg(m_textColor.name()));
    m_lockedCheck->setChecked(node.locked);
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

void PropertyPanel::applyText()
{
    NodeModel oldNode = currentNode();
    if (oldNode.id.isEmpty() || !m_undoStack)
        return;
    NodeModel newNode = oldNode;
    newNode.text = m_textEdit->text();
    if (newNode.text == oldNode.text)
        return;
    m_undoStack->push(new ChangeNodePropertyCommand(m_document, oldNode, newNode));
}

void PropertyPanel::applyNote()
{
    NodeModel oldNode = currentNode();
    if (oldNode.id.isEmpty() || !m_undoStack)
        return;
    NodeModel newNode = oldNode;
    newNode.note = m_noteEdit->toPlainText();
    if (newNode.note == oldNode.note)
        return;
    m_undoStack->push(new ChangeNodePropertyCommand(m_document, oldNode, newNode));
}

void PropertyPanel::applyUrl()
{
    NodeModel oldNode = currentNode();
    if (oldNode.id.isEmpty() || !m_undoStack)
        return;
    NodeModel newNode = oldNode;
    newNode.url = m_urlEdit->text();
    if (newNode.url == oldNode.url)
        return;
    m_undoStack->push(new ChangeNodePropertyCommand(m_document, oldNode, newNode));
}

void PropertyPanel::applyGeometry()
{
    NodeModel oldNode = currentNode();
    if (oldNode.id.isEmpty() || !m_undoStack)
        return;
    NodeModel newNode = oldNode;
    newNode.position = QPointF(m_xSpin->value(), m_ySpin->value());
    newNode.size = QSizeF(m_wSpin->value(), m_hSpin->value());
    if (newNode.position == oldNode.position && newNode.size == oldNode.size)
        return;
    m_undoStack->push(new ChangeNodePropertyCommand(m_document, oldNode, newNode));
}

void PropertyPanel::applyStyle()
{
    NodeModel oldNode = currentNode();
    if (oldNode.id.isEmpty() || !m_undoStack)
        return;
    NodeModel newNode = oldNode;
    newNode.style.fontSize = m_fontSpin->value();
    newNode.style.borderWidth = m_borderSpin->value();
    newNode.style.cornerRadius = m_radiusSpin->value();
    newNode.style.fillColor = m_fillColor;
    newNode.style.borderColor = m_borderColor;
    newNode.style.textColor = m_textColor;
    m_undoStack->push(new ChangeNodePropertyCommand(m_document, oldNode, newNode));
}

void PropertyPanel::chooseFillColor()
{
    const QColor c = QColorDialog::getColor(m_fillColor, this, QStringLiteral("填充颜色"));
    if (!c.isValid())
        return;
    m_fillColor = c;
    m_fillBtn->setStyleSheet(QStringLiteral("background:%1").arg(m_fillColor.name()));
    applyStyle();
}

void PropertyPanel::chooseBorderColor()
{
    const QColor c = QColorDialog::getColor(m_borderColor, this, QStringLiteral("边框颜色"));
    if (!c.isValid())
        return;
    m_borderColor = c;
    m_borderBtn->setStyleSheet(QStringLiteral("background:%1").arg(m_borderColor.name()));
    applyStyle();
}

void PropertyPanel::chooseTextColor()
{
    const QColor c = QColorDialog::getColor(m_textColor, this, QStringLiteral("文字颜色"));
    if (!c.isValid())
        return;
    m_textColor = c;
    m_textColorBtn->setStyleSheet(QStringLiteral("background:%1").arg(m_textColor.name()));
    applyStyle();
}

void PropertyPanel::applyLocked()
{
    NodeModel oldNode = currentNode();
    if (oldNode.id.isEmpty() || !m_undoStack)
        return;
    NodeModel newNode = oldNode;
    newNode.locked = m_lockedCheck->isChecked();
    if (newNode.locked == oldNode.locked)
        return;
    m_undoStack->push(new ChangeNodePropertyCommand(m_document, oldNode, newNode));
}

bool PropertyPanel::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_noteEdit && event->type() == QEvent::FocusOut)
        applyNote();
    return QWidget::eventFilter(watched, event);
}
