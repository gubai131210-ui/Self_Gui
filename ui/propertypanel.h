#ifndef PROPERTYPANEL_H
#define PROPERTYPANEL_H

#include "core/model/nodemodel.h"

#include <QWidget>

class Document;
class QUndoStack;
class QLineEdit;
class QPlainTextEdit;
class QDoubleSpinBox;
class QSpinBox;
class QLabel;
class QCheckBox;
class QPushButton;

class PropertyPanel : public QWidget
{
    Q_OBJECT

public:
    explicit PropertyPanel(QWidget *parent = nullptr);

    void setDocument(Document *document);
    void setUndoStack(QUndoStack *undoStack);
    void setSelectedNode(const QString &nodeId);

private slots:
    void applyText();
    void applyNote();
    void applyUrl();
    void applyGeometry();
    void applyStyle();
    void chooseFillColor();
    void chooseBorderColor();
    void chooseTextColor();
    void applyLocked();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void rebuildUi();
    void loadFromNode(const NodeModel &node);
    void blockSignalsRecursive(bool block);
    NodeModel currentNode() const;

    Document *m_document{nullptr};
    QUndoStack *m_undoStack{nullptr};
    QString m_nodeId;

    QLabel *m_typeLabel{nullptr};
    QLineEdit *m_textEdit{nullptr};
    QPlainTextEdit *m_noteEdit{nullptr};
    QLineEdit *m_urlEdit{nullptr};
    QDoubleSpinBox *m_xSpin{nullptr};
    QDoubleSpinBox *m_ySpin{nullptr};
    QDoubleSpinBox *m_wSpin{nullptr};
    QDoubleSpinBox *m_hSpin{nullptr};
    QSpinBox *m_fontSpin{nullptr};
    QDoubleSpinBox *m_borderSpin{nullptr};
    QDoubleSpinBox *m_radiusSpin{nullptr};
    QPushButton *m_fillBtn{nullptr};
    QPushButton *m_borderBtn{nullptr};
    QPushButton *m_textColorBtn{nullptr};
    QCheckBox *m_lockedCheck{nullptr};
    QColor m_fillColor;
    QColor m_borderColor;
    QColor m_textColor;
};

#endif // PROPERTYPANEL_H
