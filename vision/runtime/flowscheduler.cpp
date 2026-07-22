#include "vision/runtime/flowscheduler.h"

namespace Selt {

bool SequentialScheduler::execute(const Document &document, VisionContext &context,
                                  const ProjectVariableStore &variables,
                                  const RuntimeExecuteOptions &options)
{
    return FlowRuntime::executeOnce(document, context, variables, options);
}

} // namespace Selt
