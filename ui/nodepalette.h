#ifndef NODEPALETTE_H
#define NODEPALETTE_H

#include <QIcon>
#include <QString>
#include <QWidget>
#include <QtGlobal>

class QLineEdit;
class QComboBox;
class QTreeView;
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
    /// Mark「最近使用」dirty; rebuilds on next filter/mode change (not on create).
    void refreshPreferencesSections();
    /// Unified create entry (deduped); used by list mode, icon tiles, and tests.
    void requestCreate(const QString &type);
    /// Filter toolbox by port data type (empty = all). Used during connection drag.
    void setDataTypeFilter(const QString &dataTypeId);

signals:
    /// Single click: preview only.
    void nodeTypeSelected(const QString &type);
    /// Double-click / Enter: create once at canvas center.
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
    void showEmptyIconState(const QString &message);
    void bindOperatorTile(QToolButton *tile);
    QToolButton *makeOperatorTile(const QString &type,
                                  const QString &displayName,
                                  const QIcon &icon,
                                  const QString &toolTip,
                                  QWidget *parent);

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
    bool m_prefsDirty{false};
    QString m_pendingTileType;
    qint64 m_pendingTileClickMs{0};
    QString m_lastCreateType;
    qint64 m_lastCreateMs{0};
};

#endif // NODEPALETTE_H
