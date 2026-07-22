#ifndef INODEEXECUTOR_H
#define INODEEXECUTOR_H

#include "foundation/selterror.h"
#include "vision/data/datatype.h"
#include "vision/domain/variablescope.h"
#include "vision/model/calibrationmodel.h"
#include "vision/model/modulestatus.h"

#include <QHash>
#include <QJsonObject>
#include <QString>
#include <memory>

namespace Selt {

class ProjectResourceStore;
class VariableScopeSnapshot;

struct ExecutionRequest
{
    QString nodeId;
    QString typeId;
    QJsonObject parameters; // legacy JSON view for built-in executors
    QHash<QString, DataValue> typedParameters;
    QHash<QString, DataValue> inputs;
    QHash<QString, TypedPortDef> inputDefinitions;
};

struct ExecutionResult
{
    ModuleStatus status{ModuleStatus::Idle};
    QHash<QString, DataValue> outputs;
    OverlayList overlays;
    MeasurementResult measurement;
    QString errorMessage;
    QString diagnosticCode;
    /// Industrial outcome details (optional; empty when unused).
    QString failureStage;
    double qualityScore{1.0};
    QJsonObject autoSnapshot;
    qint64 elapsedMs{0};
};

class CancellationToken
{
public:
    void cancel() { m_cancelled = true; }
    bool isCancelled() const { return m_cancelled; }
private:
    bool m_cancelled{false};
};

class ExecutionContext
{
public:
    CancellationToken *cancellation() { return &m_token; }
    const CancellationToken *cancellation() const { return &m_token; }
    void setVariable(const QString &key, const DataValue &value) { m_variables.insert(key, value); }
    DataValue variable(const QString &key) const { return m_variables.value(key); }

    /// Live/replay frame injected by RunController; imageLoader prefers this over file path.
    void setLiveFrame(const VisionImage &frame)
    {
        m_liveFrame = frame;
        m_hasLiveFrame = !frame.isEmpty();
    }
    bool hasLiveFrame() const { return m_hasLiveFrame; }
    VisionImage liveFrame() const { return m_liveFrame; }

    /// Optional project resource store for template / asset resolution.
    void setResourceStore(const ProjectResourceStore *store) { m_resources = store; }
    const ProjectResourceStore *resourceStore() const { return m_resources; }

    /// Active calibration snapshot for this execution (immutable for the run).
    void setCalibration(const CalibrationModel &calibration)
    {
        m_calibration = calibration;
        m_hasCalibration = calibration.valid;
    }
    bool hasCalibration() const { return m_hasCalibration; }
    CalibrationModel calibration() const { return m_calibration; }

    /// Owned by the current run (FlowRuntime / RunController). May be null for legacy callers.
    void setVariableScopeSnapshot(VariableScopeSnapshot *snapshot) { m_scopeSnapshot = snapshot; }
    VariableScopeSnapshot *variableScopeSnapshot() { return m_scopeSnapshot; }
    const VariableScopeSnapshot *variableScopeSnapshot() const { return m_scopeSnapshot; }

private:
    CancellationToken m_token;
    QHash<QString, DataValue> m_variables;
    VisionImage m_liveFrame;
    bool m_hasLiveFrame{false};
    const ProjectResourceStore *m_resources{nullptr};
    CalibrationModel m_calibration;
    bool m_hasCalibration{false};
    VariableScopeSnapshot *m_scopeSnapshot{nullptr};
};

class INodeExecutor
{
public:
    virtual ~INodeExecutor() = default;
    virtual QString typeId() const = 0;
    virtual ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &context) = 0;
};

using NodeExecutorPtr = std::shared_ptr<INodeExecutor>;

} // namespace Selt

#endif // INODEEXECUTOR_H
