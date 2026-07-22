#ifndef BINDINGEDITOR_H
#define BINDINGEDITOR_H

#include "core/model/document.h"
#include "core/model/nodemodel.h"
#include "vision/data/datatype.h"
#include "vision/domain/projectvariables.h"
#include "vision/model/moduleparamdef.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;
class QStackedWidget;
class QWidget;

#if !defined(SELT_HAS_OPENCV)
#define SELT_HAS_OPENCV 0
#endif

class BindingEditor : public QWidget
{
    Q_OBJECT
public:
    explicit BindingEditor(QWidget *parent = nullptr);

    void setDocument(Document *document);
    void setVariables(const Selt::ProjectVariableStore *variables);
    void setCurrentNodeId(const QString &nodeId);
    void configure(const ModuleParamDef &param, const Selt::ParameterBinding &binding,
                   QWidget *constantEditor);

    /// compact=true：只显示当前来源页，隐藏「来源」行与诊断（检查器紧凑模式）。
    void setCompactMode(bool compact);
    bool isCompactMode() const { return m_compact; }

    Selt::ParameterBinding currentBinding() const;
    QWidget *constantEditor() const { return m_constantEditor; }
    QString diagnosticText() const;

signals:
    void bindingChanged(const QString &key, const Selt::ParameterBinding &binding);
    void restoreConstantRequested(const QString &key);

private:
    void rebuildSourceUi();
    void onKindChanged(int index);
    void onUpstreamChanged();
    void onVariableChanged();
    void emitChanged();
    void updateDiagnostics();
    void refreshUpstreamPorts();
    void refreshVariables();

    Document *m_document{nullptr};
    const Selt::ProjectVariableStore *m_variables{nullptr};
    QString m_nodeId;
    ModuleParamDef m_param;
    Selt::ParameterBinding m_binding;

    QComboBox *m_kindCombo{nullptr};
    QStackedWidget *m_stack{nullptr};
    QWidget *m_constantPage{nullptr};
    QWidget *m_upstreamPage{nullptr};
    QWidget *m_variablePage{nullptr};
    QWidget *m_constantEditor{nullptr};
    QComboBox *m_upstreamNodeCombo{nullptr};
    QComboBox *m_upstreamPortCombo{nullptr};
    QComboBox *m_variableCombo{nullptr};
    QLabel *m_typeHint{nullptr};
    QLabel *m_diagLabel{nullptr};
    QPushButton *m_restoreButton{nullptr};
    QWidget *m_topRow{nullptr};
    bool m_block{false};
    bool m_compact{false};
};

#endif // BINDINGEDITOR_H
