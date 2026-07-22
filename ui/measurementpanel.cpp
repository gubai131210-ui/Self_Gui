#include "measurementpanel.h"

#include "ui/theme/uistyle.h"
#include "ui/widgets/collapsiblesection.h"
#include "vision/data/datatype.h"
#include "vision/model/portexposure.h"
#include "vision/registry/visionnoderegistry.h"
#include "vision/runtime/runtimediagnostic.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QApplication>
#include <QBrush>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

constexpr int kDetailRole = Qt::UserRole + 11;

bool splitDebugString(const QString &debug, QString *typeOut, QString *valueOut)
{
    const int lp = debug.indexOf(QLatin1Char('('));
    const int rp = debug.lastIndexOf(QLatin1Char(')'));
    if (lp > 0 && rp > lp) {
        if (typeOut)
            *typeOut = debug.left(lp);
        if (valueOut)
            *valueOut = debug.mid(lp + 1, rp - lp - 1);
        return true;
    }
    if (typeOut)
        *typeOut = QStringLiteral("-");
    if (valueOut)
        *valueOut = debug;
    return false;
}

QString categoryForTypeName(const QString &typeName)
{
    const QString t = typeName.toLower();
    if (t == QLatin1String("image") || t == QLatin1String("overlay"))
        return QStringLiteral("图像/叠加");
    if (t == QLatin1String("point2d") || t == QLatin1String("line") || t == QLatin1String("circle")
        || t == QLatin1String("contour") || t == QLatin1String("roi") || t == QLatin1String("region"))
        return QStringLiteral("几何");
    if (t == QLatin1String("measurement"))
        return QStringLiteral("测量");
    if (t == QLatin1String("bool") || t == QLatin1String("int") || t == QLatin1String("real")
        || t == QLatin1String("string"))
        return QStringLiteral("标量");
    if (t == QLatin1String("table") || t == QLatin1String("bytearray"))
        return QStringLiteral("表格/二进制");
    if (t == QLatin1String("meta") || typeName.isEmpty())
        return QStringLiteral("元数据");
    return QStringLiteral("其他");
}

QString localizePortGroup(const QString &group)
{
    if (group == QLatin1String("Image"))
        return QStringLiteral("图像");
    if (group == QLatin1String("ROI"))
        return QStringLiteral("ROI");
    if (group == QLatin1String("Geometry"))
        return QStringLiteral("几何");
    if (group == QLatin1String("Measurement"))
        return QStringLiteral("测量");
    if (group == QLatin1String("Data"))
        return QStringLiteral("数据");
    if (group == QLatin1String("Debug"))
        return QStringLiteral("调试");
    return group.isEmpty() ? QStringLiteral("其他") : group;
}

QString typeDisplayName(const QString &typeName)
{
    bool ok = false;
    const Selt::DataTypeId id = Selt::dataTypeIdFromString(typeName, &ok);
    if (ok && id != Selt::DataTypeId::None)
        return Selt::dataTypeIdDisplayName(id);
    return typeName.isEmpty() ? QStringLiteral("-") : typeName;
}

QString statusDisplayName(const QString &status)
{
    if (status == QLatin1String("ok") || status == QLatin1String("success"))
        return QStringLiteral("成功");
    if (status == QLatin1String("warn") || status == QLatin1String("warning"))
        return QStringLiteral("警告");
    if (status == QLatin1String("fail") || status == QLatin1String("failed")
        || status == QLatin1String("ng"))
        return QStringLiteral("失败");
    if (status == QLatin1String("capability_missing") || status == QLatin1String("backend_missing"))
        return QStringLiteral("能力缺失");
    return status.isEmpty() ? QStringLiteral("成功") : status;
}

QString summaryValueFromDebug(const QString &debug)
{
    QString type;
    QString value;
    if (splitDebugString(debug, &type, &value))
        return value;
    return debug;
}

} // namespace

MeasurementPanel::MeasurementPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(Selt::UiStyle::panelMargin,
                               Selt::UiStyle::panelMargin,
                               Selt::UiStyle::panelMargin,
                               Selt::UiStyle::panelMargin);
    layout->setSpacing(Selt::UiStyle::compactSpacing);

    m_outputCombo = new QComboBox(this);
    layout->addWidget(new QLabel(QStringLiteral("查看模块输出"), this));
    layout->addWidget(m_outputCombo);

    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText(QStringLiteral("过滤：节点 / 端口 / 类型 / 诊断码 / 状态…"));
    m_filterEdit->setClearButtonEnabled(true);
    layout->addWidget(m_filterEdit);

    auto *summarySection = new CollapsibleSection(QStringLiteral("测量汇总"),
                                                  QStringLiteral("results/section/测量汇总"),
                                                  true,
                                                  this);
    m_resultTable = new QTableWidget(0, 8, summarySection);
    m_resultTable->setHorizontalHeaderLabels({
        QStringLiteral("序号"),
        QStringLiteral("节点"),
        QStringLiteral("类型"),
        QStringLiteral("数值"),
        QStringLiteral("单位"),
        QStringLiteral("判定"),
        QStringLiteral("置信度"),
        QStringLiteral("有效")});
    m_resultTable->horizontalHeader()->setStretchLastSection(true);
    m_resultTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_resultTable->setAlternatingRowColors(true);
    m_resultTable->verticalHeader()->setVisible(false);
    m_resultTable->setWordWrap(false);
    m_resultTable->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_resultTable->setMaximumHeight(160);
    summarySection->contentLayout()->addWidget(m_resultTable);
    layout->addWidget(summarySection);

    auto *outputSection = new CollapsibleSection(QStringLiteral("输出端口（按类型）"),
                                                 QStringLiteral("results/section/输出端口"),
                                                 true,
                                                 this);
    m_outputDataTable = new QTableWidget(0, 6, outputSection);
    m_outputDataTable->setHorizontalHeaderLabels({
        QStringLiteral("分类"),
        QStringLiteral("端口ID"),
        QStringLiteral("名称"),
        QStringLiteral("数据类型"),
        QStringLiteral("值"),
        QStringLiteral("状态")});
    m_outputDataTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_outputDataTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_outputDataTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_outputDataTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_outputDataTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_outputDataTable->horizontalHeader()->setStretchLastSection(true);
    m_outputDataTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_outputDataTable->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_outputDataTable->setAlternatingRowColors(true);
    m_outputDataTable->verticalHeader()->setVisible(false);
    m_outputDataTable->setWordWrap(true);
    m_outputDataTable->setTextElideMode(Qt::ElideNone);
    m_outputDataTable->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_outputDataTable->setMinimumHeight(140);
    m_outputDataTable->setToolTip(
        QStringLiteral("双击查看详情；右键可复制端口ID/类型/值/整行"));
    outputSection->contentLayout()->addWidget(m_outputDataTable);
    layout->addWidget(outputSection, 1);

    auto *statusRow = new QHBoxLayout;
    m_statusLabel = new QLabel(QStringLiteral("未执行"), this);
    m_moduleStatusLabel = new QLabel(QStringLiteral("-"), this);
    m_timeLabel = new QLabel(QStringLiteral("-"), this);
    m_moduleTimeLabel = new QLabel(QStringLiteral("-"), this);
    m_frameLabel = new QLabel(QStringLiteral("-"), this);
    const QString statusCss = QStringLiteral("color: %1;").arg(Selt::UiStyle::textPrimary().name());
    for (QLabel *lab : {m_statusLabel, m_moduleStatusLabel, m_timeLabel, m_moduleTimeLabel, m_frameLabel}) {
        lab->setStyleSheet(statusCss);
        lab->setWordWrap(true);
    }
    statusRow->addWidget(new QLabel(QStringLiteral("流程:"), this));
    statusRow->addWidget(m_statusLabel, 1);
    statusRow->addWidget(new QLabel(QStringLiteral("模块:"), this));
    statusRow->addWidget(m_moduleStatusLabel, 1);
    layout->addLayout(statusRow);

    m_diagnosisLabel = new QLabel(QStringLiteral("诊断：-"), this);
    m_diagnosisLabel->setObjectName(QStringLiteral("ResultDiagnosisLabel"));
    m_diagnosisLabel->setWordWrap(true);
    m_diagnosisLabel->setStyleSheet(
        QStringLiteral("QLabel#ResultDiagnosisLabel {"
                       " color: %1; background-color: %2; border: 1px solid %3;"
                       " border-radius: 4px; padding: 6px 8px; }")
            .arg(Selt::UiStyle::textPrimary().name(),
                 Selt::UiStyle::panelAltBackground().name(),
                 Selt::UiStyle::border().name()));
    layout->addWidget(m_diagnosisLabel);

    auto *metaRow = new QHBoxLayout;
    metaRow->addWidget(new QLabel(QStringLiteral("总耗时:"), this));
    metaRow->addWidget(m_timeLabel);
    metaRow->addSpacing(12);
    metaRow->addWidget(new QLabel(QStringLiteral("模块耗时:"), this));
    metaRow->addWidget(m_moduleTimeLabel);
    metaRow->addSpacing(12);
    metaRow->addWidget(new QLabel(QStringLiteral("帧/设备:"), this));
    metaRow->addWidget(m_frameLabel, 1);
    layout->addLayout(metaRow);

    auto *logSection = new CollapsibleSection(QStringLiteral("执行日志"),
                                              QStringLiteral("results/section/执行日志"),
                                              false,
                                              this);
    m_logEdit = new QPlainTextEdit(logSection);
    m_logEdit->setReadOnly(true);
    m_logEdit->setMaximumHeight(120);
    logSection->contentLayout()->addWidget(m_logEdit);
    layout->addWidget(logSection);

    connect(m_outputCombo, &QComboBox::currentIndexChanged, this, &MeasurementPanel::onOutputComboChanged);
    connect(m_filterEdit, &QLineEdit::textChanged, this, &MeasurementPanel::applyTableFilter);
    connect(m_outputDataTable, &QTableWidget::cellDoubleClicked, this, [this](int, int) {
        showSelectedOutputDetail();
    });
    m_outputDataTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_outputDataTable, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        const QModelIndex idx = m_outputDataTable->indexAt(pos);
        if (!idx.isValid())
            return;
        m_outputDataTable->setCurrentCell(idx.row(), idx.column());
        QMenu menu(this);
        menu.addAction(QStringLiteral("查看详情"), this, &MeasurementPanel::showSelectedOutputDetail);
        menu.addAction(QStringLiteral("复制单元格"), this, &MeasurementPanel::copySelectedOutputCell);
        menu.addAction(QStringLiteral("复制整行"), this, &MeasurementPanel::copySelectedOutputRow);
        menu.addAction(QStringLiteral("复制端口ID"), this, [this, idx]() {
            if (const QTableWidgetItem *item = m_outputDataTable->item(idx.row(), 1))
                QApplication::clipboard()->setText(item->text());
        });
        menu.addAction(QStringLiteral("复制类型"), this, [this, idx]() {
            if (const QTableWidgetItem *item = m_outputDataTable->item(idx.row(), 3))
                QApplication::clipboard()->setText(item->text());
        });
        menu.addAction(QStringLiteral("复制值"), this, [this, idx]() {
            if (const QTableWidgetItem *item = m_outputDataTable->item(idx.row(), 4)) {
                const QString detail = item->data(kDetailRole).toString();
                QApplication::clipboard()->setText(detail.isEmpty() ? item->text() : detail);
            }
        });
        menu.exec(m_outputDataTable->viewport()->mapToGlobal(pos));
    });
}

void MeasurementPanel::copySelectedOutputCell()
{
    if (!m_outputDataTable)
        return;
    const QTableWidgetItem *item = m_outputDataTable->currentItem();
    if (!item)
        return;
    const QString detail = item->data(kDetailRole).toString();
    QApplication::clipboard()->setText(detail.isEmpty() ? item->text() : detail);
}

void MeasurementPanel::copySelectedOutputRow()
{
    if (!m_outputDataTable)
        return;
    const int row = m_outputDataTable->currentRow();
    if (row < 0)
        return;
    QStringList cols;
    for (int col = 0; col < m_outputDataTable->columnCount(); ++col) {
        const QTableWidgetItem *item = m_outputDataTable->item(row, col);
        cols << (item ? item->text() : QString());
    }
    QApplication::clipboard()->setText(cols.join(QLatin1Char('\t')));
}

void MeasurementPanel::showSelectedOutputDetail()
{
    if (!m_outputDataTable)
        return;
    const int row = m_outputDataTable->currentRow();
    if (row < 0)
        return;
    const QTableWidgetItem *portItem = m_outputDataTable->item(row, 1);
    const QTableWidgetItem *nameItem = m_outputDataTable->item(row, 2);
    const QTableWidgetItem *typeItem = m_outputDataTable->item(row, 3);
    const QTableWidgetItem *valueItem = m_outputDataTable->item(row, 4);
    const QTableWidgetItem *statusItem = m_outputDataTable->item(row, 5);
    const QString detail = valueItem ? valueItem->data(kDetailRole).toString() : QString();
    const QString body = detail.isEmpty() && valueItem ? valueItem->text() : detail;

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("端口详情"));
    dialog.resize(480, 320);
    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(
        QStringLiteral("端口：%1  名称：%2\n类型：%3  状态：%4")
            .arg(portItem ? portItem->text() : QStringLiteral("-"),
                 nameItem ? nameItem->text() : QStringLiteral("-"),
                 typeItem ? typeItem->text() : QStringLiteral("-"),
                 statusItem ? statusItem->text() : QStringLiteral("-")),
        &dialog));
    auto *edit = new QPlainTextEdit(body, &dialog);
    edit->setReadOnly(true);
    layout->addWidget(edit, 1);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QPushButton *copyBtn = buttons->addButton(QStringLiteral("复制详情"), QDialogButtonBox::ActionRole);
    connect(copyBtn, &QPushButton::clicked, &dialog, [body]() {
        QApplication::clipboard()->setText(body);
    });
    layout->addWidget(buttons);
    dialog.exec();
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

void MeasurementPanel::appendOutputRow(const QString &category,
                                       const QString &portId,
                                       const QString &displayName,
                                       const QString &typeName,
                                       const QString &valueText,
                                       const QString &status,
                                       const QString &detailText)
{
    if (!m_outputDataTable)
        return;
    const int row = m_outputDataTable->rowCount();
    m_outputDataTable->insertRow(row);
    m_outputDataTable->setItem(row, 0, new QTableWidgetItem(category));
    m_outputDataTable->setItem(row, 1, new QTableWidgetItem(portId));
    m_outputDataTable->setItem(row, 2, new QTableWidgetItem(displayName));

    auto *typeItem = new QTableWidgetItem(typeDisplayName(typeName));
    typeItem->setForeground(QBrush(Selt::UiStyle::runningBlue()));
    typeItem->setToolTip(typeName.isEmpty() ? typeDisplayName(typeName) : typeName);
    typeItem->setData(kDetailRole, typeName);
    m_outputDataTable->setItem(row, 3, typeItem);

    auto *valueItem = new QTableWidgetItem(valueText);
    const QString detail = detailText.isEmpty() ? valueText : detailText;
    valueItem->setToolTip(detail);
    valueItem->setData(kDetailRole, detail);
    m_outputDataTable->setItem(row, 4, valueItem);

    const QString statusLabel = statusDisplayName(status);
    auto *statusItem = new QTableWidgetItem(statusLabel);
    statusItem->setData(kDetailRole, status);
    if (status.contains(QStringLiteral("fail"), Qt::CaseInsensitive)
        || status.contains(QStringLiteral("ng"), Qt::CaseInsensitive)
        || status.contains(QStringLiteral("缺失"))
        || status.contains(QStringLiteral("capability"), Qt::CaseInsensitive)
        || status.contains(QStringLiteral("backend"), Qt::CaseInsensitive)
        || statusLabel.contains(QStringLiteral("失败"))
        || statusLabel.contains(QStringLiteral("能力缺失"))) {
        statusItem->setForeground(QBrush(Selt::UiStyle::failRed()));
    } else if (status.contains(QStringLiteral("warn"), Qt::CaseInsensitive)
               || status.contains(QStringLiteral("低"))
               || statusLabel.contains(QStringLiteral("警告"))) {
        statusItem->setForeground(QBrush(Selt::UiStyle::accentOrange()));
    } else {
        statusItem->setForeground(QBrush(Selt::UiStyle::successGreen()));
    }
    m_outputDataTable->setItem(row, 5, statusItem);
}

void MeasurementPanel::showMeasurementRows(const MeasurementResult &measurement)
{
    const QString unit = measurement.unit.isEmpty() ? QStringLiteral("px") : measurement.unit;
    const QString cat = QStringLiteral("测量");
    const QString st = measurement.valid ? QStringLiteral("ok")
                                         : (measurement.decisionState.isEmpty()
                                                ? QStringLiteral("ng")
                                                : measurement.decisionState);
    if (!measurement.measurementType.isEmpty())
        appendOutputRow(cat, QStringLiteral("measurementType"), QStringLiteral("测量类型"),
                        QStringLiteral("String"), measurement.measurementType, st);
    appendOutputRow(cat, QStringLiteral("decision"), QStringLiteral("判定"),
                    QStringLiteral("String"),
                    measurement.decisionState.isEmpty() ? QStringLiteral("unknown")
                                                        : measurement.decisionState,
                    st);
    appendOutputRow(cat, QStringLiteral("unit"), QStringLiteral("单位"),
                    QStringLiteral("String"), unit, st);
    appendOutputRow(cat, QStringLiteral("calibrationId"), QStringLiteral("标定"),
                    QStringLiteral("String"),
                    measurement.calibrationId.isEmpty() ? QStringLiteral("(未标定)")
                                                        : measurement.calibrationId,
                    st);
    if (!measurement.valid)
        return;
    appendOutputRow(cat, QStringLiteral("width"), QStringLiteral("宽度"),
                    QStringLiteral("Real"),
                    QStringLiteral("%1 %2").arg(measurement.width, 0, 'f', 1).arg(unit), st);
    appendOutputRow(cat, QStringLiteral("height"), QStringLiteral("高度"),
                    QStringLiteral("Real"),
                    QStringLiteral("%1 %2").arg(measurement.height, 0, 'f', 1).arg(unit), st);
    appendOutputRow(cat, QStringLiteral("area"), QStringLiteral("面积"),
                    QStringLiteral("Real"),
                    QStringLiteral("%1 %2²").arg(measurement.area, 0, 'f', 1).arg(unit), st);
    appendOutputRow(cat, QStringLiteral("center"), QStringLiteral("中心点"),
                    QStringLiteral("Point2D"),
                    QStringLiteral("(%1, %2)")
                        .arg(measurement.center.x(), 0, 'f', 1)
                        .arg(measurement.center.y(), 0, 'f', 1),
                    st);
    appendOutputRow(cat, QStringLiteral("confidence"), QStringLiteral("置信度"),
                    QStringLiteral("Real"), QString::number(measurement.confidence, 'f', 3), st);
}

void MeasurementPanel::updateDiagnosisPanel(const ModuleRunResult *moduleResult)
{
    if (!m_diagnosisLabel)
        return;
    if (!moduleResult) {
        if (m_context.hasError()) {
            m_diagnosisLabel->setText(
                QStringLiteral("诊断：流程失败 — %1").arg(m_context.errorMessage));
            m_diagnosisLabel->setStyleSheet(
                QStringLiteral("QLabel#ResultDiagnosisLabel { color: %1; background-color: %2;"
                               " border: 1px solid %3; border-radius: 4px; padding: 6px 8px; }")
                    .arg(Selt::UiStyle::failRed().name(),
                         Selt::UiStyle::panelAltBackground().name(),
                         Selt::UiStyle::border().name()));
        } else {
            m_diagnosisLabel->setText(QStringLiteral("诊断：最终结果（选择具体模块可查看端口诊断）"));
            m_diagnosisLabel->setStyleSheet(
                QStringLiteral("QLabel#ResultDiagnosisLabel { color: %1; background-color: %2;"
                               " border: 1px solid %3; border-radius: 4px; padding: 6px 8px; }")
                    .arg(Selt::UiStyle::textPrimary().name(),
                         Selt::UiStyle::panelAltBackground().name(),
                         Selt::UiStyle::border().name()));
        }
        return;
    }

    const QString code = moduleResult->diagnosticCode;
    auto portText = [&](const QString &portId) -> QString {
        for (const PortValueRecord &rec : moduleResult->outputPortRecords) {
            if (rec.portId == portId)
                return rec.valueSummary;
        }
        return summaryValueFromDebug(moduleResult->outputSummary.value(portId));
    };

    const QString stage = portText(QStringLiteral("failureStage"));
    const QString strategy = portText(QStringLiteral("strategy"));
    const QString attempts = portText(QStringLiteral("attempts"));
    const QString backend = portText(QStringLiteral("backendStatus"));

    QString advice;
    if (code == QLatin1String("backend_missing") || code == QLatin1String("capability_limited"))
        advice = QStringLiteral("建议：检查 OpenCV barcode/QR 编译能力，或改用可用码制。");
    else if (code == QLatin1String("no_candidate") || code == QLatin1String("no_target"))
        advice = QStringLiteral("建议：缩小 ROI、提高分辨率、开启增强模式/CLAHE/反色。");
    else if (code == QLatin1String("decode_failed"))
        advice = QStringLiteral("建议：已检测到码区但解码失败，尝试放大、锐化或反色。");
    else if (code == QLatin1String("quality_low"))
        advice = QStringLiteral("建议：降低最低置信度，或改善光照与对焦。");
    else if (code.isEmpty() && moduleResult->status == ModuleStatus::Success)
        advice = QStringLiteral("识别链路正常。");

    QStringList parts;
    parts << QStringLiteral("诊断码=%1").arg(code.isEmpty() ? QStringLiteral("-") : code);
    if (!stage.isEmpty())
        parts << QStringLiteral("阶段=%1").arg(stage);
    if (!strategy.isEmpty())
        parts << QStringLiteral("策略=%1").arg(strategy);
    if (!attempts.isEmpty())
        parts << QStringLiteral("尝试=%1").arg(attempts);
    if (!moduleResult->errorMessage.isEmpty())
        parts << moduleResult->errorMessage;
    if (!backend.isEmpty())
        parts << backend;
    if (!advice.isEmpty())
        parts << advice;

    m_diagnosisLabel->setText(QStringLiteral("诊断：%1").arg(parts.join(QStringLiteral(" | "))));
    const bool bad = !code.isEmpty() && code != QLatin1String("none");
    m_diagnosisLabel->setStyleSheet(
        QStringLiteral("QLabel#ResultDiagnosisLabel { color: %1; background-color: %2;"
                       " border: 1px solid %3; border-radius: 4px; padding: 6px 8px; }")
            .arg(bad ? Selt::UiStyle::accentOrange().name() : Selt::UiStyle::textPrimary().name(),
                 Selt::UiStyle::panelAltBackground().name(),
                 Selt::UiStyle::border().name()));
}

void MeasurementPanel::rebuildOutputDataTable(const ModuleRunResult *moduleResult)
{
    if (!m_outputDataTable)
        return;
    m_outputDataTable->setRowCount(0);

    if (!moduleResult) {
        if (!m_context.measurement.valid && !m_context.hasError()
            && m_context.executionOrder.isEmpty()) {
            appendOutputRow(QStringLiteral("元数据"), QStringLiteral("-"), QStringLiteral("空结果"),
                            QStringLiteral("String"), QStringLiteral("尚未运行流程"),
                            QStringLiteral("warn"));
        } else {
            showMeasurementRows(m_context.measurement);
        }
        updateDiagnosisPanel(nullptr);
        applyTableFilter();
        return;
    }

    ModuleDescriptor desc;
    if (!moduleResult->type.isEmpty())
        desc = VisionNodeRegistry::descriptor(moduleResult->type);

    const QString rowStatus = moduleResult->diagnosticCode.isEmpty()
        ? QStringLiteral("ok")
        : (moduleResult->diagnosticCode == QLatin1String("backend_missing")
               ? QStringLiteral("capability_missing")
               : (moduleResult->status == ModuleStatus::Failed ? QStringLiteral("fail")
                                                              : QStringLiteral("warn")));

    if (!moduleResult->outputPortRecords.isEmpty()) {
        for (const PortValueRecord &rec : moduleResult->outputPortRecords) {
            if (rec.portId == QLatin1String("diagnosticCode"))
                continue;
            QString category = localizePortGroup(rec.category);
            if (category == QLatin1String("其他") || category.isEmpty())
                category = categoryForTypeName(rec.typeId);
            appendOutputRow(category,
                            rec.portId,
                            rec.displayName.isEmpty() ? rec.portId : rec.displayName,
                            rec.typeId,
                            rec.valueSummary,
                            rec.status.isEmpty() ? rowStatus : rec.status,
                            rec.valueDetail);
        }
    } else {
        QStringList keys = moduleResult->outputSummary.keys();
        keys.sort(Qt::CaseInsensitive);
        for (const QString &portId : keys) {
            if (portId == QLatin1String("diagnosticCode"))
                continue;
            const QString debug = moduleResult->outputSummary.value(portId);
            QString parsedType;
            QString parsedValue;
            splitDebugString(debug, &parsedType, &parsedValue);
            QString typeName = moduleResult->outputTypes.value(portId);
            if (typeName.isEmpty())
                typeName = parsedType;
            const QString valueText = parsedValue.isEmpty() ? debug : parsedValue;

            QString displayName = portId;
            QString category;
            if (const ModulePortDef *port = desc.findOutputPort(portId)) {
                displayName = port->name.isEmpty() ? portId : port->name;
                category = localizePortGroup(port->group.isEmpty()
                    ? Selt::defaultPortGroup(port->dataType, false)
                    : port->group);
                if (typeName.isEmpty())
                    typeName = Selt::dataTypeIdToString(port->dataType);
            } else {
                category = categoryForTypeName(typeName);
            }
            appendOutputRow(category, portId, displayName, typeName, valueText, rowStatus, debug);
        }
    }

    if (!moduleResult->diagnosticCode.isEmpty()) {
        appendOutputRow(QStringLiteral("元数据"), QStringLiteral("diagnosticCode"),
                        QStringLiteral("诊断码"), QStringLiteral("String"),
                        moduleResult->diagnosticCode, rowStatus);
    }

    const MeasurementResult &m = moduleResult->measurement.valid
        ? moduleResult->measurement
        : m_context.measurement;
    showMeasurementRows(m);
    updateDiagnosisPanel(moduleResult);
    applyTableFilter();
}

void MeasurementPanel::showContext(const VisionContext &context, const QString &preferredNodeId)
{
    m_context = context;
    rebuildOutputCombo(context, preferredNodeId);
    rebuildResultTable(context);

    if (context.hasError()) {
        m_statusLabel->setText(QStringLiteral("失败: %1").arg(context.errorMessage));
        m_statusLabel->setStyleSheet(
            QStringLiteral("color: %1;").arg(Selt::UiStyle::failRed().name()));
    } else if (context.measurement.valid) {
        m_statusLabel->setText(QStringLiteral("成功"));
        m_statusLabel->setStyleSheet(
            QStringLiteral("color: %1;").arg(Selt::UiStyle::successGreen().name()));
    } else {
        m_statusLabel->setText(QStringLiteral("完成"));
        m_statusLabel->setStyleSheet(
            QStringLiteral("color: %1;").arg(Selt::UiStyle::textPrimary().name()));
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
        rebuildOutputDataTable(nullptr);
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
        m_moduleStatusLabel->setText(statusText);
        m_moduleTimeLabel->setText(QStringLiteral("%1 ms").arg(result.elapsedMs));
        rebuildOutputDataTable(&result);
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
        m_resultTable->setItem(row, 7, new QTableWidgetItem(m.valid ? QStringLiteral("是")
                                                                   : QStringLiteral("否")));
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
    const QString filter = m_filterEdit ? m_filterEdit->text().trimmed() : QString();
    auto filterTable = [&](QTableWidget *table) {
        if (!table)
            return;
        for (int row = 0; row < table->rowCount(); ++row) {
            if (filter.isEmpty()) {
                table->setRowHidden(row, false);
                continue;
            }
            bool match = false;
            for (int col = 0; col < table->columnCount(); ++col) {
                const QTableWidgetItem *item = table->item(row, col);
                if (item && item->text().contains(filter, Qt::CaseInsensitive)) {
                    match = true;
                    break;
                }
                if (item) {
                    const QString detail = item->data(kDetailRole).toString();
                    if (!detail.isEmpty() && detail.contains(filter, Qt::CaseInsensitive)) {
                        match = true;
                        break;
                    }
                }
            }
            table->setRowHidden(row, !match);
        }
    };
    filterTable(m_resultTable);
    filterTable(m_outputDataTable);
}

void MeasurementPanel::clearResults()
{
    m_context = VisionContext{};
    m_outputCombo->blockSignals(true);
    m_outputCombo->clear();
    m_outputCombo->blockSignals(false);
    if (m_resultTable)
        m_resultTable->setRowCount(0);
    if (m_outputDataTable)
        m_outputDataTable->setRowCount(0);
    m_statusLabel->setText(QStringLiteral("未执行"));
    m_statusLabel->setStyleSheet(
        QStringLiteral("color: %1;").arg(Selt::UiStyle::textPrimary().name()));
    m_moduleStatusLabel->setText(QStringLiteral("-"));
    if (m_diagnosisLabel)
        m_diagnosisLabel->setText(QStringLiteral("诊断：-"));
    m_timeLabel->setText(QStringLiteral("-"));
    m_moduleTimeLabel->setText(QStringLiteral("-"));
    m_frameLabel->setText(QStringLiteral("-"));
    m_logEdit->clear();
}
