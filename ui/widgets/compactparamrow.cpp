#include "ui/widgets/compactparamrow.h"

#if SELT_HAS_OPENCV
#include "ui/bindingeditor.h"
#endif
#include "ui/theme/uistyle.h"

#include <QDoubleSpinBox>
#include <QEvent>
#include <QGridLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QSlider>
#include <QSpinBox>
#include <QToolButton>

#include <cmath>

CompactParamRow::CompactParamRow(const QString &label,
                                 QWidget *editor,
                                 bool hasSlider,
                                 QWidget *parent)
    : QWidget(parent)
    , m_editor(editor)
{
    setProperty("paramLabel", label);
    m_grid = new QGridLayout(this);
    m_grid->setContentsMargins(0, 0, 0, 0);
    m_grid->setHorizontalSpacing(6);
    m_grid->setVerticalSpacing(2);
    m_grid->setColumnStretch(0, 38);
    m_grid->setColumnStretch(1, 56);
    m_grid->setColumnStretch(2, 6);

    m_label = new QLabel(label, this);
    m_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_label->setStyleSheet(
        QStringLiteral("color: %1;").arg(Selt::UiStyle::textSecondary().name()));
    m_label->setWordWrap(true);
    if (hasSlider)
        m_label->setCursor(Qt::PointingHandCursor);

    if (m_editor)
        m_editor->setParent(this);

    m_linkButton = new QToolButton(this);
    m_linkButton->setText(QStringLiteral("🔗"));
    m_linkButton->setAutoRaise(true);
    m_linkButton->setFixedSize(22, 22);
    m_linkButton->setToolTip(QStringLiteral("展开/折叠参数绑定"));

    m_grid->addWidget(m_label, 0, 0);
    if (m_editor)
        m_grid->addWidget(m_editor, 0, 1);
    m_grid->addWidget(m_linkButton, 0, 2);

    if (hasSlider) {
        m_slider = new QSlider(Qt::Horizontal, this);
        m_slider->setVisible(false);
        m_grid->addWidget(m_slider, 1, 0, 1, 3);
        wireEditorSlider();
        m_label->installEventFilter(this);
    }

    connect(m_linkButton, &QToolButton::clicked, this, &CompactParamRow::onLinkClicked);

#if SELT_HAS_OPENCV
    if (auto *binding = qobject_cast<BindingEditor *>(m_editor)) {
        m_bindingEditor = binding;
        m_bindingEditor->setCompactMode(true);
    }
#endif
}

bool CompactParamRow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_label && m_slider && event->type() == QEvent::MouseButtonRelease) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::LeftButton) {
            setSliderVisible(!m_slider->isVisible());
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void CompactParamRow::setBindingEditor(BindingEditor *editor)
{
#if SELT_HAS_OPENCV
    if (!editor)
        return;
    m_bindingEditor = editor;
    if (m_editor && m_editor != editor) {
        m_grid->removeWidget(m_editor);
        m_editor = editor;
        m_editor->setParent(this);
        m_grid->addWidget(m_editor, 0, 1);
    } else if (!m_editor) {
        m_editor = editor;
        m_editor->setParent(this);
        m_grid->addWidget(m_editor, 0, 1);
    }
    m_bindingEditor->setCompactMode(true);
    m_bindingExpanded = false;
    wireEditorSlider();
#else
    Q_UNUSED(editor);
#endif
}

#if SELT_HAS_OPENCV
void CompactParamRow::setBindingIndicator(Selt::ParameterSourceKind kind)
{
    QColor color = Selt::UiStyle::textSecondary();
    QString tip = QStringLiteral("绑定: 常量");
    switch (kind) {
    case Selt::ParameterSourceKind::UpstreamBinding:
        color = Selt::UiStyle::accentOrange();
        tip = QStringLiteral("绑定: 上游输出");
        break;
    case Selt::ParameterSourceKind::ProjectVariable:
        color = Selt::UiStyle::runningBlue();
        tip = QStringLiteral("绑定: 项目变量");
        break;
    case Selt::ParameterSourceKind::Constant:
    default:
        break;
    }
    m_linkButton->setStyleSheet(
        QStringLiteral("QToolButton { color: %1; font-size: 12px; }").arg(color.name()));
    m_linkButton->setToolTip(tip + QStringLiteral("（点击展开/折叠）"));
}
#endif

void CompactParamRow::setSliderRange(double min, double max, int decimals)
{
    m_sliderMin = min;
    m_sliderMax = max;
    m_decimals = qMax(0, decimals);
    if (!m_slider)
        return;
    if (m_decimals <= 0) {
        m_slider->setRange(int(min), int(max));
    } else {
        const int scale = static_cast<int>(std::pow(10, m_decimals));
        m_slider->setRange(int(min * scale), int(max * scale));
    }
    syncSliderFromEditor();
}

void CompactParamRow::setSliderVisible(bool visible)
{
    if (m_slider)
        m_slider->setVisible(visible);
}

bool CompactParamRow::isSliderVisible() const
{
    return m_slider && m_slider->isVisible();
}

void CompactParamRow::onLinkClicked()
{
    m_bindingExpanded = !m_bindingExpanded;
#if SELT_HAS_OPENCV
    if (m_bindingEditor)
        m_bindingEditor->setCompactMode(!m_bindingExpanded);
#endif
    emit bindingIconClicked();
}

QWidget *CompactParamRow::sliderTargetEditor() const
{
#if SELT_HAS_OPENCV
    if (m_bindingEditor && m_bindingEditor->constantEditor())
        return m_bindingEditor->constantEditor();
#endif
    return m_editor;
}

void CompactParamRow::onSliderMoved(int value)
{
    QWidget *target = sliderTargetEditor();
    if (!target)
        return;
    if (auto *spin = qobject_cast<QSpinBox *>(target)) {
        spin->blockSignals(true);
        spin->setValue(value);
        spin->blockSignals(false);
        emit sliderEdited();
        return;
    }
    if (auto *spin = qobject_cast<QDoubleSpinBox *>(target)) {
        const double scale = m_decimals > 0 ? std::pow(10.0, m_decimals) : 1.0;
        spin->blockSignals(true);
        spin->setValue(value / scale);
        spin->blockSignals(false);
        emit sliderEdited();
    }
}

void CompactParamRow::syncSliderFromEditor()
{
    if (!m_slider)
        return;
    QWidget *target = sliderTargetEditor();
    if (!target)
        return;
    m_slider->blockSignals(true);
    if (auto *spin = qobject_cast<QSpinBox *>(target)) {
        m_slider->setValue(spin->value());
    } else if (auto *spin = qobject_cast<QDoubleSpinBox *>(target)) {
        const double scale = m_decimals > 0 ? std::pow(10.0, m_decimals) : 1.0;
        m_slider->setValue(int(spin->value() * scale));
    }
    m_slider->blockSignals(false);
}

void CompactParamRow::wireEditorSlider()
{
    if (!m_slider)
        return;
    QWidget *target = sliderTargetEditor();
    if (!target)
        return;

    disconnect(m_slider, nullptr, this, nullptr);
    connect(m_slider, &QSlider::valueChanged, this, &CompactParamRow::onSliderMoved);

    if (auto *spin = qobject_cast<QSpinBox *>(target)) {
        m_sliderMin = spin->minimum();
        m_sliderMax = spin->maximum();
        m_decimals = 0;
        m_slider->setRange(spin->minimum(), spin->maximum());
        connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this,
                [this](int) { syncSliderFromEditor(); });
    } else if (auto *spin = qobject_cast<QDoubleSpinBox *>(target)) {
        m_sliderMin = spin->minimum();
        m_sliderMax = spin->maximum();
        m_decimals = spin->decimals();
        const int scale = static_cast<int>(std::pow(10, m_decimals));
        m_slider->setRange(int(spin->minimum() * scale), int(spin->maximum() * scale));
        connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this](double) { syncSliderFromEditor(); });
    }
    syncSliderFromEditor();
}
