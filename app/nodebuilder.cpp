#include "nodebuilder.h"

#include "core/registry/nodefactory.h"

#if SELT_HAS_OPENCV
#include "vision/registry/visionnodeids.h"
#include "vision/registry/visionnoderegistry.h"
#endif

QString NodeBuilder::displayName(const QString &type)
{
#if SELT_HAS_OPENCV
    if (VisionNodeIds::isVisionType(type))
        return VisionNodeRegistry::displayName(type);
#endif
    return NodeFactory::displayName(type);
}

NodeModel NodeBuilder::create(const QString &type, const QPointF &position)
{
#if SELT_HAS_OPENCV
    if (VisionNodeIds::isVisionType(type))
        return VisionNodeRegistry::create(type, position);
#endif
    return NodeFactory::create(type, position);
}
