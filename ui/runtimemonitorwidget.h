#ifndef RUNTIMEMONITORWIDGET_H
#define RUNTIMEMONITORWIDGET_H

#include "vision/runtime/runcontroller.h"
#include "vision/model/visioncontext.h"

#include <QWidget>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;

class RuntimeMonitorWidget : public QWidget
{
    Q_OBJECT
public:
    explicit RuntimeMonitorWidget(QWidget *parent = nullptr);
    void showDiagnostics(const QStringList &lines);
    void showTasks(const QVector<Selt::ThreadTaskSnapshot> &tasks);
    void showExecution(const VisionContext &context);
    void clearAll();

private:
    void applyFilter();

    QLabel *m_statusLabel{nullptr};
    QLineEdit *m_filterEdit{nullptr};
    QPushButton *m_copyButton{nullptr};
    QPushButton *m_clearButton{nullptr};
    QTableWidget *m_taskTable{nullptr};
    QPlainTextEdit *m_logEdit{nullptr};
    QVector<Selt::ThreadTaskSnapshot> m_lastTasks;
    QStringList m_lastLines;
};

#endif
