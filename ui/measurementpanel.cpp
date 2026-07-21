#include "measurementpanel.h"

#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QVBoxLayout>

MeasurementPanel::MeasurementPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);

    m_outputCombo = new QComboBox(this);
    layout->addWidget(new QLabel(QStringLiteral("查看模块输出"), this));
    layout->addWidget(m_outputCombo);

    auto *form = new QFormLayout;
    m_statusLabel = new QLabel(QStringLiteral("未执行"), this);
    m_moduleStatusLabel = new QLabel(QStringLiteral("-"), this);
    m_widthLabel = new QLabel(QStringLiteral("-"), this);
    m_heightLabel = new QLabel(QStringLiteral("-"), this);
    m_areaLabel = new QLabel(QStringLiteral("-"), this);
    m_centerLabel = new QLabel(QStringLiteral("-"), this);
    m_timeLabel = new QLabel(QStringLiteral("-"), this);
    m_moduleTimeLabel = new QLabel(QStringLiteral("-"), this);
    form->addRow(QStringLiteral("流程状态"), m_statusLabel);
    form->addRow(QStringLiteral("模块状态"), m_moduleStatusLabel);
    form->addRow(QStringLiteral("宽度"), m_widthLabel);
    form->addRow(QStringLiteral("高度"), m_heightLabel);
    form->addRow(QStringLiteral("面积"), m_areaLabel);
    form->addRow(QStringLiteral("中心点"), m_centerLabel);
    form->addRow(QStringLiteral("总耗时"), m_timeLabel);
    form->addRow(QStringLiteral("模块耗时"), m_moduleTimeLabel);
    layout->addLayout(form);

    m_logEdit = new QPlainTextEdit(this);
    m_logEdit->setReadOnly(true);
    m_logEdit->setMaximumHeight(180);
    layout->addWidget(new QLabel(QStringLiteral("执行日志"), this));
    layout->addWidget(m_logEdit);

    connect(m_outputCombo, &QComboBox::currentIndexChanged, this, &MeasurementPanel::onOutputComboChanged);
}

void MeasurementPanel::rebuildOutputCombo(const VisionContext &context, const QString &preferredNodeId)
{
    m_outputCombo->blockSignals(true);
    m_outputCombo->clear();
    m_outputCombo->addItem(QStringLiteral("最终结果"), QString());
    for (const QString &nodeId : context.executionOrder) {
        const ModuleRunResult result = context.moduleResult(nodeId);
        const QString label = result.displayName.isEmpty()
            ? nodeId
            : QStringLiteral("%1 (%2 ms)").arg(result.displayName).arg(result.elapsedMs);
        m_outputCombo->addItem(label, nodeId);
    }

    int index = 0;
    if (!preferredNodeId.isEmpty()) {
        const int found = m_outputCombo->findData(preferredNodeId);
        if (found >= 0)
            index = found;
    } else if (!context.executionOrder.isEmpty()) {
        index = m_outputCombo->count() - 1;
    }
    m_outputCombo->setCurrentIndex(index);
    m_outputCombo->blockSignals(false);
}

void MeasurementPanel::showMeasurement(const MeasurementResult &measurement)
{
    if (measurement.valid) {
        m_widthLabel->setText(QStringLiteral("%1 px").arg(measurement.width, 0, 'f', 1));
        m_heightLabel->setText(QStringLiteral("%1 px").arg(measurement.height, 0, 'f', 1));
        m_areaLabel->setText(QStringLiteral("%1 px²").arg(measurement.area, 0, 'f', 1));
        m_centerLabel->setText(QStringLiteral("(%1, %2)")
                                   .arg(measurement.center.x(), 0, 'f', 1)
                                   .arg(measurement.center.y(), 0, 'f', 1));
    } else {
        m_widthLabel->setText(QStringLiteral("-"));
        m_heightLabel->setText(QStringLiteral("-"));
        m_areaLabel->setText(QStringLiteral("-"));
        m_centerLabel->setText(QStringLiteral("-"));
    }
}

void MeasurementPanel::showContext(const VisionContext &context, const QString &preferredNodeId)
{
    m_context = context;
    rebuildOutputCombo(context, preferredNodeId);

    if (context.hasError()) {
        m_statusLabel->setText(QStringLiteral("失败: %1").arg(context.errorMessage));
    } else if (context.measurement.valid) {
        m_statusLabel->setText(QStringLiteral("成功"));
    } else {
        m_statusLabel->setText(QStringLiteral("完成"));
    }

    m_timeLabel->setText(QStringLiteral("%1 ms").arg(context.elapsedMs));
    m_logEdit->setPlainText(context.log.join(QLatin1Char('\n')));

    onOutputComboChanged(m_outputCombo->currentIndex());
}

void MeasurementPanel::onOutputComboChanged(int index)
{
    Q_UNUSED(index);
    const QString nodeId = selectedOutputNodeId();
    if (nodeId.isEmpty()) {
        m_moduleStatusLabel->setText(QStringLiteral("最终结果"));
        m_moduleTimeLabel->setText(QStringLiteral("%1 ms").arg(m_context.elapsedMs));
        showMeasurement(m_context.measurement);
    } else {
        const ModuleRunResult result = m_context.moduleResult(nodeId);
        m_moduleStatusLabel->setText(moduleStatusToString(result.status));
        m_moduleTimeLabel->setText(QStringLiteral("%1 ms").arg(result.elapsedMs));
        showMeasurement(result.measurement.valid ? result.measurement : m_context.measurement);
    }
    emit outputNodeChanged(nodeId);
}

QString MeasurementPanel::selectedOutputNodeId() const
{
    return m_outputCombo->currentData().toString();
}

void MeasurementPanel::setSelectedOutputNodeId(const QString &nodeId)
{
    const int index = m_outputCombo->findData(nodeId);
    if (index >= 0)
        m_outputCombo->setCurrentIndex(index);
}

void MeasurementPanel::clearResults()
{
    m_context = VisionContext{};
    m_outputCombo->blockSignals(true);
    m_outputCombo->clear();
    m_outputCombo->blockSignals(false);
    m_statusLabel->setText(QStringLiteral("未执行"));
    m_moduleStatusLabel->setText(QStringLiteral("-"));
    m_widthLabel->setText(QStringLiteral("-"));
    m_heightLabel->setText(QStringLiteral("-"));
    m_areaLabel->setText(QStringLiteral("-"));
    m_centerLabel->setText(QStringLiteral("-"));
    m_timeLabel->setText(QStringLiteral("-"));
    m_moduleTimeLabel->setText(QStringLiteral("-"));
    m_logEdit->clear();
}
