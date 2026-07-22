#include "visionpipeline.h"

#include "vision/nodes/builtinexecutors.h"
#include "vision/runtime/flowruntime.h"

bool VisionPipeline::execute(const Document &document, VisionContext &context, ExecutionMode mode)
{
    // SELT_DEPRECATED(phase-3): legacy entry kept; dispatch via FlowRuntime + executor registry.
    Selt::registerBuiltInOpenCvExecutors();
    Q_UNUSED(mode);
    return Selt::FlowRuntime::executeOnce(document, context, nullptr);
}

QStringList VisionPipeline::buildExecutionOrder(const Document &document)
{
    return Selt::FlowRuntime::topologicalOrder(document, nullptr);
}
