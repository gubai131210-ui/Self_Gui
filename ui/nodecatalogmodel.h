#ifndef NODECATALOGMODEL_H
#define NODECATALOGMODEL_H

#include <QAbstractItemModel>
#include <QMimeData>
#include <QSortFilterProxyModel>
#include <QStringList>
#include <QVector>

namespace Selt {

struct NodeCatalogItem
{
    bool isCategory{false};
    QString category;
    QString type;
    QString displayName;
    QString description;
    QString iconKey;
};

class NodeCatalogModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    enum Roles {
        TypeRole = Qt::UserRole + 1,
        CategoryRole,
        DescriptionRole,
        IsCategoryRole
    };

    explicit NodeCatalogModel(QObject *parent = nullptr);

    void reload();

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    QStringList mimeTypes() const override;

private:
    struct CategoryNode {
        QString name;
        QVector<NodeCatalogItem> children;
    };

    QVector<CategoryNode> m_categories;
};

class NodeCatalogFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit NodeCatalogFilterProxyModel(QObject *parent = nullptr);
    void setFilterText(const QString &text);
    /// 按端口数据类型筛选；空表示不过滤。匹配算子任意输入或输出端口类型。
    void setFilterDataType(const QString &dataTypeId);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    bool matchesDataType(const QString &typeId) const;

    QString m_filter;
    QString m_dataTypeFilter;
};

} // namespace Selt

#endif // NODECATALOGMODEL_H
