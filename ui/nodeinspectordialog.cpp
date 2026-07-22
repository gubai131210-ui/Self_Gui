#include "ui/nodeinspectordialog.h"

#include "core/model/document.h"
#include "ui/propertypanel.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

NodeInspectorDialog::NodeInspectorDialog(Document *document,
                                         QUndoStack *undoStack,
                                         const Selt::ProjectVariableStore *variables,
                                         QWidget *parent)
    : QDialog(parent)
    , m_document(document)
    , m_undoStack(undoStack)
    , m_variables(variables)
{
    setWindowFlag(Qt::Tool, true);
    setModal(false);
    setAttribute(Qt::WA_DeleteOnClose, false);
    resize(380, 620);

    auto *layout = new QVBoxLayout(this);
    m_propertyPanel = new PropertyPanel(this);
    m_propertyPanel->setDocument(m_document);
    m_propertyPanel->setUndoStack(m_undoStack);
    m_propertyPanel->setProjectVariables(m_variables);
    layout->addWidget(m_propertyPanel, 1);
    m_runtimeSummaryLabel = new QLabel(QStringLiteral("最近运行：-"), this);
    m_runtimeSummaryLabel->setWordWrap(true);
    layout->addWidget(m_runtimeSummaryLabel);

    auto *buttons = new QDialogButtonBox(this);
    m_runButton = buttons->addButton(QStringLiteral("应用并运行"), QDialogButtonBox::AcceptRole);
    auto *closeButton = buttons->addButton(QStringLiteral("关闭"), QDialogButtonBox::RejectRole);
    connect(m_runButton, &QPushButton::clicked, this, &NodeInspectorDialog::runRequested);
    connect(closeButton, &QPushButton::clicked, this, [this]() { hide(); });
    layout->addWidget(buttons);

    updateTitle();
}

void NodeInspectorDialog::setDocument(Document *document)
{
    m_document = document;
    m_propertyPanel->setDocument(document);
    refresh();
}

void NodeInspectorDialog::setUndoStack(QUndoStack *undoStack)
{
    m_undoStack = undoStack;
    m_propertyPanel->setUndoStack(undoStack);
}

void NodeInspectorDialog::setProjectVariables(const Selt::ProjectVariableStore *variables)
{
    m_variables = variables;
    m_propertyPanel->setProjectVariables(variables);
}

void NodeInspectorDialog::setNodeId(const QString &nodeId)
{
    m_nodeId = nodeId;
    refresh();
}

void NodeInspectorDialog::refresh()
{
    m_propertyPanel->setSelectedNode(m_nodeId);
    updateTitle();
}

void NodeInspectorDialog::setModuleStatusText(const QString &text)
{
    m_propertyPanel->setModuleStatusText(text);
}

void NodeInspectorDialog::setRuntimeSummaryText(const QString &text)
{
    if (m_runtimeSummaryLabel)
        m_runtimeSummaryLabel->setText(text.isEmpty()
                                           ? QStringLiteral("最近运行：-")
                                           : QStringLiteral("最近运行：%1").arg(text));
}

void NodeInspectorDialog::setRunEnabled(bool enabled)
{
    if (m_runButton)
        m_runButton->setEnabled(enabled);
}

void NodeInspectorDialog::updateTitle()
{
    QString title = QStringLiteral("模块参数");
    if (m_document && m_document->hasNode(m_nodeId))
        title += QStringLiteral(" - %1").arg(m_document->node(m_nodeId).text);
    setWindowTitle(title);
}
