#include "nodepalette.h"

#include "app/nodebuilder.h"
#if SELT_HAS_OPENCV
#include "vision/registry/visionnoderegistry.h"
#endif

#include <QDrag>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMimeData>
#include <QMouseEvent>
#include <QVBoxLayout>

static const char *kNodeTypeMime = "application/x-selt-node-type";

NodePalette::NodePalette(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    auto *title = new QLabel(QStringLiteral("视觉工具箱"), this);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    title->setFont(titleFont);

    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText(QStringLiteral("搜索模块..."));

    m_list = new QListWidget(this);
    m_list->setDragEnabled(true);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setSpacing(2);

    layout->addWidget(title);
    layout->addWidget(m_filterEdit);
    layout->addWidget(m_list, 1);

    rebuildList();

    connect(m_filterEdit, &QLineEdit::textChanged, this, &NodePalette::onFilterChanged);
    connect(m_list, &QListWidget::itemActivated, this, &NodePalette::onItemActivated);
    connect(m_list, &QListWidget::itemPressed, this, [this](QListWidgetItem *item) {
        if (item && (item->flags() & Qt::ItemIsDragEnabled))
            startDragFromItem(item);
    });
}

void NodePalette::rebuildList(const QString &filter)
{
    m_list->clear();
#if SELT_HAS_OPENCV
    const QString needle = filter.trimmed();
    for (const QString &category : VisionNodeRegistry::categories()) {
        QStringList matched;
        for (const QString &type : VisionNodeRegistry::typesInCategory(category)) {
            const QString name = VisionNodeRegistry::displayName(type);
            if (!needle.isEmpty()
                && !name.contains(needle, Qt::CaseInsensitive)
                && !type.contains(needle, Qt::CaseInsensitive)
                && !category.contains(needle, Qt::CaseInsensitive)) {
                continue;
            }
            matched.append(type);
        }
        if (matched.isEmpty())
            continue;

        auto *header = new QListWidgetItem(category, m_list);
        header->setFlags(Qt::NoItemFlags);
        QFont font = header->font();
        font.setBold(true);
        header->setFont(font);

        for (const QString &type : matched) {
            auto *item = new QListWidgetItem(VisionNodeRegistry::displayName(type), m_list);
            item->setData(Qt::UserRole, type);
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
            item->setToolTip(VisionNodeRegistry::descriptor(type).description);
        }
    }
#else
    Q_UNUSED(filter);
    auto *item = new QListWidgetItem(QStringLiteral("OpenCV 未启用"), m_list);
    item->setFlags(Qt::NoItemFlags);
#endif
}

void NodePalette::onFilterChanged(const QString &text)
{
    rebuildList(text);
}

void NodePalette::onItemActivated(QListWidgetItem *item)
{
    if (!item || !(item->flags() & Qt::ItemIsSelectable))
        return;
    emit nodeTypeActivated(item->data(Qt::UserRole).toString());
}

void NodePalette::startDragFromItem(QListWidgetItem *item)
{
    if (!item || !(item->flags() & Qt::ItemIsDragEnabled))
        return;

    auto *mime = new QMimeData;
    mime->setData(QString::fromLatin1(kNodeTypeMime),
                  item->data(Qt::UserRole).toString().toUtf8());

    auto *drag = new QDrag(this);
    drag->setMimeData(mime);
    drag->exec(Qt::CopyAction);
}

void NodePalette::mouseDoubleClickEvent(QMouseEvent *event)
{
    QWidget::mouseDoubleClickEvent(event);
    if (QListWidgetItem *item = m_list->itemAt(m_list->mapFrom(this, event->pos()))) {
        if (item->flags() & Qt::ItemIsSelectable)
            emit nodeTypeActivated(item->data(Qt::UserRole).toString());
    }
}
