#ifndef PROPERTYPANEL_H
#define PROPERTYPANEL_H

#include "core/model/nodemodel.h"

#include <QHash>
#include <QWidget>

#ifndef SELT_HAS_OPENCV
#define SELT_HAS_OPENCV 0
#endif

namespace Selt {
class ProjectVariableStore;
}

class Document;
class QUndoStack;
class QLineEdit;
class QLabel;
class QFormLayout;
class QCheckBox;
class QWidget;
class QScrollArea;
class QVBoxLayout;
class QGroupBox;
class BindingEditor;

class PropertyPanel : public QWidget
{
    Q_OBJECT

public:
    explicit PropertyPanel(QWidget *parent = nullptr);

    void setDocument(Document *document);
    void setUndoStack(QUndoStack *undoStack);
    void setProjectVariables(const Selt::ProjectVariableStore *variables);
    void setSelectedNode(const QString &nodeId);
    void setModuleStatusText(const QString &text);
    void applyRoiParameter(const QString &key, const QJsonObject &roiJson);
    /// 参数面板自身正在写回 Document 时为 true，避免 nodeUpdated 同步重建导致崩溃
    bool isApplyingChanges() const { return m_applyingChanges; }

private slots:
    void applyText();
    void applyCommon();
    void applyDynamicParameters();
    void browseFilePath(const QString &key);

private:
    class ApplyingGuard {
    public:
        explicit ApplyingGuard(PropertyPanel *panel)
            : m_panel(panel)
        {
            if (m_panel)
                ++m_panel->m_applyingChanges;
        }
        ~ApplyingGuard()
        {
            if (m_panel && m_panel->m_applyingChanges > 0)
                --m_panel->m_applyingChanges;
        }
    private:
        PropertyPanel *m_panel{nullptr};
    };

    void rebuildUi();
    void clearDynamicWidgets();
    void rebuildDynamicParameters(const NodeModel &node);
    void applyParamFilter();
    void loadFromNode(const NodeModel &node);
    void blockSignalsRecursive(bool block);
    void updateRoiInfoLabel(const QString &key, const QJsonObject &roiJson);
    NodeModel currentNode() const;
    QJsonObject collectDynamicParameters() const;
    QJsonObject collectParameterBindings() const;
    void pushNodeChange(const NodeModel &oldNode, const NodeModel &newNode);
    void restoreConstantBinding(const QString &key);

    Document *m_document{nullptr};
    QUndoStack *m_undoStack{nullptr};
    const Selt::ProjectVariableStore *m_variables{nullptr};
    QString m_nodeId;
    int m_applyingChanges{0};

    QLabel *m_typeLabel{nullptr};
    QLabel *m_categoryLabel{nullptr};
    QLabel *m_descLabel{nullptr};
    QLabel *m_statusLabel{nullptr};
    QLineEdit *m_textEdit{nullptr};
    QCheckBox *m_lockedCheck{nullptr};
    QWidget *m_commonWidget{nullptr};
    QLineEdit *m_paramFilterEdit{nullptr};
    QCheckBox *m_boundOnlyCheck{nullptr};
    QScrollArea *m_dynamicScroll{nullptr};
    QWidget *m_dynamicWidget{nullptr};
    QVBoxLayout *m_dynamicLayout{nullptr};
    QHash<QString, QWidget *> m_paramWidgets;
    QHash<QString, BindingEditor *> m_bindingEditors;
    QHash<QString, QGroupBox *> m_paramGroups;
};

#endif // PROPERTYPANEL_H
