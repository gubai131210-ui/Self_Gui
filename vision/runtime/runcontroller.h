#ifndef RUNCONTROLLER_H
#define RUNCONTROLLER_H

#include "core/model/document.h"
#include "vision/model/calibrationmodel.h"
#include "vision/model/modulestatus.h"
#include "vision/model/visioncontext.h"
#include "vision/runtime/flowscheduler.h"
#include "vision/runtime/inodeexecutor.h"
#include "vision/runtime/executionscope.h"

#include <QObject>
#include <QTimer>
#include <QVector>
#include <memory>

namespace Selt {

class InputSourceService;
class ProjectResourceStore;
class ProjectVariableStore;

struct ThreadTaskSnapshot
{
    QString name;
    QString state;
    qint64 elapsedMs{0};
    QString nodeId;
    QString failureKind;
    QString diagnosticCode;
};

class RunController : public QObject
{
    Q_OBJECT
public:
    explicit RunController(QObject *parent = nullptr);

    void setDocument(Document *document);
    void setProjectVariables(const ProjectVariableStore *variables);
    void setFlowVariables(const ProjectVariableStore *flowVariables, const QString &flowId = {});
    void setResourceStore(const ProjectResourceStore *resources);
    void setScheduler(FlowSchedulerPtr scheduler);
    void setInputSource(InputSourceService *source);
    void setCalibration(const CalibrationModel &calibration);
    void clearCalibration();
    void setBatchId(const QString &batchId) { m_batchId = batchId; }
    FlowSchedulerPtr scheduler() const { return m_scheduler; }

    bool isRunning() const { return m_running; }
    bool isBusy() const { return m_running || m_tickInProgress; }
    RuntimeMode mode() const { return m_mode; }
    VisionContext lastContext() const { return m_lastContext; }
    QVector<ThreadTaskSnapshot> lastTasks() const { return m_lastTasks; }

public slots:
    bool runOnce();
    bool startLoop(int intervalMs = 0);
    void stop();
    bool runSingleNode(const QString &nodeId);
    bool runFromNode(const QString &nodeId);
    bool runScope(const Selt::ExecutionScope &scope);

signals:
    void runStarted(RuntimeMode mode);
    void runFinished(bool ok, const VisionContext &context);
    void runProgress(const QString &nodeId, ModuleStatus status);
    void diagnosticsChanged(const QStringList &lines);
    void tasksChanged(const QVector<ThreadTaskSnapshot> &tasks);
    void busyChanged(bool busy);

private:
    void onLoopTick();
    void publishContext(const VisionContext &ctx);
    QVector<ThreadTaskSnapshot> buildTasks(const VisionContext &ctx) const;
    RuntimeExecuteOptions makeOptions(VisionImage *liveFrameStorage,
                                      const ExecutionScope &scope = ExecutionScope::fullFlow());
    bool executeScope(const ExecutionScope &scope);
    void setBusyState(bool running, bool tickInProgress);
    bool grabLiveFrame(VisionImage *out);

    Document *m_document{nullptr};
    const ProjectVariableStore *m_variables{nullptr};
    const ProjectVariableStore *m_flowVariables{nullptr};
    QString m_flowId;
    const ProjectResourceStore *m_resources{nullptr};
    InputSourceService *m_inputSource{nullptr};
    FlowSchedulerPtr m_scheduler;
    bool m_running{false};
    bool m_tickInProgress{false};
    bool m_stopRequested{false};
    RuntimeMode m_mode{RuntimeMode::Once};
    CancellationToken m_token;
    QTimer m_loopTimer;
    VisionContext m_lastContext;
    QStringList m_diagnostics;
    QVector<ThreadTaskSnapshot> m_lastTasks;
    CalibrationModel m_calibration;
    bool m_hasCalibration{false};
    QString m_batchId;
};

class RuntimeMonitor
{
public:
    void setTasks(const QVector<ThreadTaskSnapshot> &tasks) { m_tasks = tasks; }
    QVector<ThreadTaskSnapshot> tasks() const { return m_tasks; }
    void appendLog(const QString &line) { m_logs.append(line); }
    QStringList logs() const { return m_logs; }

private:
    QVector<ThreadTaskSnapshot> m_tasks;
    QStringList m_logs;
};

} // namespace Selt

#endif
