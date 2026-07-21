#ifndef VISIONPIPELINE_H
#define VISIONPIPELINE_H

#include "core/model/document.h"
#include "vision/model/visioncontext.h"

class VisionPipeline
{
public:
    static bool execute(const Document &document, VisionContext &context,
                        ExecutionMode mode = ExecutionMode::Once);
    static QStringList buildExecutionOrder(const Document &document);
};

#endif // VISIONPIPELINE_H
