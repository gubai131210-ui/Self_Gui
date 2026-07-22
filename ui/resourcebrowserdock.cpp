#include "resourcebrowserdock.h"

#include <QBrush>
#include <QColor>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

ResourceBrowserDock::ResourceBrowserDock(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(4);

    auto *row = new QHBoxLayout;
    m_kindFilter = new QComboBox(this);
    m_kindFilter->addItem(QStringLiteral("全部"), QString());
    m_kindFilter->addItem(QStringLiteral("模板"), QStringLiteral("template"));
    m_kindFilter->addItem(QStringLiteral("标定"), QStringLiteral("calibration"));
    m_kindFilter->addItem(QStringLiteral("图像"), QStringLiteral("image"));
    m_kindFilter->addItem(QStringLiteral("其他"), QStringLiteral("other"));
    m_refreshBtn = new QPushButton(QStringLiteral("刷新"), this);
    row->addWidget(m_kindFilter, 1);
    row->addWidget(m_refreshBtn);
    layout->addLayout(row);

    m_list = new QListWidget(this);
    layout->addWidget(m_list, 1);

    m_summary = new QLabel(QStringLiteral("无项目资源"), this);
    m_summary->setWordWrap(true);
    layout->addWidget(m_summary);

    connect(m_kindFilter, &QComboBox::currentIndexChanged, this, &ResourceBrowserDock::onFilterChanged);
    connect(m_refreshBtn, &QPushButton::clicked, this, &ResourceBrowserDock::reload);
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *) {
        onItemActivated();
    });
}

#if SELT_HAS_OPENCV
void ResourceBrowserDock::setStore(const Selt::ProjectResourceStore *store)
{
    m_store = store;
    reload();
}
#endif

void ResourceBrowserDock::reload()
{
    m_list->clear();
#if SELT_HAS_OPENCV
    if (!m_store) {
        m_summary->setText(QStringLiteral("无项目资源"));
        return;
    }
    const QString kind = m_kindFilter->currentData().toString();
    const QVector<Selt::ProjectResource> resources = kind.isEmpty()
        ? m_store->resources()
        : m_store->resourcesOfKind(kind);
    int missing = 0;
    for (const Selt::ProjectResource &r : resources) {
        auto *item = new QListWidgetItem(m_list);
        const QString title = r.displayName.isEmpty() ? r.id : r.displayName;
        item->setText(QStringLiteral("%1 [%2]%3")
                          .arg(title, r.kind, r.exists ? QString() : QStringLiteral(" 缺失")));
        item->setData(Qt::UserRole, r.id);
        item->setToolTip(r.relativePath.isEmpty() ? r.absolutePath : r.relativePath);
        if (!r.exists) {
            item->setForeground(QBrush(QColor(200, 60, 60)));
            ++missing;
        }
    }
    m_summary->setText(QStringLiteral("共 %1 项，缺失 %2")
                           .arg(resources.size())
                           .arg(missing));
#else
    m_summary->setText(QStringLiteral("OpenCV 未启用"));
#endif
}

void ResourceBrowserDock::onFilterChanged()
{
    reload();
}

void ResourceBrowserDock::onItemActivated()
{
    QListWidgetItem *item = m_list->currentItem();
    if (!item)
        return;
    emit resourceActivated(item->data(Qt::UserRole).toString());
}
