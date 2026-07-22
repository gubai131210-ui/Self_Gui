#include "ui/widgets/inspectorheader.h"

#include "ui/theme/iconprovider.h"
#include "ui/theme/uistyle.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

InspectorHeader::InspectorHeader(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(8);

    m_iconLabel = new QLabel(this);
    m_iconLabel->setFixedSize(20, 20);
    m_iconLabel->setScaledContents(true);

    auto *textCol = new QVBoxLayout;
    textCol->setContentsMargins(0, 0, 0, 0);
    textCol->setSpacing(2);
    m_nameLabel = new QLabel(QStringLiteral("未选择模块"), this);
    QFont nameFont = m_nameLabel->font();
    nameFont.setBold(true);
    m_nameLabel->setFont(nameFont);
    m_nameLabel->setStyleSheet(
        QStringLiteral("color: %1;").arg(Selt::UiStyle::textPrimary().name()));
    m_categoryLabel = new QLabel(this);
    m_categoryLabel->setStyleSheet(
        QStringLiteral("color: %1; font-size: 11px;")
            .arg(Selt::UiStyle::textSecondary().name()));
    textCol->addWidget(m_nameLabel);
    textCol->addWidget(m_categoryLabel);

    m_statusDot = new QLabel(this);
    m_statusDot->setObjectName(QStringLiteral("InspectorStatusDot"));
    m_statusDot->setFixedSize(12, 12);
    m_statusText = new QLabel(QStringLiteral("未执行"), this);
    m_statusText->setStyleSheet(
        QStringLiteral("color: %1; font-size: 11px;")
            .arg(Selt::UiStyle::textSecondary().name()));
    m_statusText->setWordWrap(true);

    auto *statusCol = new QVBoxLayout;
    statusCol->setContentsMargins(0, 0, 0, 0);
    statusCol->setSpacing(2);
    auto *statusRow = new QHBoxLayout;
    statusRow->setContentsMargins(0, 0, 0, 0);
    statusRow->setSpacing(4);
    statusRow->addWidget(m_statusDot);
    statusRow->addWidget(m_statusText, 1);
    statusCol->addLayout(statusRow);

    m_runButton = new QPushButton(QStringLiteral("▶"), this);
    m_runButton->setToolTip(QStringLiteral("运行此节点"));
    m_runButton->setFixedSize(28, 28);
    m_runButton->setEnabled(false);

    m_menuButton = new QToolButton(this);
    m_menuButton->setText(QStringLiteral("⚙"));
    m_menuButton->setToolTip(QStringLiteral("更多"));
    m_menuButton->setFixedSize(28, 28);
    m_menuButton->setPopupMode(QToolButton::InstantPopup);
    auto *menu = new QMenu(m_menuButton);
    menu->addAction(QStringLiteral("复制参数到剪贴板"), this, [this]() {
        emit copyParamsRequested();
    });
    m_menuButton->setMenu(menu);

    root->addWidget(m_iconLabel);
    root->addLayout(textCol, 1);
    root->addLayout(statusCol, 1);
    root->addWidget(m_runButton);
    root->addWidget(m_menuButton);

    connect(m_runButton, &QPushButton::clicked, this, &InspectorHeader::runNodeRequested);
    updateStatusDot(NodeRunVisualStatus::Idle);
}

void InspectorHeader::setNodeInfo(const QString &displayName,
                                  const QString &category,
                                  const QString &iconKey)
{
    m_nameLabel->setText(displayName.isEmpty() ? QStringLiteral("未命名模块") : displayName);
    m_categoryLabel->setText(category);
    const QIcon icon = Selt::IconProvider::visionIcon(
        iconKey.isEmpty() ? category : iconKey, 20);
    m_iconLabel->setPixmap(icon.pixmap(20, 20));
    m_runButton->setEnabled(true);
    m_menuButton->setEnabled(true);
}

void InspectorHeader::setEmpty()
{
    m_nameLabel->setText(QStringLiteral("未选择模块"));
    m_categoryLabel->clear();
    m_iconLabel->clear();
    setStatus(NodeRunVisualStatus::Idle, QStringLiteral("未执行"));
    m_runButton->setEnabled(false);
    m_menuButton->setEnabled(false);
}

void InspectorHeader::setStatus(NodeRunVisualStatus status, const QString &text)
{
    m_statusText->setText(text.isEmpty() ? QStringLiteral("未执行") : text);
    updateStatusDot(status);
}

void InspectorHeader::updateStatusDot(NodeRunVisualStatus status)
{
    QString key = QStringLiteral("idle");
    QColor color = Selt::UiStyle::connectionIdle();
    switch (status) {
    case NodeRunVisualStatus::Running:
        key = QStringLiteral("running");
        color = Selt::UiStyle::runningBlue();
        break;
    case NodeRunVisualStatus::Success:
        key = QStringLiteral("success");
        color = Selt::UiStyle::successGreen();
        break;
    case NodeRunVisualStatus::Failed:
        key = QStringLiteral("failed");
        color = Selt::UiStyle::failRed();
        break;
    case NodeRunVisualStatus::Warning:
        key = QStringLiteral("warning");
        color = Selt::UiStyle::accentOrange();
        break;
    case NodeRunVisualStatus::Disabled:
        key = QStringLiteral("idle");
        color = Selt::UiStyle::textSecondary();
        break;
    case NodeRunVisualStatus::Idle:
    default:
        break;
    }
    m_statusDot->setProperty("status", key);
    m_statusDot->setStyleSheet(
        QStringLiteral("QLabel#InspectorStatusDot {"
                       " background-color: %1; border-radius: 6px; }")
            .arg(color.name()));
}
