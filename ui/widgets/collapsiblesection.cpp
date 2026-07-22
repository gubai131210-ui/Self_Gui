#include "ui/widgets/collapsiblesection.h"

#include "ui/theme/uistyle.h"

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QSettings>
#include <QVBoxLayout>

CollapsibleSection::CollapsibleSection(const QString &title,
                                       const QString &settingsKey,
                                       bool defaultExpanded,
                                       QWidget *parent)
    : QWidget(parent)
    , m_settingsKey(settingsKey)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_header = new QFrame(this);
    m_header->setObjectName(QStringLiteral("CollapsibleSectionHeader"));
    m_header->setCursor(Qt::PointingHandCursor);
    m_header->setStyleSheet(
        QStringLiteral("QFrame#CollapsibleSectionHeader {"
                       " background-color: %1; border: 1px solid %2; border-radius: 4px; }"
                       "QFrame#CollapsibleSectionHeader:hover {"
                       " background-color: %3; border-color: %4; }")
            .arg(Selt::UiStyle::sectionHeaderBackground().name(),
                 Selt::UiStyle::sectionHeaderBorder().name(),
                 Selt::UiStyle::sectionHeaderHover().name(),
                 Selt::UiStyle::accent().name()));
    auto *headerLayout = new QHBoxLayout(m_header);
    headerLayout->setContentsMargins(8, 4, 8, 4);
    headerLayout->setSpacing(6);

    m_arrow = new QLabel(m_header);
    m_arrow->setFixedWidth(12);
    m_arrow->setStyleSheet(
        QStringLiteral("color: %1;").arg(Selt::UiStyle::sectionHeaderText().name()));
    m_titleLabel = new QLabel(title, m_header);
    QFont titleFont = m_titleLabel->font();
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setStyleSheet(
        QStringLiteral("color: %1;").arg(Selt::UiStyle::sectionHeaderText().name()));
    headerLayout->addWidget(m_arrow);
    headerLayout->addWidget(m_titleLabel, 1);

    m_bodyHost = new QWidget(this);
    m_bodyLayout = new QVBoxLayout(m_bodyHost);
    m_bodyLayout->setContentsMargins(4, 6, 4, 4);
    m_bodyLayout->setSpacing(Selt::UiStyle::compactSpacing);

    root->addWidget(m_header);
    root->addWidget(m_bodyHost);

    m_header->installEventFilter(this);
    m_expanded = loadExpandedDefault(defaultExpanded);
    applyExpandedState();
}

QString CollapsibleSection::title() const
{
    return m_titleLabel ? m_titleLabel->text() : QString();
}

void CollapsibleSection::setTitle(const QString &title)
{
    if (m_titleLabel)
        m_titleLabel->setText(title);
}

void CollapsibleSection::setBodyWidget(QWidget *body)
{
    if (m_body) {
        m_bodyLayout->removeWidget(m_body);
        m_body->deleteLater();
        m_body = nullptr;
    }
    m_body = body;
    if (m_body) {
        m_body->setParent(m_bodyHost);
        m_bodyLayout->addWidget(m_body);
    }
    applyExpandedState();
}

void CollapsibleSection::setExpanded(bool expanded)
{
    if (m_expanded == expanded)
        return;
    m_expanded = expanded;
    applyExpandedState();
    persistExpanded();
    emit toggled(m_expanded);
}

bool CollapsibleSection::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_header && event->type() == QEvent::MouseButtonRelease) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::LeftButton) {
            setExpanded(!m_expanded);
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void CollapsibleSection::applyExpandedState()
{
    if (m_arrow)
        m_arrow->setText(m_expanded ? QStringLiteral("▼") : QStringLiteral("▶"));
    if (m_bodyHost)
        m_bodyHost->setVisible(m_expanded);
}

void CollapsibleSection::persistExpanded() const
{
    if (m_settingsKey.isEmpty())
        return;
    QSettings settings;
    settings.setValue(m_settingsKey, m_expanded);
}

bool CollapsibleSection::loadExpandedDefault(bool fallback) const
{
    if (m_settingsKey.isEmpty())
        return fallback;
    QSettings settings;
    return settings.value(m_settingsKey, fallback).toBool();
}
