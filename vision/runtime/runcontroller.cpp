#include "vision/runtime/runcontroller.h"

#include "communication/communicationmanager.h"
#include "vision/runtime/diagnosticcodes.h"
#include "vision/runtime/inputsourceservice.h"
#include "vision/runtime/resultsink.h"
#include "vision/runtime/runtimediagnostic.h"

#include <QCoreApplication>
#include <QEventLoop>

namespace Selt {

RunController::RunController(QObject *parent)
    : QObject(parent)
    , m_scheduler(std::make_shared<SequentialScheduler>())
{
    connect(&m_loopTimer, &QTimer::timeout, this, &RunController::onLoopTick);
}

void RunController::setDocument(Document *document)
{
    m_document = document;
}

void RunController::setProjectVariables(const ProjectVariableStore *variables)
{
    m_variables = variables;
}

void RunController::setFlowVariables(const ProjectVariableStore *flowVariables, const QString &flowId)
{
    m_flowVariables = flowVariables;
    m_flowId = flowId;
}

void RunController::setResourceStore(const ProjectResourceStore *resources)
{
    m_resources = resources;
}

void RunController::setScheduler(FlowSchedulerPtr scheduler)
{
    m_scheduler = scheduler ? std::move(scheduler) : std::make_shared<SequentialScheduler>();
}

void RunController::setInputSource(InputSourceService *source)
{
    m_inputSource = source;
}

void RunController::setCalibration(const CalibrationModel &calibration)
{
    m_calibration = calibration;
    m_hasCalibration = calibration.valid;
}

void RunController::clearCalibration()
{
    m_calibration = {};
    m_hasCalibration = false;
}

void RunController::setBusyState(bool running, bool tickInProgress)
{
    const bool wasBusy = isBusy();
    m_running = running;
    m_tickInProgress = tickInProgress;
    if (wasBusy != isBusy())
        emit busyChanged(isBusy());
}

bool RunController::grabLiveFrame(VisionImage *out)
{
    if (!out || !m_inputSource || !m_inputSource->isOpen())
        return false;
    const Error err = m_inputSource->grab(*out);
    if (!err.ok() || out->isEmpty()) {
        m_diagnostics.append(QStringLiteral("[%1/%2] %3")
                                 .arg(runtimeFailureKindDisplayName(RuntimeFailureKind::Device),
                                      DiagnosticCodes::deviceGrabFailed(),
                                      err.ok() ? QStringLiteral("采集帧为空") : err.message));
        emit diagnosticsChanged(m_diagnostics);
        return false;
    }
    return true;
}

RuntimeExecuteOptions RunController::makeOptions(VisionImage *liveFrameStorage,
                                                 const ExecutionScope &scope)
{
    RuntimeExecuteOptions options;
    options.token = &m_token;
    options.onProgress = [this](const QString &nodeId, ModuleStatus status) {
        emit runProgress(nodeId, status);
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    };
    if (liveFrameStorage && !liveFrameStorage->isEmpty())
        options.liveFrame = liveFrameStorage;
    options.resources = m_resources;
    options.scope = scope;
    if (m_hasCalibration) {
        options.calibration = m_calibration;
        options.hasCalibration = true;
    }
    if (m_inputSource && m_inputSource->isOpen()) {
        options.frameId = m_inputSource->lastFrameId();
        options.deviceId = m_inputSource->deviceId();
        options.sourceDescription = m_inputSource->sourceDescription();
        options.frameGrabbedAt = m_inputSource->lastGrabbedAt();
    }
    options.batchId = m_batchId;
    options.hasVariableScopes = true;
    options.variableScopes = VariableScopeSnapshot::fromStores(m_variables, m_flowVariables, m_flowId);
    return options;
}

QVector<ThreadTaskSnapshot> RunController::buildTasks(const VisionContext &ctx) const
{
    QVector<ThreadTaskSnapshot> tasks;
    for (const QString &nodeId : ctx.executionOrder) {
        const ModuleRunResult result = ctx.moduleResult(nodeId);
        ThreadTaskSnapshot snap;
        snap.nodeId = nodeId;
        snap.name = result.displayName.isEmpty() ? nodeId : result.displayName;
        snap.state = moduleStatusToString(result.status);
        snap.elapsedMs = result.elapsedMs;
        snap.failureKind = result.failureKind;
        snap.diagnosticCode = result.diagnosticCode;
        tasks.append(snap);
    }
    return tasks;
}

void RunController::publishContext(const VisionContext &ctx)
{
    m_lastContext = ctx;
    m_diagnostics = ctx.log;
    if (ctx.measurement.valid) {
        ResultRecord record;
        record.batchId = ctx.batchId;
        record.frameId = QString::number(ctx.frameId);
        record.nodeId = ctx.measurement.sourceNodeId;
        record.deviceId = ctx.deviceId;
        record.calibrationId = ctx.calibrationId;
        record.measurement = ctx.measurement;
        record.decisionState = ctx.measurement.decisionState;
        const QStringList sinkErrors = ResultSinkRegistry::instance().publishAll(record);
        for (const QString &err : sinkErrors)
            m_diagnostics.append(QStringLiteral("[结果输出] %1").arg(err));
    }
    for (const RuntimeDiagnostic &d : ctx.runtimeDiagnostics) {
        m_diagnostics.append(QStringLiteral("[%1/%2] %3")
                                 .arg(runtimeFailureKindDisplayName(d.kind), d.code, d.message));
        if (d.code.startsWith(QLatin1String("comm_"))) {
            m_diagnostics.append(QStringLiteral("  建议: 检查通讯 Dock 能力摘要、DLL 与 connectionId 复用配置"));
            for (const QString &hint : Communication::runtimeDeploymentHints())
                m_diagnostics.append(QStringLiteral("  %1").arg(hint));
        }
    }
    for (const GraphDiagnostic &d : ctx.diagnostics) {
        if (d.severity == GraphDiagnosticSeverity::Error || d.severity == GraphDiagnosticSeverity::Warning)
            m_diagnostics.append(QStringLiteral("[%1] %2").arg(d.code, d.message));
    }
    m_lastTasks = buildTasks(ctx);
    emit diagnosticsChanged(m_diagnostics);
    emit tasksChanged(m_lastTasks);
}

bool RunController::runOnce()
{
    return executeScope(ExecutionScope::fullFlow());
}

bool RunController::executeScope(const ExecutionScope &scope)
{
    if (!m_document) {
        m_diagnostics.append(QStringLiteral("[validation] 未绑定文档，无法运行"));
        emit diagnosticsChanged(m_diagnostics);
        return false;
    }
    if (!m_scheduler) {
        m_diagnostics.append(QStringLiteral("[validation] 调度器不可用"));
        emit diagnosticsChanged(m_diagnostics);
        return false;
    }
    if (isBusy())
        return false;
    m_stopRequested = false;
    setBusyState(true, true); // treat once-run as an in-flight tick so stop() cannot re-enter
    m_mode = RuntimeMode::Once;
    m_token = CancellationToken{};
    Communication::CommunicationManager::instance().clearCancel();
    emit runStarted(m_mode);

    VisionImage liveFrame;
    const bool hasLive = grabLiveFrame(&liveFrame);
    VisionContext ctx;
    ProjectVariableStore empty;
    const ProjectVariableStore &vars = m_variables ? *m_variables : empty;
    const bool ok = m_scheduler->execute(*m_document, ctx, vars,
                                         makeOptions(hasLive ? &liveFrame : nullptr, scope));
    publishContext(ctx);
    setBusyState(false, false);
    emit runFinished(ok && !m_stopRequested, ctx);
    return ok && !m_stopRequested;
}

bool RunController::startLoop(int intervalMs)
{
    if (!m_document) {
        m_diagnostics.append(QStringLiteral("[validation] 未绑定文档，无法连续运行"));
        emit diagnosticsChanged(m_diagnostics);
        return false;
    }
    if (!m_scheduler) {
        m_diagnostics.append(QStringLiteral("[validation] 调度器不可用"));
        emit diagnosticsChanged(m_diagnostics);
        return false;
    }
    if (isBusy())
        return false;
    m_stopRequested = false;
    m_mode = RuntimeMode::Loop;
    m_token = CancellationToken{};
    Communication::CommunicationManager::instance().clearCancel();
    setBusyState(true, false);
    emit runStarted(m_mode);
    m_loopTimer.start(qMax(1, intervalMs)); // avoid zero-interval busy loop
    onLoopTick();
    return true;
}

void RunController::stop()
{
    m_stopRequested = true;
    m_token.cancel();
    Communication::CommunicationManager::instance().cancelAll();
    m_loopTimer.stop();
    m_diagnostics.append(QStringLiteral("运行已停止"));
    emit diagnosticsChanged(m_diagnostics);
    // If a tick is still executing, defer busy-clear until it finishes to avoid re-entry.
    if (!m_tickInProgress) {
        VisionContext stopped = m_lastContext;
        setBusyState(false, false);
        emit runFinished(false, stopped);
    }
}

bool RunController::runSingleNode(const QString &nodeId)
{
    if (nodeId.isEmpty())
        return runOnce();
    return executeScope(ExecutionScope::singleNode(nodeId));
}

bool RunController::runFromNode(const QString &nodeId)
{
    if (nodeId.isEmpty())
        return runOnce();
    return executeScope(ExecutionScope::withUpstream(nodeId));
}

bool RunController::runScope(const ExecutionScope &scope)
{
    return executeScope(scope);
}

void RunController::onLoopTick()
{
    if (!m_document || !m_scheduler || m_token.isCancelled() || m_stopRequested) {
        m_loopTimer.stop();
        if (m_tickInProgress)
            return;
        VisionContext stopped = m_lastContext;
        setBusyState(false, false);
        emit runFinished(false, stopped);
        return;
    }
    if (m_tickInProgress)
        return; // prevent re-entrant ticks while a run is still active

    setBusyState(true, true);
    VisionImage liveFrame;
    const bool hasLive = grabLiveFrame(&liveFrame);
    VisionContext ctx;
    ProjectVariableStore empty;
    const ProjectVariableStore &vars = m_variables ? *m_variables : empty;
    const bool ok = m_scheduler->execute(*m_document, ctx, vars,
                                         makeOptions(hasLive ? &liveFrame : nullptr));
    publishContext(ctx);

    if (!ok || m_token.isCancelled() || m_stopRequested) {
        m_loopTimer.stop();
        setBusyState(false, false);
        emit runFinished(ok && !m_stopRequested, ctx);
        return;
    }
    setBusyState(true, false);
}

} // namespace Selt
