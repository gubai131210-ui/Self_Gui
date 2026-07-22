#include "nodepalette.h"

#include "ui/nodecatalogmodel.h"
#include "ui/theme/iconprovider.h"
#include "ui/theme/operatorpreferences.h"
#include "ui/theme/uistyle.h"
#include "vision/registry/visionnoderegistry.h"
#include "vision/workflow/workflowtemplates.h"
#if SELT_HAS_OPENCV
#include "vision/algorithms/recognitionalgorithms.h"
#include "vision/data/datatype.h"
#include "vision/registry/visionnodeids.h"
#endif

#include <QAbstractItemView>
#include <QButtonGroup>
#include <QComboBox>
#include <QDateTime>
#include <QDrag>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QStackedWidget>
#include <QToolBox>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>

namespace {

constexpr char kNodeTypeMime[] = "application/x-selt-node-type";

class OperatorIconList : public QListWidget
{
public:
    explicit OperatorIconList(QWidget *parent = nullptr)
        : QListWidget(parent)
    {
        setViewMode(QListView::IconMode);
        setResizeMode(QListView::Adjust);
        setMovement(QListView::Static);
        setWrapping(true);
        setWordWrap(true);
        setSpacing(6);
        setUniformItemSizes(true);
        setIconSize(QSize(Selt::UiStyle::toolboxIconSize, Selt::UiStyle::toolboxIconSize));
        setGridSize(QSize(92, 78));
        setDragEnabled(true);
        setDragDropMode(QAbstractItemView::DragOnly);
        setDefaultDropAction(Qt::CopyAction);
        setSelectionMode(QAbstractItemView::SingleSelection);
    }

protected:
    void startDrag(Qt::DropActions) override
    {
        QListWidgetItem *item = currentItem();
        if (!item)
            return;
        const QString type = item->data(Selt::NodeCatalogModel::TypeRole).toString();
        if (type.isEmpty())
            return;
        auto *mime = new QMimeData;
        mime->setData(QString::fromLatin1(kNodeTypeMime), type.toUtf8());
        auto *drag = new QDrag(this);
        drag->setMimeData(mime);
        const QPixmap preview = item->icon().pixmap(Selt::UiStyle::toolboxIconSize,
                                                    Selt::UiStyle::toolboxIconSize);
        if (!preview.isNull()) {
            drag->setPixmap(preview);
            drag->setHotSpot(QPoint(preview.width() / 2, preview.height() / 2));
        }
        drag->exec(Qt::CopyAction);
    }
};

} // namespace

NodePalette::NodePalette(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(Selt::UiStyle::panelMargin,
                               Selt::UiStyle::panelMargin,
                               Selt::UiStyle::panelMargin,
                               Selt::UiStyle::panelMargin);
    layout->setSpacing(Selt::UiStyle::compactSpacing);

    auto *title = new QLabel(QStringLiteral("工具箱"), this);
    title->setObjectName(QStringLiteral("PanelTitle"));
    QFont titleFont = title->font();
    titleFont.setBold(true);
    title->setFont(titleFont);

    auto *modeRow = new QHBoxLayout();
    m_iconModeBtn = new QToolButton(this);
    m_iconModeBtn->setText(QStringLiteral("图标"));
    m_iconModeBtn->setCheckable(true);
    m_iconModeBtn->setChecked(true);
    m_iconModeBtn->setToolTip(QStringLiteral("图标网格模式"));
    m_listModeBtn = new QToolButton(this);
    m_listModeBtn->setText(QStringLiteral("列表"));
    m_listModeBtn->setCheckable(true);
    m_listModeBtn->setToolTip(QStringLiteral("树列表模式"));
    auto *modeGroup = new QButtonGroup(this);
    modeGroup->setExclusive(true);
    modeGroup->addButton(m_iconModeBtn);
    modeGroup->addButton(m_listModeBtn);
    modeRow->addWidget(title, 1);
    modeRow->addWidget(m_iconModeBtn);
    modeRow->addWidget(m_listModeBtn);

    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText(QStringLiteral("搜索模块..."));
    m_filterEdit->setClearButtonEnabled(true);

    m_typeFilter = new QComboBox(this);
    m_typeFilter->setToolTip(QStringLiteral("按端口数据类型筛选算子"));
    m_typeFilter->addItem(QStringLiteral("全部类型"), QString());
#if SELT_HAS_OPENCV
    const QList<QPair<QString, Selt::DataTypeId>> typeItems = {
        {QStringLiteral("图像"), Selt::DataTypeId::Image},
        {QStringLiteral("区域"), Selt::DataTypeId::Region},
        {QStringLiteral("ROI"), Selt::DataTypeId::Roi},
        {QStringLiteral("轮廓"), Selt::DataTypeId::Contour},
        {QStringLiteral("点"), Selt::DataTypeId::Point2D},
        {QStringLiteral("直线"), Selt::DataTypeId::Line},
        {QStringLiteral("圆"), Selt::DataTypeId::Circle},
        {QStringLiteral("实数"), Selt::DataTypeId::Real},
        {QStringLiteral("整数"), Selt::DataTypeId::Int},
        {QStringLiteral("布尔"), Selt::DataTypeId::Bool},
        {QStringLiteral("测量"), Selt::DataTypeId::Measurement},
        {QStringLiteral("叠加层"), Selt::DataTypeId::Overlay},
        {QStringLiteral("字符串"), Selt::DataTypeId::String},
        {QStringLiteral("字节"), Selt::DataTypeId::ByteArray},
    };
    for (const auto &item : typeItems)
        m_typeFilter->addItem(item.first, Selt::dataTypeIdToString(item.second));
#endif

    m_model = new Selt::NodeCatalogModel(this);
    m_proxy = new Selt::NodeCatalogFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);

    m_stack = new QStackedWidget(this);

    m_toolBox = new QToolBox(m_stack);
    m_stack->addWidget(m_toolBox);

    m_tree = new QTreeView(m_stack);
    m_tree->setModel(m_proxy);
    m_tree->setHeaderHidden(true);
    m_tree->setAnimated(true);
    m_tree->setIndentation(14);
    m_tree->setUniformRowHeights(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setExpandsOnDoubleClick(false);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setDragEnabled(true);
    m_tree->setDragDropMode(QAbstractItemView::DragOnly);
    m_tree->setDefaultDropAction(Qt::CopyAction);
    m_tree->setIconSize(QSize(Selt::UiStyle::dockIconSize, Selt::UiStyle::dockIconSize));
    m_tree->expandToDepth(0);
    m_stack->addWidget(m_tree);

    layout->addLayout(modeRow);
    layout->addWidget(m_filterEdit);
    layout->addWidget(m_typeFilter);
    layout->addWidget(m_stack, 1);

    rebuildIconToolbox();
    m_stack->setCurrentIndex(0);

    connect(m_filterEdit, &QLineEdit::textChanged, this, &NodePalette::onFilterChanged);
    connect(m_typeFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &NodePalette::onTypeFilterChanged);
    // 单击只选中/预览；双击创建一次。不再同时连接 activated+doubleClicked，避免双击创建两个节点。
    connect(m_tree, &QTreeView::clicked, this, &NodePalette::onTreeClicked);
    connect(m_tree, &QTreeView::doubleClicked, this, &NodePalette::onTreeCreateRequested);
    connect(m_tree, &QTreeView::activated, this, [this](const QModelIndex &index) {
        // activated 在双击和 Enter 时都会触发；双击已由 doubleClicked 处理。
        // 仅当焦点来自键盘（无近期双击）时创建，由短抑制窗口去重。
        onTreeCreateRequested(index);
    });
    connect(m_iconModeBtn, &QToolButton::toggled, this, [this](bool checked) {
        if (checked) {
            m_userPreferIconMode = true;
            m_narrowForcedList = false;
            setIconMode(true);
        }
    });
    connect(m_listModeBtn, &QToolButton::toggled, this, [this](bool checked) {
        if (checked) {
            m_userPreferIconMode = false;
            m_narrowForcedList = false;
            setIconMode(false);
        }
    });
}

void NodePalette::reloadCatalog()
{
    if (m_model)
        m_model->reload();
    rebuildIconToolbox();
    if (m_tree)
        m_tree->expandToDepth(0);
}

void NodePalette::requestCreate(const QString &type)
{
    emitCreateIfValid(type);
}

void NodePalette::onFilterChanged(const QString &text)
{
    m_proxy->setFilterText(text);
    rebuildIconToolbox();
    if (!text.trimmed().isEmpty())
        m_tree->expandAll();
    else
        m_tree->expandToDepth(0);
}

void NodePalette::onTypeFilterChanged(int index)
{
    if (!m_proxy || !m_typeFilter)
        return;
    const QString typeId = m_typeFilter->itemData(index).toString();
    m_proxy->setFilterDataType(typeId);
    rebuildIconToolbox();
    if (!typeId.isEmpty() || (m_filterEdit && !m_filterEdit->text().trimmed().isEmpty()))
        m_tree->expandAll();
    else
        m_tree->expandToDepth(0);
}

void NodePalette::setDataTypeFilter(const QString &dataTypeId)
{
    if (!m_typeFilter)
        return;
    const int index = m_typeFilter->findData(dataTypeId);
    if (index >= 0)
        m_typeFilter->setCurrentIndex(index);
    else if (dataTypeId.isEmpty())
        m_typeFilter->setCurrentIndex(0);
}

void NodePalette::setIconMode(bool enabled)
{
    m_iconMode = enabled;
    if (m_iconModeBtn)
        m_iconModeBtn->blockSignals(true);
    if (m_listModeBtn)
        m_listModeBtn->blockSignals(true);
    if (m_iconModeBtn)
        m_iconModeBtn->setChecked(enabled);
    if (m_listModeBtn)
        m_listModeBtn->setChecked(!enabled);
    if (m_iconModeBtn)
        m_iconModeBtn->blockSignals(false);
    if (m_listModeBtn)
        m_listModeBtn->blockSignals(false);
    if (m_stack)
        m_stack->setCurrentIndex(enabled ? 0 : 1);
}

void NodePalette::bindIconListActivation(QListWidget *list)
{
    if (!list)
        return;
    // 单击：选中预览；双击：创建。不连接 itemActivated，避免与双击叠加。
    connect(list, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        if (!item)
            return;
        const QString type = item->data(Selt::NodeCatalogModel::TypeRole).toString();
        if (!type.isEmpty())
            emit nodeTypeSelected(type);
    });
    connect(list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        if (!item)
            return;
        emitCreateIfValid(item->data(Selt::NodeCatalogModel::TypeRole).toString());
    });
    list->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(list, &QWidget::customContextMenuRequested, this, [this, list](const QPoint &pos) {
        QListWidgetItem *item = list->itemAt(pos);
        if (!item)
            return;
        const QString type = item->data(Selt::NodeCatalogModel::TypeRole).toString();
        if (type.isEmpty())
            return;
        QMenu menu(list);
        const bool fav = Selt::OperatorPreferences::isFavorite(type);
        menu.addAction(fav ? QStringLiteral("取消收藏") : QStringLiteral("加入收藏"), this, [this, type]() {
            Selt::OperatorPreferences::toggleFavorite(type);
            reloadCatalog();
        });
        menu.exec(list->mapToGlobal(pos));
    });
}

void NodePalette::showEmptyIconState(const QString &message)
{
    auto *page = new QWidget(m_toolBox);
    auto *pageLayout = new QVBoxLayout(page);
    m_emptyLabel = new QLabel(message, page);
    m_emptyLabel->setWordWrap(true);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setObjectName(QStringLiteral("ToolboxEmptyLabel"));
    pageLayout->addStretch(1);
    pageLayout->addWidget(m_emptyLabel);
    pageLayout->addStretch(1);
    m_toolBox->addItem(page, QStringLiteral("搜索结果"));
}

void NodePalette::rebuildIconToolbox()
{
    if (!m_toolBox || !m_proxy)
        return;

    m_emptyLabel = nullptr;
    while (m_toolBox->count() > 0) {
        QWidget *page = m_toolBox->widget(0);
        m_toolBox->removeItem(0);
        delete page;
    }

    auto addTypeList = [&](const QString &title, const QStringList &types, const QString &iconKey) {
        if (types.isEmpty())
            return;
        auto *list = new OperatorIconList(m_toolBox);
        for (const QString &type : types) {
#if SELT_HAS_OPENCV
            const ModuleDescriptor desc = VisionNodeRegistry::descriptor(type);
            if (desc.typeId.isEmpty())
                continue;
            auto *item = new QListWidgetItem(
                Selt::IconProvider::visionIcon(desc.iconKey.isEmpty() ? desc.category : desc.iconKey, 24),
                desc.displayName.isEmpty() ? type : desc.displayName,
                list);
            item->setData(Selt::NodeCatalogModel::TypeRole, type);
            QString tip = desc.helpText.isEmpty() ? desc.description : desc.helpText;
            if (type == VisionNodeIds::barcodeDecode()) {
                tip += QStringLiteral("\nQR: %1\nBarcode: %2")
                           .arg(Selt::RecognitionCapability::qrStatusText(),
                                Selt::RecognitionCapability::barcodeStatusText());
            } else if (type == VisionNodeIds::ocr()) {
                tip += QStringLiteral("\nOCR: %1").arg(Selt::RecognitionCapability::ocrStatusText());
            }
            item->setToolTip(tip);
#else
            auto *item = new QListWidgetItem(type, list);
            item->setData(Selt::NodeCatalogModel::TypeRole, type);
#endif
            item->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
        }
        if (list->count() == 0) {
            delete list;
            return;
        }
        bindIconListActivation(list);
        m_toolBox->addItem(list, Selt::IconProvider::visionIcon(iconKey, 16), title);
    };

    addTypeList(QStringLiteral("收藏"), Selt::OperatorPreferences::favorites(), QStringLiteral("模板"));
    addTypeList(QStringLiteral("最近使用"), Selt::OperatorPreferences::recent(), QStringLiteral("输入"));

    const int categoryCount = m_proxy->rowCount(QModelIndex());
    int operatorCount = 0;
    for (int c = 0; c < categoryCount; ++c) {
        const QModelIndex categoryIndex = m_proxy->index(c, 0);
        const QString categoryName = categoryIndex.data(Qt::DisplayRole).toString();
        auto *list = new OperatorIconList(m_toolBox);

        const int childCount = m_proxy->rowCount(categoryIndex);
        for (int i = 0; i < childCount; ++i) {
            const QModelIndex child = m_proxy->index(i, 0, categoryIndex);
            const QString type = child.data(Selt::NodeCatalogModel::TypeRole).toString();
            if (type.isEmpty())
                continue;
            auto *item = new QListWidgetItem(child.data(Qt::DecorationRole).value<QIcon>(),
                                             child.data(Qt::DisplayRole).toString(),
                                             list);
            item->setData(Selt::NodeCatalogModel::TypeRole, type);
            item->setToolTip(child.data(Qt::ToolTipRole).toString());
            item->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
            ++operatorCount;
        }

        if (list->count() == 0) {
            delete list;
            continue;
        }

        bindIconListActivation(list);
        list->setProperty("seltCategory", categoryName);
        const int stageRank = Selt::WorkflowTemplates::categoryStageRank(categoryName);
        const QString stageTitle = stageRank < 100
            ? QStringLiteral("%1 · %2").arg(stageRank + 1).arg(categoryName)
            : categoryName;
        m_toolBox->addItem(list, Selt::IconProvider::visionIcon(categoryName, 16), stageTitle);
    }

    if (operatorCount == 0) {
        const QString filter = m_filterEdit ? m_filterEdit->text().trimmed() : QString();
        if (filter.isEmpty())
            showEmptyIconState(QStringLiteral("暂无可用算子。"));
        else
            showEmptyIconState(QStringLiteral("无匹配算子：%1").arg(filter));
    }

    updateIconColumns();
}

void NodePalette::updateIconColumns()
{
    if (!m_toolBox)
        return;
    const int width = qMax(140, this->width() - 24);
    const int columns = qBound(1, width / 96, 3);
    const int gridW = qMax(72, width / columns - 4);
    for (int i = 0; i < m_toolBox->count(); ++i) {
        if (auto *list = qobject_cast<QListWidget *>(m_toolBox->widget(i)))
            list->setGridSize(QSize(gridW, 78));
    }
}

void NodePalette::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateIconColumns();

    constexpr int kForceListWidth = 220;
    constexpr int kRestoreIconWidth = 240;
    if (width() < kForceListWidth) {
        if (m_iconMode) {
            m_narrowForcedList = true;
            setIconMode(false);
        }
    } else if (width() >= kRestoreIconWidth && m_narrowForcedList && m_userPreferIconMode) {
        m_narrowForcedList = false;
        setIconMode(true);
    }
}

void NodePalette::onTreeClicked(const QModelIndex &index)
{
    const QString type = typeAt(index);
    if (!type.isEmpty())
        emit nodeTypeSelected(type);
}

void NodePalette::onTreeCreateRequested(const QModelIndex &index)
{
    emitCreateIfValid(typeAt(index));
}

void NodePalette::emitCreateIfValid(const QString &type)
{
    if (type.isEmpty())
        return;
    // 同一 UI 事件内 activated 与 doubleClicked 可能各发一次：短窗口内同 type 只创建一次。
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (type == m_lastCreateType && (now - m_lastCreateMs) < 80)
        return;
    m_lastCreateType = type;
    m_lastCreateMs = now;
    emit nodeTypeActivated(type);
}

QString NodePalette::typeAt(const QModelIndex &proxyIndex) const
{
    if (!proxyIndex.isValid() || !m_proxy)
        return {};
    return m_proxy->data(proxyIndex, Selt::NodeCatalogModel::TypeRole).toString();
}
