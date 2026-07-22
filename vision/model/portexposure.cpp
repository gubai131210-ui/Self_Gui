#include "vision/model/portexposure.h"

#include "vision/data/datatype.h"

#include <QtMath>

namespace Selt {
namespace {

bool portIsPrimaryImage(const ModulePortDef &port)
{
    if (port.dataType != DataTypeId::Image)
        return false;
    const QString id = port.id.toLower();
    return id == QLatin1String("in") || id == QLatin1String("out")
        || id == QLatin1String("image");
}

bool connectionTouchesPort(const QVector<ConnectionModel> &connections,
                           const QString &nodeId,
                           const QString &portId)
{
    for (const ConnectionModel &conn : connections) {
        if (conn.sourceNodeId == nodeId && conn.sourcePortId == portId)
            return true;
        if (conn.targetNodeId == nodeId && conn.targetPortId == portId)
            return true;
        if ((portId == QLatin1String("in") || portId == QLatin1String("image"))
            && conn.targetNodeId == nodeId
            && (conn.targetPortId == QLatin1String("in")
                || conn.targetPortId == QLatin1String("image"))) {
            return true;
        }
        if ((portId == QLatin1String("out") || portId == QLatin1String("image"))
            && conn.sourceNodeId == nodeId
            && (conn.sourcePortId == QLatin1String("out")
                || conn.sourcePortId == QLatin1String("image"))) {
            return true;
        }
    }
    return false;
}

qreal portSlotY(int index, int count)
{
    if (count <= 1)
        return 0.5;
    return static_cast<qreal>(index + 1) / static_cast<qreal>(count + 1);
}

} // namespace

QString portRoleToString(PortRole role)
{
    switch (role) {
    case PortRole::Result:
        return QStringLiteral("result");
    case PortRole::Debug:
        return QStringLiteral("debug");
    case PortRole::Primary:
    default:
        return QStringLiteral("primary");
    }
}

PortRole portRoleFromString(const QString &text, bool *ok)
{
    const QString t = text.trimmed().toLower();
    if (ok)
        *ok = true;
    if (t == QLatin1String("result"))
        return PortRole::Result;
    if (t == QLatin1String("debug"))
        return PortRole::Debug;
    if (t == QLatin1String("primary") || t.isEmpty())
        return PortRole::Primary;
    if (ok)
        *ok = false;
    return PortRole::Primary;
}

QString portRoleDisplayName(PortRole role)
{
    switch (role) {
    case PortRole::Result:
        return QStringLiteral("结果");
    case PortRole::Debug:
        return QStringLiteral("调试");
    case PortRole::Primary:
    default:
        return QStringLiteral("主链路");
    }
}

QString defaultPortGroup(DataTypeId type, bool isInput)
{
    Q_UNUSED(isInput);
    switch (type) {
    case DataTypeId::Image:
        return QStringLiteral("Image");
    case DataTypeId::Roi:
    case DataTypeId::Region:
    case DataTypeId::Rect:
        return QStringLiteral("ROI");
    case DataTypeId::Point2D:
    case DataTypeId::Line:
    case DataTypeId::Circle:
    case DataTypeId::Contour:
        return QStringLiteral("Geometry");
    case DataTypeId::Measurement:
    case DataTypeId::Real:
    case DataTypeId::Int:
    case DataTypeId::Bool:
        return QStringLiteral("Measurement");
    case DataTypeId::String:
    case DataTypeId::Table:
    case DataTypeId::ByteArray:
        return QStringLiteral("Data");
    case DataTypeId::Overlay:
        return QStringLiteral("Debug");
    default:
        return QStringLiteral("Other");
    }
}

void applyBuiltinPortExposureHints(ModuleDescriptor &descriptor)
{
    descriptor.hasExposureHints = true;
    for (ModulePortDef &port : descriptor.ports) {
        if (port.group.isEmpty())
            port.group = defaultPortGroup(port.dataType, port.isInput);

        if (portIsPrimaryImage(port) || (port.isInput && port.required)) {
            port.role = PortRole::Primary;
            port.defaultExposed = true;
            continue;
        }
        if (port.dataType == DataTypeId::Overlay
            || port.id.compare(QStringLiteral("overlay"), Qt::CaseInsensitive) == 0) {
            port.role = PortRole::Debug;
            port.defaultExposed = false;
            continue;
        }
        if (port.dataType == DataTypeId::Roi || port.dataType == DataTypeId::Region) {
            port.role = PortRole::Primary;
            port.defaultExposed = false;
            continue;
        }
        if (port.isInput) {
            port.role = PortRole::Primary;
            port.defaultExposed = false;
            continue;
        }
        port.role = PortRole::Result;
        port.defaultExposed = false;
    }
}

QStringList defaultExposedPortIds(const ModuleDescriptor &descriptor)
{
    QStringList ids;
    if (!descriptor.hasExposureHints) {
        for (const ModulePortDef &port : descriptor.ports)
            ids.append(port.id);
        return ids;
    }
    for (const ModulePortDef &port : descriptor.ports) {
        if (port.defaultExposed)
            ids.append(port.id);
    }
    if (ids.isEmpty()) {
        for (const ModulePortDef &port : descriptor.ports) {
            if (portIsPrimaryImage(port) || (port.isInput && port.required))
                ids.append(port.id);
        }
    }
    return ids;
}

QStringList resolveExposedPortIds(const NodeModel &node,
                                  const ModuleDescriptor &descriptor,
                                  const QVector<ConnectionModel> &connections)
{
    QStringList ids = node.portExposureCustomized
        ? node.exposedPortIds
        : defaultExposedPortIds(descriptor);

    for (const ModulePortDef &port : descriptor.ports) {
        if (connectionTouchesPort(connections, node.id, port.id) && !ids.contains(port.id))
            ids.append(port.id);
    }
    QStringList filtered;
    for (const QString &id : ids) {
        if (descriptor.findPort(id) || node.findPort(id))
            filtered.append(id);
    }
    return filtered;
}

bool isPortExposedOnCanvas(const NodeModel &node,
                           const ModuleDescriptor &descriptor,
                           const QString &portId,
                           const QVector<ConnectionModel> &connections)
{
    return resolveExposedPortIds(node, descriptor, connections).contains(portId);
}

void syncNodePortExposure(NodeModel &node,
                          const ModuleDescriptor &descriptor,
                          const QVector<ConnectionModel> &connections)
{
    if (!node.portExposureCustomized)
        node.exposedPortIds = defaultExposedPortIds(descriptor);
    else
        node.exposedPortIds = resolveExposedPortIds(node, descriptor, connections);
}

void layoutExposedPorts(const ModuleDescriptor &descriptor,
                        NodeModel &node,
                        const QVector<ConnectionModel> &connections)
{
    const QStringList visible = resolveExposedPortIds(node, descriptor, connections);
    int inputCount = 0;
    int outputCount = 0;
    for (const QString &id : visible) {
        const ModulePortDef *def = descriptor.findPort(id);
        if (!def)
            continue;
        if (def->isInput)
            ++inputCount;
        else
            ++outputCount;
    }

    int inputIndex = 0;
    int outputIndex = 0;
    for (PortModel &port : node.ports) {
        const ModulePortDef *def = descriptor.findPort(port.id);
        if (!def)
            continue;
        if (!visible.contains(port.id)) {
            // Park hidden ports outside the node so hit-tests ignore them less often;
            // graphics layer simply skips creating items for non-exposed ports.
            port.relativeX = def->isInput ? -0.2 : 1.2;
            port.relativeY = 0.5;
            continue;
        }
        if (def->isInput) {
            port.relativeX = 0.0;
            port.relativeY = portSlotY(inputIndex++, inputCount);
        } else {
            port.relativeX = 1.0;
            port.relativeY = portSlotY(outputIndex++, outputCount);
        }
    }
}

} // namespace Selt
