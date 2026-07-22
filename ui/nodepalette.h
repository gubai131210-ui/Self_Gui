#ifndef NODEPALETTE_H
#define NODEPALETTE_H

#include <QString>
#include <QWidget>
#include <QtGlobal>

class QLineEdit;
class QComboBox;
class QTreeView;
class QListWidget;
class QToolBox;
class QStackedWidget;
class QToolButton;
class QLabel;
class QResizeEvent;
class QModelIndex;

namespace Selt {
class NodeCatalogModel;
class NodeCatalogFilterProxyModel;
}

class NodePalette : public QWidget
{
    Q_OBJECT

public:
    explicit NodePalette(QWidget *parent = nullptr);
    void reloadCatalog();
    /// 统一创建入口（含短窗口去重）；供键盘 Enter 与测试调用。
    void requestCreate(const QString &type);
    /// 按数据类型筛选工具箱（空字符串恢复全部）。连线拖拽时可联动。
    void setDataTypeFilter(const QString &dataTypeId);

signals:
    /// 单击选中：仅预览/高亮，不创建节点。
    void nodeTypeSelected(const QString &type);
    /// 双击或 Enter：在画布视口中心创建一次节点。
    void nodeTypeActivated(const QString &type);

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onFilterChanged(const QString &text);
    void onTypeFilterChanged(int index);
    void onTreeClicked(const QModelIndex &index);
    void onTreeCreateRequested(const QModelIndex &index);
    void setIconMode(bool enabled);

private:
    QString typeAt(const QModelIndex &proxyIndex) const;
    void emitCreateIfValid(const QString &type);
    void rebuildIconToolbox();
    void updateIconColumns();
    void bindIconListActivation(QListWidget *list);
    void showEmptyIconState(const QString &message);

    QLineEdit *m_filterEdit{nullptr};
    QComboBox *m_typeFilter{nullptr};
    QToolButton *m_iconModeBtn{nullptr};
    QToolButton *m_listModeBtn{nullptr};
    QStackedWidget *m_stack{nullptr};
    QTreeView *m_tree{nullptr};
    QToolBox *m_toolBox{nullptr};
    QLabel *m_emptyLabel{nullptr};
    Selt::NodeCatalogModel *m_model{nullptr};
    Selt::NodeCatalogFilterProxyModel *m_proxy{nullptr};
    bool m_iconMode{true};
    bool m_userPreferIconMode{true};
    bool m_narrowForcedList{false};
    QString m_lastCreateType;
    qint64 m_lastCreateMs{0};
};

#endif // NODEPALETTE_H
