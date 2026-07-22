#ifndef FLOWSCHEDULER_H
#define FLOWSCHEDULER_H

#include "core/model/document.h"
#include "vision/domain/projectvariables.h"
#include "vision/model/visioncontext.h"
#include "vision/runtime/flowruntime.h"
#include "vision/runtime/inodeexecutor.h"

#include <memory>

namespace Selt {

/// Pluggable scheduler interface; SequentialScheduler wraps FlowRuntime::executeOnce.
class IFlowScheduler
{
public:
    virtual ~IFlowScheduler() = default;
    virtual QString name() const = 0;
    virtual bool execute(const Document &document, VisionContext &context,
                         const ProjectVariableStore &variables,
                         const RuntimeExecuteOptions &options) = 0;
};

class SequentialScheduler : public IFlowScheduler
{
public:
    QString name() const override { return QStringLiteral("sequential"); }
    bool execute(const Document &document, VisionContext &context,
                 const ProjectVariableStore &variables,
                 const RuntimeExecuteOptions &options) override;
};

using FlowSchedulerPtr = std::shared_ptr<IFlowScheduler>;

} // namespace Selt

#endif // FLOWSCHEDULER_H
