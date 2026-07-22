#ifndef NODEINSPECTORDIALOG_H
#define NODEINSPECTORDIALOG_H

#include <QDialog>

class Document;
class PropertyPanel;
class QUndoStack;
class QPushButton;
class QLabel;

namespace Selt {
class ProjectVariableStore;
}

class NodeInspectorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NodeInspectorDialog(Document *document,
                                 QUndoStack *undoStack,
                                 const Selt::ProjectVariableStore *variables,
                                 QWidget *parent = nullptr);

    void setDocument(Document *document);
    void setUndoStack(QUndoStack *undoStack);
    void setProjectVariables(const Selt::ProjectVariableStore *variables);
    void setNodeId(const QString &nodeId);
    void refresh();
    QString nodeId() const { return m_nodeId; }
    void setModuleStatusText(const QString &text);
    void setRuntimeSummaryText(const QString &text);
    void setRunEnabled(bool enabled);

signals:
    void runRequested();

private:
    void updateTitle();

    Document *m_document{nullptr};
    QUndoStack *m_undoStack{nullptr};
    const Selt::ProjectVariableStore *m_variables{nullptr};
    QString m_nodeId;
    PropertyPanel *m_propertyPanel{nullptr};
    QLabel *m_runtimeSummaryLabel{nullptr};
    QPushButton *m_runButton{nullptr};
};

#endif // NODEINSPECTORDIALOG_H
