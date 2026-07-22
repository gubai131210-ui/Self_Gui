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
#include <QApplication>
#include <QButtonGroup>
#include <QComboBox>
#include <QDateTime>
#include <QDrag>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPointer>
#include <QResizeEvent>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTimer>
#include <QToolBox>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>

namespace {

constexpr int kTileWidth = 92;
constexpr int kTileHeight = 78;

/// Icon-mode tile. It intentionally uses only QToolButton's native mouse handling.
/// The owner detects a double click from clicked() events, avoiding custom mouse
/// event re-entry in Qt's button state machine.
class OperatorTileButton : public QToolButton
{
public:
    QString typeId;

    explicit OperatorTileButton(QWidget *parent = nullptr)
        : QToolButton(parent)
    {
        setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        setIconSize(QSize(Selt::UiStyle::toolboxIconSize, Selt::UiStyle::toolboxIconSize));
        setFixedSize(kTileWidth, kTileHeight);
        setCheckable(false);
        setAutoRaise(true);
        setFocusPolicy(Qt::StrongFocus);
        setCursor(Qt::PointingHandCursor);
    }

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        m_pressPos = event->position().toPoint();
        m_dragStarted = false;
        QToolButton::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (!(event->buttons() & Qt::LeftButton)
            || typeId.isEmpty()
            || m_dragStarted
            || (event->position().toPoint() - m_pressPos).manhattanLength()
                   < QApplication::startDragDistance()) {
            QToolButton::mouseMoveEvent(event);
            return;
        }

        m_dragStarted = true;
        auto *mime = new QMimeData;
        mime->setData(QStringLiteral("application/x-selt-node-type"), typeId.toUtf8());
        mime->setText(text());

        auto *drag = new QDrag(this);
        drag->setMimeData(mime);
        const QPixmap preview = icon().pixmap(Selt::UiStyle::toolboxIconSize,
                                              Selt::UiStyle::toolboxIconSize);
        if (!preview.isNull()) {
            drag->setPixmap(preview);
            drag->setHotSpot(QPoint(preview.width() / 2, preview.height() / 2));
        }
        drag->exec(Qt::CopyAction);
        m_dragStarted = false;
        event->accept();
    }

private:
    QPoint m_pressPos;
    bool m_dragStarted{false};
};

QScrollArea *makeIconPage(QWidget *parent, QGridLayout **outGrid)
{
    auto *scroll = new QScrollArea(parent);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *host = new QWidget(scroll);
    auto *grid = new QGridLayout(host);
    grid->setContentsMargins(4, 4, 4, 4);
    grid->setSpacing(6);
    grid->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    host->setLayout(grid);
    scroll->setWidget(host);
    *outGrid = grid;
    return scroll;
}

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
    // List mode: click = preview, double-click / Enter = create (shared emitCreateIfValid).
    connect(m_tree, &QTreeView::clicked, this, &NodePalette::onTreeClicked);
    connect(m_tree, &QTreeView::doubleClicked, this, &NodePalette::onTreeCreateRequested);
    connect(m_tree, &QTreeView::activated, this, [this](const QModelIndex &index) {
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

void NodePalette::refreshPreferencesSections()
{
    // Defer icon-page rebuild until idle filter/mode change; create must not destroy tiles.
    m_prefsDirty = true;
}

void NodePalette::requestCreate(const QString &type)
{
    emitCreateIfValid(type);
}

void NodePalette::onFilterChanged(const QString &text)
{
    m_proxy->setFilterText(text);
    m_prefsDirty = false;
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
    m_prefsDirty = false;
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
    if (enabled && m_prefsDirty) {
        m_prefsDirty = false;
        rebuildIconToolbox();
    }
    if (m_stack)
        m_stack->setCurrentIndex(enabled ? 0 : 1);
}

void NodePalette::bindOperatorTile(QToolButton *tileBtn)
{
    auto *tile = static_cast<OperatorTileButton *>(tileBtn);
    if (!tile)
        return;
    connect(tile, &QToolButton::clicked, this, [this, tile]() {
        const QString type = tile->typeId;
        if (type.isEmpty())
            return;

        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        emit nodeTypeSelected(type);

        if (type == m_pendingTileType
            && (now - m_pendingTileClickMs) <= QApplication::doubleClickInterval()) {
            m_pendingTileType.clear();
            m_pendingTileClickMs = 0;
            emitCreateIfValid(type);
            return;
        }

        m_pendingTileType = type;
        m_pendingTileClickMs = now;
        const QString pendingType = type;
        const qint64 pendingAt = now;
        QTimer::singleShot(QApplication::doubleClickInterval(), this,
                           [this, pendingType, pendingAt]() {
                               if (m_pendingTileType == pendingType
                                   && m_pendingTileClickMs == pendingAt) {
                                   m_pendingTileType.clear();
                                   m_pendingTileClickMs = 0;
                               }
                           });
    });
    tile->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(tile, &QWidget::customContextMenuRequested, this, [this, tile](const QPoint &pos) {
        if (tile->typeId.isEmpty())
            return;
        const QString type = tile->typeId;
        QMenu menu(tile);
        const bool fav = Selt::OperatorPreferences::isFavorite(type);
        menu.addAction(fav ? QStringLiteral("取消收藏") : QStringLiteral("加入收藏"), this,
                       [this, type]() {
                           Selt::OperatorPreferences::toggleFavorite(type);
                           m_prefsDirty = false;
                           rebuildIconToolbox();
                       });
        menu.exec(tile->mapToGlobal(pos));
    });
}

QToolButton *NodePalette::makeOperatorTile(const QString &type,
                                           const QString &displayName,
                                           const QIcon &icon,
                                           const QString &toolTip,
                                           QWidget *parent)
{
    auto *tile = new OperatorTileButton(parent);
    tile->typeId = type;
    tile->setText(displayName.isEmpty() ? type : displayName);
    tile->setIcon(icon);
    tile->setToolTip(toolTip);
    bindOperatorTile(tile);
    return tile;
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

    m_prefsDirty = false;
    m_emptyLabel = nullptr;
    const QString currentTitle = m_toolBox->count() > 0
        ? m_toolBox->itemText(m_toolBox->currentIndex())
        : QString();
    while (m_toolBox->count() > 0) {
        QWidget *page = m_toolBox->widget(0);
        m_toolBox->removeItem(0);
        if (page)
            page->deleteLater();
    }

    auto addTypePage = [&](const QString &title, const QStringList &types, const QString &iconKey) {
        if (types.isEmpty())
            return;
        QGridLayout *grid = nullptr;
        QScrollArea *scroll = makeIconPage(m_toolBox, &grid);
        int added = 0;
        for (const QString &type : types) {
#if SELT_HAS_OPENCV
            const ModuleDescriptor desc = VisionNodeRegistry::descriptor(type);
            if (desc.typeId.isEmpty())
                continue;
            QString tip = desc.helpText.isEmpty() ? desc.description : desc.helpText;
            if (type == VisionNodeIds::barcodeDecode()) {
                tip += QStringLiteral("\nQR: %1\nBarcode: %2")
                           .arg(Selt::RecognitionCapability::qrStatusText(),
                                Selt::RecognitionCapability::barcodeStatusText());
            } else if (type == VisionNodeIds::ocr()) {
                tip += QStringLiteral("\nOCR: %1").arg(Selt::RecognitionCapability::ocrStatusText());
            }
            auto *tile = makeOperatorTile(
                type,
                desc.displayName,
                Selt::IconProvider::visionIcon(
                    desc.iconKey.isEmpty() ? desc.category : desc.iconKey, 24),
                tip,
                scroll->widget());
#else
            auto *tile = makeOperatorTile(type, type, QIcon(), type, scroll->widget());
#endif
            grid->addWidget(tile, 0, added);
            ++added;
        }
        if (added == 0) {
            delete scroll;
            return;
        }
        m_toolBox->addItem(scroll, Selt::IconProvider::visionIcon(iconKey, 16), title);
    };

    addTypePage(QStringLiteral("收藏"), Selt::OperatorPreferences::favorites(), QStringLiteral("模板"));
    addTypePage(QStringLiteral("最近使用"), Selt::OperatorPreferences::recent(), QStringLiteral("输入"));

    const int categoryCount = m_proxy->rowCount(QModelIndex());
    int operatorCount = 0;
    for (int c = 0; c < categoryCount; ++c) {
        const QModelIndex categoryIndex = m_proxy->index(c, 0);
        const QString categoryName = categoryIndex.data(Qt::DisplayRole).toString();
        QGridLayout *grid = nullptr;
        QScrollArea *scroll = makeIconPage(m_toolBox, &grid);
        int col = 0;
        const int childCount = m_proxy->rowCount(categoryIndex);
        for (int i = 0; i < childCount; ++i) {
            const QModelIndex child = m_proxy->index(i, 0, categoryIndex);
            const QString type = child.data(Selt::NodeCatalogModel::TypeRole).toString();
            if (type.isEmpty())
                continue;
            auto *tile = makeOperatorTile(type,
                                          child.data(Qt::DisplayRole).toString(),
                                          child.data(Qt::DecorationRole).value<QIcon>(),
                                          child.data(Qt::ToolTipRole).toString(),
                                          scroll->widget());
            grid->addWidget(tile, 0, col++);
            ++operatorCount;
        }
        if (col == 0) {
            delete scroll;
            continue;
        }
        scroll->setProperty("seltCategory", categoryName);
        const int stageRank = Selt::WorkflowTemplates::categoryStageRank(categoryName);
        const QString stageTitle = stageRank < 100
            ? QStringLiteral("%1 · %2").arg(stageRank + 1).arg(categoryName)
            : categoryName;
        m_toolBox->addItem(scroll, Selt::IconProvider::visionIcon(categoryName, 16), stageTitle);
    }

    if (operatorCount == 0) {
        const QString filter = m_filterEdit ? m_filterEdit->text().trimmed() : QString();
        if (filter.isEmpty())
            showEmptyIconState(QStringLiteral("暂无可用算子。"));
        else
            showEmptyIconState(QStringLiteral("无匹配算子：%1").arg(filter));
    }

    if (!currentTitle.isEmpty()) {
        for (int i = 0; i < m_toolBox->count(); ++i) {
            if (m_toolBox->itemText(i) == currentTitle) {
                m_toolBox->setCurrentIndex(i);
                break;
            }
        }
    }

    updateIconColumns();
}

void NodePalette::updateIconColumns()
{
    if (!m_toolBox)
        return;
    const int width = qMax(140, this->width() - 24);
    const int columns = qBound(1, width / (kTileWidth + 6), 4);

    for (int page = 0; page < m_toolBox->count(); ++page) {
        auto *scroll = qobject_cast<QScrollArea *>(m_toolBox->widget(page));
        if (!scroll || !scroll->widget())
            continue;
        auto *grid = qobject_cast<QGridLayout *>(scroll->widget()->layout());
        if (!grid)
            continue;

        QList<QWidget *> tiles;
        while (grid->count() > 0) {
            QLayoutItem *item = grid->takeAt(0);
            if (item && item->widget())
                tiles.append(item->widget());
            delete item;
        }
        int index = 0;
        for (QWidget *tile : tiles) {
            grid->addWidget(tile, index / columns, index % columns);
            ++index;
        }
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
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (type == m_lastCreateType && (now - m_lastCreateMs) < 120)
        return;
    m_lastCreateType = type;
    m_lastCreateMs = now;

    // Leave the UI event stack before creating (shared by icon tiles and tree).
    const QString typeCopy = type;
    QPointer<NodePalette> guard(this);
    QTimer::singleShot(0, this, [guard, typeCopy]() {
        if (!guard)
            return;
        emit guard->nodeTypeActivated(typeCopy);
    });
}

QString NodePalette::typeAt(const QModelIndex &proxyIndex) const
{
    if (!proxyIndex.isValid() || !m_proxy)
        return {};
    return m_proxy->data(proxyIndex, Selt::NodeCatalogModel::TypeRole).toString();
}
