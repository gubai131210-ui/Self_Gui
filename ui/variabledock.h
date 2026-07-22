#pragma once

#include <QWidget>

class QComboBox;
class QLineEdit;
class QTableWidget;
class QPushButton;
class QLabel;
class VisionProject;

namespace Selt {
class ProjectVariableStore;
}

/// 变量作用域管理：全局 / 工程 / 当前流程，支持增删与刷新。
class VariableDock : public QWidget
{
    Q_OBJECT
public:
    explicit VariableDock(QWidget *parent = nullptr);

    void setProject(VisionProject *project);
    void reload();

signals:
    void diagnosticsReady(const QStringList &lines);

private slots:
    void onAdd();
    void onRemove();
    void onScopeChanged();

private:
    Selt::ProjectVariableStore *activeStore() const;
    QString scopeLabel() const;

    VisionProject *m_project{nullptr};
    QComboBox *m_scopeCombo{nullptr};
    QTableWidget *m_table{nullptr};
    QLineEdit *m_nameEdit{nullptr};
    QComboBox *m_typeCombo{nullptr};
    QLineEdit *m_valueEdit{nullptr};
    QPushButton *m_addButton{nullptr};
    QPushButton *m_removeButton{nullptr};
    QLabel *m_hintLabel{nullptr};
};
