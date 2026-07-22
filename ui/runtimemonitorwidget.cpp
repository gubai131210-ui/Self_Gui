#include "ui/runtimemonitorwidget.h"

#include "vision/runtime/runtimediagnostic.h"

#include <QAbstractItemView>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QGuiApplication>
#include <QClipboard>

RuntimeMonitorWidget::RuntimeMonitorWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    m_statusLabel = new QLabel(QStringLiteral("运行监控：空闲"), this);
    auto *toolbar = new QHBoxLayout;
    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText(QStringLiteral("按节点/错误码过滤"));
    m_copyButton = new QPushButton(QStringLiteral("复制日志"), this);
    m_clearButton = new QPushButton(QStringLiteral("清空"), this);
    m_taskTable = new QTableWidget(0, 6, this);
    m_taskTable->setHorizontalHeaderLabels(
        {QStringLiteral("任务"), QStringLiteral("状态"), QStringLiteral("耗时(ms)"),
         QStringLiteral("失败类型"), QStringLiteral("诊断码"), QStringLiteral("节点")});
    m_taskTable->horizontalHeader()->setStretchLastSection(true);
    m_taskTable->verticalHeader()->setVisible(false);
    m_taskTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_taskTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_taskTable->setWordWrap(false);
    m_logEdit = new QPlainTextEdit(this);
    m_logEdit->setReadOnly(true);
    layout->addWidget(m_statusLabel);
    toolbar->addWidget(new QLabel(QStringLiteral("筛选"), this));
    toolbar->addWidget(m_filterEdit, 1);
    toolbar->addWidget(m_copyButton);
    toolbar->addWidget(m_clearButton);
    layout->addLayout(toolbar);
    layout->addWidget(m_taskTable, 1);
    layout->addWidget(new QLabel(QStringLiteral("运行日志"), this));
    layout->addWidget(m_logEdit, 1);
    connect(m_filterEdit, &QLineEdit::textChanged, this, [this]() { applyFilter(); });
    connect(m_copyButton, &QPushButton::clicked, this, [this]() {
        QGuiApplication::clipboard()->setText(m_logEdit->toPlainText());
    });
    connect(m_clearButton, &QPushButton::clicked, this, &RuntimeMonitorWidget::clearAll);
}

void RuntimeMonitorWidget::showDiagnostics(const QStringList &lines)
{
    m_lastLines = lines;
    applyFilter();
    m_statusLabel->setText(lines.isEmpty() ? QStringLiteral("运行监控：空闲")
                                           : QStringLiteral("运行监控：已更新"));
}

void RuntimeMonitorWidget::showTasks(const QVector<Selt::ThreadTaskSnapshot> &tasks)
{
    m_lastTasks = tasks;
    applyFilter();
    m_statusLabel->setText(QStringLiteral("运行监控：%1 个节点").arg(tasks.size()));
}

void RuntimeMonitorWidget::applyFilter()
{
    const QString filter = m_filterEdit ? m_filterEdit->text().trimmed() : QString();
    QVector<Selt::ThreadTaskSnapshot> filteredTasks;
    QStringList filteredLines;
    for (const Selt::ThreadTaskSnapshot &task : m_lastTasks) {
        const QString haystack = QStringLiteral("%1 %2 %3 %4 %5")
                                     .arg(task.name, task.state, task.nodeId, task.failureKind,
                                          task.diagnosticCode);
        if (filter.isEmpty() || haystack.contains(filter, Qt::CaseInsensitive))
            filteredTasks.append(task);
    }
    for (const QString &line : m_lastLines) {
        if (filter.isEmpty() || line.contains(filter, Qt::CaseInsensitive))
            filteredLines.append(line);
    }
    m_taskTable->setRowCount(filteredTasks.size());
    for (int i = 0; i < filteredTasks.size(); ++i) {
        const Selt::ThreadTaskSnapshot &task = filteredTasks.at(i);
        m_taskTable->setItem(i, 0, new QTableWidgetItem(task.name));
        m_taskTable->setItem(i, 1, new QTableWidgetItem(task.state));
        m_taskTable->setItem(i, 2, new QTableWidgetItem(QString::number(task.elapsedMs)));
        const QString kind = task.failureKind;
        const QString label = kind.isEmpty()
            ? QStringLiteral("-")
            : Selt::runtimeFailureKindDisplayName(Selt::runtimeFailureKindFromString(kind));
        m_taskTable->setItem(i, 3, new QTableWidgetItem(label));
        m_taskTable->setItem(i, 4, new QTableWidgetItem(
            task.diagnosticCode.isEmpty() ? QStringLiteral("-") : task.diagnosticCode));
        m_taskTable->setItem(i, 5, new QTableWidgetItem(
            task.nodeId.isEmpty() ? QStringLiteral("-") : task.nodeId));
    }
    m_logEdit->setPlainText(filteredLines.join(QLatin1Char('\n')));
}

void RuntimeMonitorWidget::showExecution(const VisionContext &context)
{
    const QString timestamp = context.snapshotCreatedAt.isValid()
        ? context.snapshotCreatedAt.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz"))
        : QStringLiteral("-");
    m_statusLabel->setText(
        QStringLiteral("范围：%1 | 快照：%2 | 帧：%3 | 设备：%4 | 标定：%5 | 节点：%6 | 判定：%7 | 总耗时：%8 ms")
            .arg(context.executionScope, timestamp)
            .arg(context.frameId)
            .arg(context.deviceId.isEmpty() ? QStringLiteral("-") : context.deviceId)
            .arg(context.calibrationId.isEmpty() ? QStringLiteral("-") : context.calibrationId)
            .arg(context.executionOrder.size())
            .arg(context.measurement.decisionState.isEmpty()
                     ? QStringLiteral("-")
                     : context.measurement.decisionState)
            .arg(context.elapsedMs));
}

void RuntimeMonitorWidget::clearAll()
{
    m_taskTable->setRowCount(0);
    m_logEdit->clear();
    m_lastTasks.clear();
    m_lastLines.clear();
    if (m_filterEdit)
        m_filterEdit->clear();
    m_statusLabel->setText(QStringLiteral("运行监控：空闲"));
}
