#ifndef FLOWRUNTIME_H
#define FLOWRUNTIME_H

#include "vision/domain/projectvariables.h"
#include "vision/domain/variablescope.h"
#include "vision/model/calibrationmodel.h"
#include "vision/model/modulestatus.h"
#include "vision/model/visioncontext.h"
#include "vision/runtime/inodeexecutor.h"
#include "vision/runtime/executionscope.h"

#include <QDateTime>
#include <functional>

namespace Selt {

class ProjectResourceStore;

enum class RuntimeMode {
    Once,
    Loop // reserved; stop controlled by CancellationToken
};

using RuntimeProgressFn = std::function<void(const QString &nodeId, ModuleStatus status)>;

struct RuntimeExecuteOptions
{
    CancellationToken *token{nullptr};
    RuntimeProgressFn onProgress;
    const VisionImage *liveFrame{nullptr}; // optional live/replay frame for imageLoader
    const ProjectResourceStore *resources{nullptr};
    ExecutionScope scope;
    CalibrationModel calibration;
    bool hasCalibration{false};
    qint64 frameId{0};
    QString deviceId;
    QString sourceDescription;
    QString batchId;
    QDateTime frameGrabbedAt;
    /// 若设置，则参数绑定按全局→工程→流程→分组→算子链解析。
    VariableScopeSnapshot variableScopes;
    bool hasVariableScopes{false};
    /// When false, module image snapshots are dropped after summaries (production).
    bool retainImageSnapshots{true};
};

class FlowRuntime
{
public:
    static QStringList topologicalOrder(const Document &document, QString *error = nullptr);
    static bool executeOnce(const Document &document, VisionContext &context,
                            CancellationToken *token = nullptr);
    static bool executeOnce(const Document &document, VisionContext &context,
                            const ProjectVariableStore &variables,
                            CancellationToken *token = nullptr);
    static bool executeOnce(const Document &document, VisionContext &context,
                            const ProjectVariableStore &variables,
                            const RuntimeExecuteOptions &options);
    static bool executeOnce(const Document &document, VisionContext &context,
                            const ProjectVariableStore &variables,
                            const ExecutionScope &scope,
                            CancellationToken *token = nullptr);
};

} // namespace Selt

#endif // FLOWRUNTIME_H
