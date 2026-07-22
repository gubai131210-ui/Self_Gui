#include "ui/flowbarwidget.h"

#include "ui/theme/iconprovider.h"
#include "ui/theme/uistyle.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QTabBar>
#include <QToolButton>

FlowBarWidget::FlowBarWidget(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("FlowBarWidget"));
    setMaximumHeight(Selt::UiStyle::flowBarHeight + 8);

    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(Selt::UiStyle::panelMargin,
                             Selt::UiStyle::compactSpacing,
                             Selt::UiStyle::panelMargin,
                             Selt::UiStyle::compactSpacing);
    root->setSpacing(Selt::UiStyle::compactSpacing);

    m_selectorStack = new QStackedWidget(this);
    m_tabBar = new QTabBar(m_selectorStack);
    m_tabBar->setObjectName(QStringLiteral("FlowTabBar"));
    m_tabBar->setExpanding(false);
    m_tabBar->setDrawBase(false);
    m_tabBar->setMovable(false);
    m_tabBar->setUsesScrollButtons(true);
    m_tabBar->setElideMode(Qt::ElideRight);
    m_combo = new QComboBox(m_selectorStack);
    m_combo->setObjectName(QStringLiteral("FlowCombo"));
    m_combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_selectorStack->addWidget(m_tabBar);
    m_selectorStack->addWidget(m_combo);
    root->addWidget(m_selectorStack, 1);

    auto makeButton = [this](Selt::IconId icon, const QString &tip) {
        auto *btn = new QToolButton(this);
        btn->setIcon(Selt::IconProvider::icon(icon, Selt::UiStyle::toolIconSize));
        btn->setToolTip(tip);
        btn->setAutoRaise(true);
        btn->setIconSize(QSize(Selt::UiStyle::toolIconSize, Selt::UiStyle::toolIconSize));
        return btn;
    };

    m_addButton = makeButton(Selt::IconId::AddFlow, QStringLiteral("新建流程"));
    m_renameButton = makeButton(Selt::IconId::Rename, QStringLiteral("重命名流程"));
    m_removeButton = makeButton(Selt::IconId::RemoveFlow, QStringLiteral("删除流程"));
    root->addWidget(m_addButton);
    root->addWidget(m_renameButton);
    root->addWidget(m_removeButton);

    connect(m_tabBar, &QTabBar::currentChanged, this, [this](int index) {
        emitActiveFromIndex(index);
    });
    connect(m_combo, &QComboBox::currentIndexChanged, this, [this](int index) {
        emitActiveFromIndex(index);
    });
    connect(m_addButton, &QToolButton::clicked, this, &FlowBarWidget::createFlowRequested);
    connect(m_renameButton, &QToolButton::clicked, this, &FlowBarWidget::renameFlowRequested);
    connect(m_removeButton, &QToolButton::clicked, this, &FlowBarWidget::removeFlowRequested);
}

void FlowBarWidget::emitActiveFromIndex(int index)
{
    if (m_updating || index < 0 || index >= m_ids.size())
        return;
    const QString id = m_ids.at(index);
    if (id == m_activeId)
        return;
    m_activeId = id;
    m_updating = true;
    if (m_tabBar->currentIndex() != index)
        m_tabBar->setCurrentIndex(index);
    if (m_combo->currentIndex() != index)
        m_combo->setCurrentIndex(index);
    m_updating = false;
    emit activeFlowChosen(id);
}

void FlowBarWidget::setFlows(const QStringList &ids, const QStringList &names, const QString &activeId)
{
    m_updating = true;
    m_ids = ids;
    m_combo->clear();
    while (m_tabBar->count() > 0)
        m_tabBar->removeTab(0);

    const int count = qMin(ids.size(), names.size());
    int activeIndex = 0;
    for (int i = 0; i < count; ++i) {
        m_combo->addItem(names.at(i), ids.at(i));
        m_tabBar->addTab(names.at(i));
        if (ids.at(i) == activeId)
            activeIndex = i;
    }
    if (count > 0) {
        m_activeId = ids.at(activeIndex);
        m_combo->setCurrentIndex(activeIndex);
        m_tabBar->setCurrentIndex(activeIndex);
    } else {
        m_activeId.clear();
    }
    m_removeButton->setEnabled(count > 1);
    m_renameButton->setEnabled(count > 0);
    m_updating = false;
    updatePresentationMode();
}

void FlowBarWidget::syncActiveUi(const QString &activeId)
{
    const int index = m_ids.indexOf(activeId);
    if (index < 0)
        return;
    m_activeId = activeId;
    m_updating = true;
    m_tabBar->setCurrentIndex(index);
    m_combo->setCurrentIndex(index);
    m_updating = false;
}

void FlowBarWidget::updatePresentationMode()
{
    // Prefer tabs; fall back to combo when the bar is too narrow or too many flows.
    const bool useCombo = width() < 420 || m_ids.size() > 8;
    const int target = useCombo ? 1 : 0;
    if (m_selectorStack->currentIndex() == target)
        return;
    m_updating = true;
    m_selectorStack->setCurrentIndex(target);
    // Keep the visible control aligned with the single active id.
    const int index = m_ids.indexOf(m_activeId);
    if (index >= 0) {
        m_tabBar->setCurrentIndex(index);
        m_combo->setCurrentIndex(index);
    }
    m_updating = false;
}

void FlowBarWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updatePresentationMode();
}
