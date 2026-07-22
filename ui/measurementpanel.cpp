#include "measurementpanel.h"

#include "vision/runtime/runtimediagnostic.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTableWidget>
#include <QVBoxLayout>

MeasurementPanel::MeasurementPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    m_outputCombo = new QComboBox(this);
    layout->addWidget(new QLabel(QStringLiteral("查看模块输出"), this));
    layout->addWidget(m_outputCombo);

    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText(QStringLiteral("过滤节点/类型/判定…"));
    m_filterEdit->setClearButtonEnabled(true);
    layout->addWidget(m_filterEdit);

    m_resultTable = new QTableWidget(0, 8, this);
    m_resultTable->setHorizontalHeaderLabels({
        QStringLiteral("序号"),
        QStringLiteral("节点"),
        QStringLiteral("类型"),
        QStringLiteral("数值"),
        QStringLiteral("单位"),
        QStringLiteral("判定"),
        QStringLiteral("置信度"),
        QStringLiteral("Data Available")});
    m_resultTable->horizontalHeader()->setStretchLastSection(true);
    m_resultTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_resultTable->setAlternatingRowColors(true);
    m_resultTable->verticalHeader()->setVisible(false);
    m_resultTable->setWordWrap(false);
    m_resultTable->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_resultTable->setMaximumHeight(180);
    layout->addWidget(m_resultTable);

    auto *form = new QFormLayout;
    m_statusLabel = new QLabel(QStringLiteral("未执行"), this);
    m_moduleStatusLabel = new QLabel(QStringLiteral("-"), this);
    m_widthLabel = new QLabel(QStringLiteral("-"), this);
    m_heightLabel = new QLabel(QStringLiteral("-"), this);
    m_areaLabel = new QLabel(QStringLiteral("-"), this);
    m_centerLabel = new QLabel(QStringLiteral("-"), this);
    m_unitLabel = new QLabel(QStringLiteral("-"), this);
    m_decisionLabel = new QLabel(QStringLiteral("-"), this);
    m_calibrationLabel = new QLabel(QStringLiteral("-"), this);
    m_frameLabel = new QLabel(QStringLiteral("-"), this);
    m_timeLabel = new QLabel(QStringLiteral("-"), this);
    m_moduleTimeLabel = new QLabel(QStringLiteral("-"), this);
    form->addRow(QStringLiteral("流程状态"), m_statusLabel);
    form->addRow(QStringLiteral("模块状态"), m_moduleStatusLabel);
    form->addRow(QStringLiteral("宽度"), m_widthLabel);
    form->addRow(QStringLiteral("高度"), m_heightLabel);
    form->addRow(QStringLiteral("面积"), m_areaLabel);
    form->addRow(QStringLiteral("中心点"), m_centerLabel);
    form->addRow(QStringLiteral("单位"), m_unitLabel);
    form->addRow(QStringLiteral("判定"), m_decisionLabel);
    form->addRow(QStringLiteral("标定"), m_calibrationLabel);
    form->addRow(QStringLiteral("帧/设备"), m_frameLabel);
    form->addRow(QStringLiteral("总耗时"), m_timeLabel);
    form->addRow(QStringLiteral("模块耗时"), m_moduleTimeLabel);
    layout->addLayout(form);

    m_logEdit = new QPlainTextEdit(this);
    m_logEdit->setReadOnly(true);
    m_logEdit->setMaximumHeight(120);
    layout->addWidget(new QLabel(QStringLiteral("执行日志"), this));
    layout->addWidget(m_logEdit);

    connect(m_outputCombo, &QComboBox::currentIndexChanged, this, &MeasurementPanel::onOutputComboChanged);
    connect(m_filterEdit, &QLineEdit::textChanged, this, &MeasurementPanel::applyTableFilter);
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
    const QString unit = measurement.unit.isEmpty() ? QStringLiteral("px") : measurement.unit;
    m_unitLabel->setText(unit);
    m_decisionLabel->setText(measurement.decisionState.isEmpty()
                                 ? QStringLiteral("unknown")
                                 : measurement.decisionState);
    m_calibrationLabel->setText(measurement.calibrationId.isEmpty()
                                    ? QStringLiteral("(未标定)")
                                    : measurement.calibrationId);
    if (measurement.valid) {
        m_widthLabel->setText(QStringLiteral("%1 %2")
                                   .arg(measurement.width, 0, 'f', 1)
                                   .arg(unit));
        m_heightLabel->setText(QStringLiteral("%1 %2")
                                    .arg(measurement.height, 0, 'f', 1)
                                    .arg(unit));
        m_areaLabel->setText(QStringLiteral("%1 %2²")
                                  .arg(measurement.area, 0, 'f', 1)
                                  .arg(unit));
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
    rebuildResultTable(context);

    if (context.hasError()) {
        m_statusLabel->setText(QStringLiteral("失败: %1").arg(context.errorMessage));
    } else if (context.measurement.valid) {
        m_statusLabel->setText(QStringLiteral("成功"));
    } else {
        m_statusLabel->setText(QStringLiteral("完成"));
    }

    m_timeLabel->setText(QStringLiteral("%1 ms").arg(context.elapsedMs));
    m_frameLabel->setText(QStringLiteral("frame=%1 / %2")
                              .arg(context.frameId)
                              .arg(context.deviceId.isEmpty() ? QStringLiteral("-") : context.deviceId));

    QStringList logLines = context.log;
    if (!context.diagnostics.isEmpty()) {
        logLines.append(QStringLiteral("--- 静态/绑定诊断 ---"));
        for (const Selt::GraphDiagnostic &d : context.diagnostics) {
            QString kind = QStringLiteral("错误");
            if (d.severity == Selt::GraphDiagnosticSeverity::Warning)
                kind = QStringLiteral("警告");
            else if (d.severity == Selt::GraphDiagnosticSeverity::Info)
                kind = QStringLiteral("信息");
            logLines.append(QStringLiteral("[%1] %2 (%3) node=%4 param=%5")
                                .arg(kind, d.message, d.code, d.nodeId, d.parameterKey));
        }
    }
    m_logEdit->setPlainText(logLines.join(QLatin1Char('\n')));

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
        QString statusText = moduleStatusToString(result.status);
        if (!result.failureKind.isEmpty()) {
            QString kindLabel = Selt::runtimeFailureKindDisplayName(
                Selt::runtimeFailureKindFromString(result.failureKind));
            if (kindLabel == QLatin1String("无"))
                kindLabel = result.failureKind;
            statusText = QStringLiteral("%1 / %2").arg(statusText, kindLabel);
        }
        if (result.status == ModuleStatus::Disabled)
            statusText = QStringLiteral("被禁用");
        if (!result.errorMessage.isEmpty())
            statusText += QStringLiteral(" — %1").arg(result.errorMessage);
        if (!result.outputSummary.isEmpty()) {
            QStringList outs;
            for (auto it = result.outputSummary.cbegin(); it != result.outputSummary.cend(); ++it)
                outs.append(QStringLiteral("%1=%2").arg(it.key(), it.value()));
            statusText += QStringLiteral(" | 输出: %1").arg(outs.join(QStringLiteral(", ")));
        }
        m_moduleStatusLabel->setText(statusText);
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

void MeasurementPanel::rebuildResultTable(const VisionContext &context)
{
    if (!m_resultTable)
        return;
    m_resultTable->setRowCount(0);
    int row = 0;
    auto appendRow = [&](const QString &nodeLabel, const MeasurementResult &m) {
        m_resultTable->insertRow(row);
        const double primary = m.width > 0 ? m.width : (m.area > 0 ? m.area : m.height);
        m_resultTable->setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));
        m_resultTable->setItem(row, 1, new QTableWidgetItem(nodeLabel));
        m_resultTable->setItem(row, 2, new QTableWidgetItem(
            m.measurementType.isEmpty() ? QStringLiteral("-") : m.measurementType));
        m_resultTable->setItem(row, 3, new QTableWidgetItem(
            m.valid ? QString::number(primary, 'f', 3) : QStringLiteral("-")));
        m_resultTable->setItem(row, 4, new QTableWidgetItem(
            m.unit.isEmpty() ? QStringLiteral("px") : m.unit));
        m_resultTable->setItem(row, 5, new QTableWidgetItem(
            m.decisionState.isEmpty() ? QStringLiteral("unknown") : m.decisionState));
        m_resultTable->setItem(row, 6, new QTableWidgetItem(
            m.valid ? QString::number(m.confidence, 'f', 3) : QStringLiteral("-")));
        m_resultTable->setItem(row, 7, new QTableWidgetItem(m.valid ? QStringLiteral("True")
                                                                   : QStringLiteral("False")));
        ++row;
    };

    for (const QString &nodeId : context.executionOrder) {
        const ModuleRunResult result = context.moduleResult(nodeId);
        const QString label = result.displayName.isEmpty() ? nodeId : result.displayName;
        if (result.measurement.valid || !result.measurement.measurementType.isEmpty()) {
            appendRow(label, result.measurement);
            continue;
        }
        if (result.status == ModuleStatus::Failed || !result.errorMessage.isEmpty()) {
            MeasurementResult failed;
            failed.valid = false;
            failed.measurementType = result.failureKind.isEmpty()
                ? QStringLiteral("error")
                : result.failureKind;
            failed.decisionState = QStringLiteral("fail");
            failed.unit = QStringLiteral("-");
            appendRow(label, failed);
        }
    }
    if (row == 0 && context.measurement.valid)
        appendRow(QStringLiteral("最终结果"), context.measurement);
    if (row == 0 && context.hasError()) {
        MeasurementResult failed;
        failed.valid = false;
        failed.measurementType = QStringLiteral("error");
        failed.decisionState = QStringLiteral("fail");
        failed.unit = QStringLiteral("-");
        appendRow(QStringLiteral("流程失败"), failed);
    }
    applyTableFilter();
}

void MeasurementPanel::applyTableFilter()
{
    if (!m_resultTable)
        return;
    const QString filter = m_filterEdit ? m_filterEdit->text().trimmed() : QString();
    for (int row = 0; row < m_resultTable->rowCount(); ++row) {
        if (filter.isEmpty()) {
            m_resultTable->setRowHidden(row, false);
            continue;
        }
        bool match = false;
        for (int col = 0; col < m_resultTable->columnCount(); ++col) {
            const QTableWidgetItem *item = m_resultTable->item(row, col);
            if (item && item->text().contains(filter, Qt::CaseInsensitive)) {
                match = true;
                break;
            }
        }
        m_resultTable->setRowHidden(row, !match);
    }
}

void MeasurementPanel::clearResults()
{
    m_context = VisionContext{};
    m_outputCombo->blockSignals(true);
    m_outputCombo->clear();
    m_outputCombo->blockSignals(false);
    if (m_resultTable)
        m_resultTable->setRowCount(0);
    m_statusLabel->setText(QStringLiteral("未执行"));
    m_moduleStatusLabel->setText(QStringLiteral("-"));
    m_widthLabel->setText(QStringLiteral("-"));
    m_heightLabel->setText(QStringLiteral("-"));
    m_areaLabel->setText(QStringLiteral("-"));
    m_centerLabel->setText(QStringLiteral("-"));
    m_unitLabel->setText(QStringLiteral("-"));
    m_decisionLabel->setText(QStringLiteral("-"));
    m_calibrationLabel->setText(QStringLiteral("-"));
    m_frameLabel->setText(QStringLiteral("-"));
    m_timeLabel->setText(QStringLiteral("-"));
    m_moduleTimeLabel->setText(QStringLiteral("-"));
    m_logEdit->clear();
}
