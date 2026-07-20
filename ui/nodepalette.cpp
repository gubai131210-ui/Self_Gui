#include "nodepalette.h"

#include "core/registry/nodefactory.h"

#include <QDrag>
#include <QMimeData>
#include <QMouseEvent>

static const char *kNodeTypeMime = "application/x-selt-node-type";

NodePalette::NodePalette(QWidget *parent)
    : QListWidget(parent)
{
    setDragEnabled(true);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setIconSize(QSize(28, 28));
    setSpacing(2);

    for (const QString &type : NodeFactory::supportedTypes()) {
        auto *item = new QListWidgetItem(NodeFactory::displayName(type), this);
        item->setData(Qt::UserRole, type);
        item->setToolTip(QStringLiteral("拖拽到画布创建: %1").arg(NodeFactory::displayName(type)));
    }

    connect(this, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        if (item)
            emit nodeTypeActivated(item->data(Qt::UserRole).toString());
    });
}

void NodePalette::startDrag(Qt::DropActions supportedActions)
{
    Q_UNUSED(supportedActions);
    QListWidgetItem *item = currentItem();
    if (!item)
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
    QListWidget::mouseDoubleClickEvent(event);
    if (QListWidgetItem *item = itemAt(event->pos()))
        emit nodeTypeActivated(item->data(Qt::UserRole).toString());
}
