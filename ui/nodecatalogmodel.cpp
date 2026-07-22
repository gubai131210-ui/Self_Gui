#include "ui/nodecatalogmodel.h"

#include "ui/theme/iconprovider.h"
#if SELT_HAS_OPENCV
#include "vision/algorithms/recognitionalgorithms.h"
#include "vision/data/datatype.h"
#include "vision/model/moduledescriptor.h"
#include "vision/registry/visionnodeids.h"
#include "vision/registry/visionnoderegistry.h"
#endif

#include <QMimeData>

namespace Selt {

static const char *kNodeTypeMime = "application/x-selt-node-type";

NodeCatalogModel::NodeCatalogModel(QObject *parent)
    : QAbstractItemModel(parent)
{
    reload();
}

void NodeCatalogModel::reload()
{
    beginResetModel();
    m_categories.clear();
#if SELT_HAS_OPENCV
    for (const QString &category : VisionNodeRegistry::categories()) {
        CategoryNode node;
        node.name = category;
        for (const QString &type : VisionNodeRegistry::typesInCategory(category)) {
            NodeCatalogItem item;
            item.isCategory = false;
            item.category = category;
            item.type = type;
            item.displayName = VisionNodeRegistry::displayName(type);
            const ModuleDescriptor desc = VisionNodeRegistry::descriptor(type);
            item.description = desc.helpText.isEmpty() ? desc.description : desc.helpText;
            if (type == VisionNodeIds::barcodeDecode()) {
                item.description += QStringLiteral("\nQR: %1\nBarcode: %2")
                                        .arg(RecognitionCapability::qrStatusText(),
                                             RecognitionCapability::barcodeStatusText());
            } else if (type == VisionNodeIds::ocr()) {
                item.description += QStringLiteral("\nOCR: %1")
                                        .arg(RecognitionCapability::ocrStatusText());
            }
            item.iconKey = desc.iconKey.isEmpty() ? category : desc.iconKey;
            node.children.append(item);
        }
        if (!node.children.isEmpty())
            m_categories.append(node);
    }
#endif
    endResetModel();
}

QModelIndex NodeCatalogModel::index(int row, int column, const QModelIndex &parent) const
{
    if (column != 0 || row < 0)
        return {};
    if (!parent.isValid()) {
        if (row >= m_categories.size())
            return {};
        return createIndex(row, column, quintptr(0));
    }
    const int categoryRow = parent.row();
    if (categoryRow < 0 || categoryRow >= m_categories.size())
        return {};
    if (row >= m_categories.at(categoryRow).children.size())
        return {};
    return createIndex(row, column, quintptr(categoryRow + 1));
}

QModelIndex NodeCatalogModel::parent(const QModelIndex &child) const
{
    if (!child.isValid())
        return {};
    const quintptr id = child.internalId();
    if (id == 0)
        return {};
    return createIndex(int(id) - 1, 0, quintptr(0));
}

int NodeCatalogModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return m_categories.size();
    if (parent.internalId() != 0)
        return 0;
    const int categoryRow = parent.row();
    if (categoryRow < 0 || categoryRow >= m_categories.size())
        return 0;
    return m_categories.at(categoryRow).children.size();
}

int NodeCatalogModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 1;
}

QVariant NodeCatalogModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return {};

    if (index.internalId() == 0) {
        const CategoryNode &cat = m_categories.at(index.row());
        switch (role) {
        case Qt::DisplayRole:
            return cat.name;
        case Qt::DecorationRole:
            return IconProvider::visionIcon(cat.name, 16);
        case IsCategoryRole:
            return true;
        case CategoryRole:
            return cat.name;
        default:
            return {};
        }
    }

    const CategoryNode &cat = m_categories.at(int(index.internalId()) - 1);
    const NodeCatalogItem &item = cat.children.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
        return item.displayName;
    case Qt::ToolTipRole:
        return item.description.isEmpty() ? item.displayName : item.description;
    case Qt::DecorationRole:
        return IconProvider::visionIcon(
            item.iconKey.isEmpty() ? item.category : item.iconKey, 16);
    case TypeRole:
        return item.type;
    case CategoryRole:
        return item.category;
    case DescriptionRole:
        return item.description;
    case IsCategoryRole:
        return false;
    default:
        return {};
    }
}

Qt::ItemFlags NodeCatalogModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    if (index.internalId() == 0)
        return Qt::ItemIsEnabled;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled;
}

QStringList NodeCatalogModel::mimeTypes() const
{
    return {QString::fromLatin1(kNodeTypeMime)};
}

QMimeData *NodeCatalogModel::mimeData(const QModelIndexList &indexes) const
{
    if (indexes.isEmpty())
        return nullptr;
    const QModelIndex index = indexes.first();
    const QString type = data(index, TypeRole).toString();
    if (type.isEmpty())
        return nullptr;
    auto *mime = new QMimeData;
    mime->setData(QString::fromLatin1(kNodeTypeMime), type.toUtf8());
    return mime;
}

NodeCatalogFilterProxyModel::NodeCatalogFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
}

void NodeCatalogFilterProxyModel::setFilterText(const QString &text)
{
    m_filter = text.trimmed();
    invalidateFilter();
}

void NodeCatalogFilterProxyModel::setFilterDataType(const QString &dataTypeId)
{
    m_dataTypeFilter = dataTypeId.trimmed().toLower();
    invalidateFilter();
}

bool NodeCatalogFilterProxyModel::matchesDataType(const QString &typeId) const
{
    if (m_dataTypeFilter.isEmpty() || typeId.isEmpty())
        return m_dataTypeFilter.isEmpty();
#if SELT_HAS_OPENCV
    const ModuleDescriptor desc = VisionNodeRegistry::descriptor(typeId);
    for (const ModulePortDef &port : desc.ports) {
        if (Selt::dataTypeIdToString(port.dataType).toLower() == m_dataTypeFilter)
            return true;
    }
#else
    Q_UNUSED(typeId);
#endif
    return false;
}

bool NodeCatalogFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if (m_filter.isEmpty() && m_dataTypeFilter.isEmpty())
        return true;

    const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    if (!index.isValid())
        return false;

    if (!sourceParent.isValid()) {
        // Category: accept if any child matches.
        const int childCount = sourceModel()->rowCount(index);
        for (int i = 0; i < childCount; ++i) {
            if (filterAcceptsRow(i, index))
                return true;
        }
        if (!m_filter.isEmpty()
            && sourceModel()->data(index, Qt::DisplayRole).toString()
                   .contains(m_filter, Qt::CaseInsensitive))
            return m_dataTypeFilter.isEmpty();
        return false;
    }

    const QString name = sourceModel()->data(index, Qt::DisplayRole).toString();
    const QString type = sourceModel()->data(index, NodeCatalogModel::TypeRole).toString();
    const QString category = sourceModel()->data(index, NodeCatalogModel::CategoryRole).toString();
    const QString desc = sourceModel()->data(index, NodeCatalogModel::DescriptionRole).toString();
    const bool textOk = m_filter.isEmpty()
        || name.contains(m_filter, Qt::CaseInsensitive)
        || type.contains(m_filter, Qt::CaseInsensitive)
        || category.contains(m_filter, Qt::CaseInsensitive)
        || desc.contains(m_filter, Qt::CaseInsensitive);
    const bool typeOk = matchesDataType(type);
    return textOk && typeOk;
}

} // namespace Selt
