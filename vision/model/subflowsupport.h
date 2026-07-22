#pragma once

#include "core/model/connectionmodel.h"
#include "core/model/nodemodel.h"
#include "vision/domain/projectvariables.h"
#include "vision/model/moduledescriptor.h"
#include "vision/model/moduleparamdef.h"
#include "vision/model/portexposure.h"

namespace Selt {

inline SubflowInterface subflowInterfaceFromParameters(const QJsonObject &parameters)
{
    QString error;
    SubflowInterface iface = SubflowInterface::fromJson(
        parameters.value(QStringLiteral("subflowInterface")).toObject(), &error);
    if (iface.flowId.isEmpty())
        iface.flowId = parameters.value(QStringLiteral("flowId")).toString();
    if (iface.displayName.isEmpty())
        iface.displayName = parameters.value(QStringLiteral("displayName")).toString();
    return iface;
}

inline QVector<ModulePortDef> modulePortsFromSubflowInterface(const SubflowInterface &iface)
{
    QVector<ModulePortDef> ports;
    for (const TypedPortDef &def : iface.inputs) {
        ModulePortDef port;
        port.id = def.id;
        port.name = def.name.isEmpty() ? def.id : def.name;
        port.isInput = true;
        port.dataType = def.dataType;
        port.required = def.required;
        port.description = def.description;
        port.group = defaultPortGroup(def.dataType, true);
        port.role = PortRole::Primary;
        port.defaultExposed = true;
        ports.append(port);
    }
    for (const TypedPortDef &def : iface.outputs) {
        ModulePortDef port;
        port.id = def.id;
        port.name = def.name.isEmpty() ? def.id : def.name;
        port.isInput = false;
        port.dataType = def.dataType;
        port.required = false;
        port.description = def.description;
        port.group = defaultPortGroup(def.dataType, false);
        port.role = (def.dataType == DataTypeId::Overlay) ? PortRole::Debug : PortRole::Result;
        port.defaultExposed = def.dataType != DataTypeId::Overlay;
        ports.append(port);
    }
    return ports;
}

/// Rebuild node.ports from SubflowInterface terminal contract (external terminals only).
inline bool applySubflowInterfaceToNode(NodeModel &node,
                                        const SubflowInterface &iface,
                                        const QVector<ConnectionModel> &connections = {})
{
    QString error;
    // Empty flowId is allowed for newly created shell nodes; full validation when configured.
    if (!iface.flowId.isEmpty() && !iface.validateScope(&error))
        return false;

    ModuleDescriptor synthetic;
    synthetic.typeId = node.type;
    synthetic.hasExposureHints = true;
    synthetic.ports = modulePortsFromSubflowInterface(iface);
    if (synthetic.ports.isEmpty()) {
        ModulePortDef in;
        in.id = QStringLiteral("in");
        in.name = QStringLiteral("输入");
        in.isInput = true;
        in.dataType = DataTypeId::Image;
        in.required = true;
        in.role = PortRole::Primary;
        in.defaultExposed = true;
        ModulePortDef out;
        out.id = QStringLiteral("out");
        out.name = QStringLiteral("输出");
        out.isInput = false;
        out.dataType = DataTypeId::Image;
        out.role = PortRole::Primary;
        out.defaultExposed = true;
        synthetic.ports = {in, out};
    }

    QList<PortModel> rebuilt;
    for (const ModulePortDef &portDef : synthetic.ports) {
        PortModel port;
        port.id = portDef.id;
        port.name = portDef.name;
        port.direction = portDef.isInput ? PortDirection::Input : PortDirection::Output;
        port.dataType = dataTypeIdToString(portDef.dataType);
        rebuilt.append(port);
    }
    // Preserve unknown connected ports (plugin/subflow migration safety).
    for (const PortModel &old : node.ports) {
        bool known = false;
        for (const PortModel &p : rebuilt) {
            if (p.id == old.id) {
                known = true;
                break;
            }
        }
        if (known)
            continue;
        bool wired = false;
        for (const ConnectionModel &conn : connections) {
            if ((conn.sourceNodeId == node.id && conn.sourcePortId == old.id)
                || (conn.targetNodeId == node.id && conn.targetPortId == old.id)) {
                wired = true;
                break;
            }
        }
        if (wired)
            rebuilt.append(old);
    }
    node.ports = rebuilt;
    node.parameters.insert(QStringLiteral("subflowInterface"), iface.toJson());
    if (!iface.flowId.isEmpty())
        node.parameters.insert(QStringLiteral("flowId"), iface.flowId);
    if (!iface.displayName.isEmpty()) {
        node.parameters.insert(QStringLiteral("displayName"), iface.displayName);
        if (node.text.isEmpty())
            node.text = iface.displayName;
    }
    syncNodePortExposure(node, synthetic, connections);
    layoutExposedPorts(synthetic, node, connections);
    return true;
}

/// Inject optional ROI port + normalize exposure for external/plugin descriptors.
inline void prepareExternalDescriptor(ModuleDescriptor &descriptor)
{
    bool hasRoiParam = false;
    for (const ModuleParamDef &param : descriptor.parameters) {
        if (param.key == QLatin1String("roi") || param.type == ModuleParamType::RoiRect) {
            hasRoiParam = true;
            break;
        }
    }
    if (hasRoiParam && !descriptor.findInputPort(QStringLiteral("roi"))) {
        ModulePortDef roiPort;
        roiPort.id = QStringLiteral("roi");
        roiPort.name = QStringLiteral("ROI");
        roiPort.isInput = true;
        roiPort.dataType = DataTypeId::Roi;
        roiPort.required = false;
        roiPort.description = QStringLiteral("可选上游 ROI；未连接时使用参数面板 ROI");
        roiPort.group = QStringLiteral("ROI");
        roiPort.role = PortRole::Primary;
        roiPort.defaultExposed = false;
        descriptor.ports.append(roiPort);
    }

    if (descriptor.capabilityVersion >= 1 || descriptor.hasExposureHints) {
        // New plugins / hinted descriptors: fill missing group/role and keep declared exposure.
        for (ModulePortDef &port : descriptor.ports) {
            if (port.group.isEmpty())
                port.group = defaultPortGroup(port.dataType, port.isInput);
        }
        descriptor.hasExposureHints = true;
    }
    // Legacy plugins (capabilityVersion 0, no hints): leave hasExposureHints=false → full expose.
}

} // namespace Selt
