#include "visioncontext.h"

void VisionContext::appendLog(const QString &line)
{
    log.append(line);
}

void VisionContext::setError(const QString &message, const QString &nodeId)
{
    errorMessage = message;
    failedNodeId = nodeId;
    appendLog(QStringLiteral("[错误] %1").arg(message));
}

ModuleRunResult VisionContext::moduleResult(const QString &nodeId) const
{
    return moduleResults.value(nodeId);
}

VisionImage VisionContext::imageForNode(const QString &nodeId) const
{
    return moduleResults.value(nodeId).outputImage;
}

OverlayList VisionContext::overlaysForNode(const QString &nodeId) const
{
    return moduleResults.value(nodeId).overlays;
}
