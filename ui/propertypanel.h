#ifndef PROPERTYPANEL_H
#define PROPERTYPANEL_H

#include "core/model/nodemodel.h"
#include "graphics/items/nodegraphicsitem.h"

#include <QHash>
#include <QImage>
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
class QCheckBox;
class QScrollArea;
class QVBoxLayout;
class BindingEditor;
class InspectorHeader;
class CollapsibleSection;
class CompactParamRow;

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
    void setModuleRunStatus(NodeRunVisualStatus status, const QString &text);
    void setResultThumbnail(const QImage &image);
    void clearResultThumbnail();
    void applyRoiParameter(const QString &key, const QJsonObject &roiJson);
    /// 参数面板自身正在写回 Document 时为 true，避免 nodeUpdated 同步重建导致崩溃
    bool isApplyingChanges() const { return m_applyingChanges; }
    QString currentNodeId() const { return m_nodeId; }

signals:
    void runNodeRequested(const QString &nodeId);
    void copyParamsRequested(const QString &nodeId);

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
    void copyParametersToClipboard();
#if SELT_HAS_OPENCV
    CollapsibleSection *ensureSection(const QString &title, bool defaultExpanded);
#endif

    Document *m_document{nullptr};
    QUndoStack *m_undoStack{nullptr};
    const Selt::ProjectVariableStore *m_variables{nullptr};
    QString m_nodeId;
    int m_applyingChanges{0};

    InspectorHeader *m_header{nullptr};
    QLabel *m_thumbnail{nullptr};
    QLineEdit *m_textEdit{nullptr};
    QCheckBox *m_lockedCheck{nullptr};
    QLineEdit *m_paramFilterEdit{nullptr};
    QCheckBox *m_boundOnlyCheck{nullptr};
    QScrollArea *m_dynamicScroll{nullptr};
    QWidget *m_dynamicWidget{nullptr};
    QVBoxLayout *m_dynamicLayout{nullptr};
    QHash<QString, QWidget *> m_paramWidgets;
    QHash<QString, BindingEditor *> m_bindingEditors;
    QHash<QString, CompactParamRow *> m_paramRows;
    QHash<QString, CollapsibleSection *> m_sections;
};

#endif // PROPERTYPANEL_H
