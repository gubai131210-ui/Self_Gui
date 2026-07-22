#include "vision/validation/graphvalidator.h"

#include "vision/data/datatype.h"
#include "vision/model/subflowsupport.h"
#include "vision/registry/communicationnodeids.h"
#include "vision/registry/visionnodeids.h"
#include "vision/registry/visionnoderegistry.h"

#include <QHash>
#include <QSet>

namespace Selt {
namespace {

const PortModel *findNodePort(const NodeModel &node, const QString &portId)
{
    for (const PortModel &port : node.ports) {
        if (port.id == portId)
            return &port;
    }
    return nullptr;
}

DataTypeId resolvePortDataType(const NodeModel &node, const QString &portId, bool *isInput)
{
    const ModuleDescriptor desc = VisionNodeRegistry::descriptor(node.type);
    if (const ModulePortDef *def = desc.findPort(portId)) {
        if (isInput)
            *isInput = def->isInput;
        return def->dataType;
    }
    if (const PortModel *port = findNodePort(node, portId)) {
        if (isInput)
            *isInput = port->direction == PortDirection::Input
                || port->direction == PortDirection::Both;
        if (!port->dataType.isEmpty()) {
            bool ok = false;
            const DataTypeId typed = dataTypeIdFromString(port->dataType, &ok);
            if (ok)
                return typed;
        }
        // Legacy vision nodes: in/out default to Image.
        if (VisionNodeIds::isVisionType(node.type))
            return DataTypeId::Image;
        return DataTypeId::None;
    }
    if (isInput)
        *isInput = true;
    return DataTypeId::None;
}

GraphDiagnostic makeDiag(GraphDiagnosticSeverity sev, const QString &code, const QString &message,
                         const QString &nodeId = {}, const QString &portId = {},
                         const QString &parameterKey = {}, const QString &connectionId = {})
{
    GraphDiagnostic d;
    d.severity = sev;
    d.code = code;
    d.message = message;
    d.nodeId = nodeId;
    d.portId = portId;
    d.parameterKey = parameterKey;
    d.connectionId = connectionId;
    return d;
}

QVector<GraphDiagnostic> validateGraphCommon(const QVector<NodeModel> &nodes,
                                             const QVector<ConnectionModel> &connections,
                                             bool fullTyped)
{
    QVector<GraphDiagnostic> diags;
    QHash<QString, NodeModel> nodeMap;
    QSet<QString> nodeIds;
    QSet<QString> connIds;

    for (const NodeModel &node : nodes) {
        if (node.id.isEmpty()) {
            diags.append(makeDiag(GraphDiagnosticSeverity::Error, QStringLiteral("empty_node_id"),
                                  QStringLiteral("节点 ID 不能为空")));
            continue;
        }
        if (nodeIds.contains(node.id)) {
            diags.append(makeDiag(GraphDiagnosticSeverity::Error, QStringLiteral("duplicate_node_id"),
                                  QStringLiteral("存在重复节点 ID: %1").arg(node.id), node.id));
        }
        nodeIds.insert(node.id);
        nodeMap.insert(node.id, node);
    }

    QHash<QString, int> inputFanIn; // targetNode|targetPort -> count

    for (const ConnectionModel &conn : connections) {
        if (conn.id.isEmpty()) {
            diags.append(makeDiag(GraphDiagnosticSeverity::Error, QStringLiteral("empty_connection_id"),
                                  QStringLiteral("连线 ID 不能为空")));
            continue;
        }
        if (connIds.contains(conn.id)) {
            diags.append(makeDiag(GraphDiagnosticSeverity::Error, QStringLiteral("duplicate_connection_id"),
                                  QStringLiteral("存在重复连线 ID: %1").arg(conn.id),
                                  {}, {}, {}, conn.id));
        }
        connIds.insert(conn.id);

        if (conn.sourceNodeId == conn.targetNodeId) {
            diags.append(makeDiag(GraphDiagnosticSeverity::Error, QStringLiteral("self_loop"),
                                  QStringLiteral("不允许自环连线"), conn.sourceNodeId, conn.sourcePortId,
                                  {}, conn.id));
            continue;
        }
        if (!nodeMap.contains(conn.sourceNodeId) || !nodeMap.contains(conn.targetNodeId)) {
            diags.append(makeDiag(GraphDiagnosticSeverity::Error, QStringLiteral("missing_endpoint_node"),
                                  QStringLiteral("连线引用了不存在的节点"),
                                  conn.sourceNodeId, conn.sourcePortId, {}, conn.id));
            continue;
        }

        const NodeModel &source = nodeMap.value(conn.sourceNodeId);
        const NodeModel &target = nodeMap.value(conn.targetNodeId);
        if (!findNodePort(source, conn.sourcePortId)) {
            diags.append(makeDiag(GraphDiagnosticSeverity::Error, QStringLiteral("missing_source_port"),
                                  QStringLiteral("源端口不存在: %1.%2")
                                      .arg(conn.sourceNodeId, conn.sourcePortId),
                                  conn.sourceNodeId, conn.sourcePortId, {}, conn.id));
            continue;
        }
        if (!findNodePort(target, conn.targetPortId)) {
            diags.append(makeDiag(GraphDiagnosticSeverity::Error, QStringLiteral("missing_target_port"),
                                  QStringLiteral("目标端口不存在: %1.%2")
                                      .arg(conn.targetNodeId, conn.targetPortId),
                                  conn.targetNodeId, conn.targetPortId, {}, conn.id));
            continue;
        }

        bool sourceIsInput = false;
        bool targetIsInput = true;
        const DataTypeId sourceType = resolvePortDataType(source, conn.sourcePortId, &sourceIsInput);
        const DataTypeId targetType = resolvePortDataType(target, conn.targetPortId, &targetIsInput);

        if (sourceIsInput) {
            diags.append(makeDiag(GraphDiagnosticSeverity::Error, QStringLiteral("source_not_output"),
                                  QStringLiteral("源端口必须是输出端口"),
                                  conn.sourceNodeId, conn.sourcePortId, {}, conn.id));
        }
        if (!targetIsInput) {
            diags.append(makeDiag(GraphDiagnosticSeverity::Error, QStringLiteral("target_not_input"),
                                  QStringLiteral("目标端口必须是输入端口"),
                                  conn.targetNodeId, conn.targetPortId, {}, conn.id));
        }

        if (fullTyped && sourceType != DataTypeId::None && targetType != DataTypeId::None
            && !dataTypesCompatible(sourceType, targetType)) {
            diags.append(makeDiag(GraphDiagnosticSeverity::Error, QStringLiteral("type_mismatch"),
                                  QStringLiteral("端口类型不兼容: %1 → %2")
                                      .arg(dataTypeIdDisplayName(sourceType),
                                           dataTypeIdDisplayName(targetType)),
                                  conn.targetNodeId, conn.targetPortId, {}, conn.id));
        }

        const QString fanKey = conn.targetNodeId + QLatin1Char('|') + conn.targetPortId;
        inputFanIn[fanKey] = inputFanIn.value(fanKey) + 1;
    }

    for (auto it = inputFanIn.cbegin(); it != inputFanIn.cend(); ++it) {
        if (it.value() > 1) {
            const QStringList parts = it.key().split(QLatin1Char('|'));
            diags.append(makeDiag(GraphDiagnosticSeverity::Error, QStringLiteral("multi_input"),
                                  QStringLiteral("输入端口存在多重连接"),
                                  parts.value(0), parts.value(1)));
        }
    }

    if (!fullTyped)
        return diags;

    for (const NodeModel &node : nodes) {
        if (!VisionNodeIds::isVisionType(node.type))
            continue;
        const ModuleDescriptor desc = VisionNodeRegistry::descriptor(node.type);
        for (const ModulePortDef &port : desc.ports) {
            if (!port.isInput || !port.required)
                continue;
            const QString fanKey = node.id + QLatin1Char('|') + port.id;
            if (inputFanIn.value(fanKey) > 0)
                continue;
            // Compatible alias: required "image" may be wired via "in".
            bool wired = false;
            for (const ConnectionModel &conn : connections) {
                if (conn.targetNodeId != node.id)
                    continue;
                if (conn.targetPortId == port.id) {
                    wired = true;
                    break;
                }
                if (port.id == QLatin1String("image") && conn.targetPortId == QLatin1String("in")) {
                    wired = true;
                    break;
                }
                if (port.id == QLatin1String("in") && conn.targetPortId == QLatin1String("image")) {
                    wired = true;
                    break;
                }
            }
            if (!wired && node.type != VisionNodeIds::imageLoader()) {
                diags.append(makeDiag(GraphDiagnosticSeverity::Warning, QStringLiteral("missing_required_input"),
                                      QStringLiteral("节点 %1 缺少必需输入端口 %2")
                                          .arg(node.text.isEmpty() ? node.id : node.text, port.name),
                                      node.id, port.id));
            }
        }

        for (const ModuleParamDef &param : desc.parameters) {
            const DataTypeId expected = dataTypeIdFromModuleParamType(static_cast<int>(param.type));
            ParameterBinding binding;
            if (node.parameterBindings.contains(param.key)) {
                QString err;
                binding = ParameterBinding::fromJson(node.parameterBindings.value(param.key).toObject(), &err);
                if (!err.isEmpty() || !binding.isValid()) {
                    diags.append(makeDiag(GraphDiagnosticSeverity::Error, QStringLiteral("bad_binding"),
                                          QStringLiteral("参数 %1 绑定无效: %2")
                                              .arg(param.label, err.isEmpty() ? QStringLiteral("格式错误") : err),
                                          node.id, {}, param.key));
                    continue;
                }
            } else {
                binding = ParameterBinding::legacyConstantBinding(node.parameters.value(param.key), expected);
            }

            // Lightweight structural checks without ProjectVariableStore.
            if (binding.kind == ParameterSourceKind::UpstreamBinding) {
                if (!nodeMap.contains(binding.upstreamNodeId)) {
                    diags.append(makeDiag(GraphDiagnosticSeverity::Error, QStringLiteral("binding_missing_node"),
                                          QStringLiteral("参数 %1 上游节点不存在: %2")
                                              .arg(param.label, binding.upstreamNodeId),
                                          node.id, binding.upstreamPortId, param.key));
                    continue;
                }
                const NodeModel &up = nodeMap.value(binding.upstreamNodeId);
                bool sourceIsInput = false;
                const DataTypeId upType = resolvePortDataType(up, binding.upstreamPortId, &sourceIsInput);
                if (!findNodePort(up, binding.upstreamPortId)
                    && !VisionNodeRegistry::descriptor(up.type).resolvePortAlias(binding.upstreamPortId, false)) {
                    diags.append(makeDiag(GraphDiagnosticSeverity::Error, QStringLiteral("binding_missing_port"),
                                          QStringLiteral("参数 %1 上游端口不存在: %2.%3")
                                              .arg(param.label, binding.upstreamNodeId, binding.upstreamPortId),
                                          node.id, binding.upstreamPortId, param.key));
                    continue;
                }
                if (upType != DataTypeId::None && !dataTypesCompatible(upType, expected)) {
                    diags.append(makeDiag(GraphDiagnosticSeverity::Error, QStringLiteral("binding_type_mismatch"),
                                          QStringLiteral("参数 %1 绑定类型不兼容: %2 → %3")
                                              .arg(param.label,
                                                   dataTypeIdDisplayName(upType),
                                                   dataTypeIdDisplayName(expected)),
                                          node.id, binding.upstreamPortId, param.key));
                }
            }
        }

        if (node.type == VisionNodeIds::subflow()) {
            const SubflowInterface iface = subflowInterfaceFromParameters(node.parameters);
            QString ifaceError;
            if (!iface.flowId.isEmpty() && !iface.validateScope(&ifaceError)) {
                diags.append(makeDiag(GraphDiagnosticSeverity::Error,
                                      QStringLiteral("subflow_interface_invalid"),
                                      ifaceError.isEmpty()
                                          ? QStringLiteral("子流程接口无效")
                                          : ifaceError,
                                      node.id, {}, QStringLiteral("subflowInterface")));
            }
        }
    }

    return diags;
}

} // namespace

QVector<GraphDiagnostic> GraphValidator::validateStructure(const QVector<NodeModel> &nodes,
                                                           const QVector<ConnectionModel> &connections)
{
    return validateGraphCommon(nodes, connections, false);
}

QVector<GraphDiagnostic> GraphValidator::validateFlow(const VisionFlow &flow)
{
    return validateGraphCommon(flow.nodes(), flow.connections(), true);
}

QVector<GraphDiagnostic> GraphValidator::validateDocument(const Document &document)
{
    QVector<NodeModel> nodes;
    for (const NodeModel &n : document.nodes())
        nodes.append(n);
    QVector<ConnectionModel> conns;
    for (const ConnectionModel &c : document.connections())
        conns.append(c);
    return validateGraphCommon(nodes, conns, true);
}

bool GraphValidator::hasErrors(const QVector<GraphDiagnostic> &diags)
{
    for (const GraphDiagnostic &d : diags) {
        if (d.severity == GraphDiagnosticSeverity::Error)
            return true;
    }
    return false;
}

QStringList GraphValidator::toMessages(const QVector<GraphDiagnostic> &diags)
{
    QStringList lines;
    for (const GraphDiagnostic &d : diags) {
        QString prefix;
        switch (d.severity) {
        case GraphDiagnosticSeverity::Warning:
            prefix = QStringLiteral("[警告]");
            break;
        case GraphDiagnosticSeverity::Info:
            prefix = QStringLiteral("[信息]");
            break;
        case GraphDiagnosticSeverity::Error:
        default:
            prefix = QStringLiteral("[错误]");
            break;
        }
        lines.append(QStringLiteral("%1 %2 (%3)").arg(prefix, d.message, d.code));
    }
    return lines;
}

bool GraphValidator::canConnect(const Document &document,
                                const QString &sourceNodeId, const QString &sourcePortId,
                                const QString &targetNodeId, const QString &targetPortId,
                                QString *error,
                                bool allowReplaceOccupied)
{
    if (sourceNodeId == targetNodeId) {
        if (error)
            *error = QStringLiteral("不允许连接到同一节点");
        return false;
    }
    if (!document.hasNode(sourceNodeId) || !document.hasNode(targetNodeId)) {
        if (error)
            *error = QStringLiteral("连线端点节点不存在");
        return false;
    }
    const NodeModel source = document.node(sourceNodeId);
    const NodeModel target = document.node(targetNodeId);
    if (!findNodePort(source, sourcePortId) || !findNodePort(target, targetPortId)) {
        if (error)
            *error = QStringLiteral("连线端点端口不存在");
        return false;
    }

    bool sourceIsInput = false;
    bool targetIsInput = true;
    const DataTypeId sourceType = resolvePortDataType(source, sourcePortId, &sourceIsInput);
    const DataTypeId targetType = resolvePortDataType(target, targetPortId, &targetIsInput);
    if (sourceIsInput || !targetIsInput) {
        if (error)
            *error = QStringLiteral("连线方向无效：需要 输出端口 → 输入端口");
        return false;
    }
    if (sourceType != DataTypeId::None && targetType != DataTypeId::None
        && !dataTypesCompatible(sourceType, targetType)) {
        if (error) {
            *error = QStringLiteral("端口类型不兼容: %1 → %2")
                         .arg(dataTypeIdDisplayName(sourceType), dataTypeIdDisplayName(targetType));
        }
        return false;
    }

    for (const ConnectionModel &conn : document.connections()) {
        if (conn.targetNodeId == targetNodeId && conn.targetPortId == targetPortId) {
            if (allowReplaceOccupied)
                return true;
            if (error)
                *error = QStringLiteral("目标输入端口已有连接");
            return false;
        }
    }
    return true;
}

QVector<GraphDiagnostic> GraphValidator::validateForPublish(const Document &document,
                                                            const QStringList &resourceMissingMessages)
{
    QVector<GraphDiagnostic> diags = validateDocument(document);
    // Publish gate: missing required inputs become hard errors (edit-time stays Warning).
    for (GraphDiagnostic &d : diags) {
        if (d.code == QLatin1String("missing_required_input")
            && d.severity == GraphDiagnosticSeverity::Warning) {
            d.severity = GraphDiagnosticSeverity::Error;
        }
    }
    for (const QString &msg : resourceMissingMessages) {
        GraphDiagnostic d;
        d.severity = GraphDiagnosticSeverity::Error;
        d.code = QStringLiteral("resource_missing");
        d.message = msg;
        diags.append(d);
    }
    bool hasMeasureNode = false;
    for (const NodeModel &node : document.nodes()) {
        if (!node.enabled) {
            GraphDiagnostic d;
            d.severity = GraphDiagnosticSeverity::Warning;
            d.code = QStringLiteral("node_disabled");
            d.nodeId = node.id;
            d.message = QStringLiteral("节点已禁用: %1").arg(node.text.isEmpty() ? node.id : node.text);
            diags.append(d);
        }
        if (node.breakpoint) {
            GraphDiagnostic d;
            d.severity = GraphDiagnosticSeverity::Info;
            d.code = QStringLiteral("breakpoint_set");
            d.nodeId = node.id;
            d.message = QStringLiteral("发布前仍有断点: %1").arg(node.text.isEmpty() ? node.id : node.text);
            diags.append(d);
        }
        if (node.type == CommunicationNodeIds::modbusWrite()) {
            const bool allowWrite = node.parameters.value(QStringLiteral("allowWrite")).toBool(false);
            const bool useMock = node.parameters.value(QStringLiteral("useMock")).toBool(true);
            if (!allowWrite) {
                GraphDiagnostic d;
                d.severity = GraphDiagnosticSeverity::Info;
                d.code = QStringLiteral("modbus_write_protected");
                d.nodeId = node.id;
                d.parameterKey = QStringLiteral("allowWrite");
                d.message = QStringLiteral("Modbus写受保护（allowWrite=false），运行时不会真正写出");
                diags.append(d);
            } else if (!useMock) {
                GraphDiagnostic d;
                d.severity = GraphDiagnosticSeverity::Warning;
                d.code = QStringLiteral("modbus_real_write");
                d.nodeId = node.id;
                d.parameterKey = QStringLiteral("useMock");
                d.message = QStringLiteral(
                    "Modbus写已开启真实写出（allowWrite=true, useMock=false），请确认现场地址与安全联锁");
                diags.append(d);
            } else {
                GraphDiagnostic d;
                d.severity = GraphDiagnosticSeverity::Info;
                d.code = QStringLiteral("modbus_mock_write");
                d.nodeId = node.id;
                d.parameterKey = QStringLiteral("useMock");
                d.message = QStringLiteral("Modbus写使用 Mock 传输（不会访问真实设备）");
                diags.append(d);
            }
        }
#if SELT_HAS_OPENCV
        const QString cat = VisionNodeRegistry::category(node.type);
        if (cat == QLatin1String("测量") || cat == QLatin1String("定位"))
            hasMeasureNode = true;
#else
        Q_UNUSED(hasMeasureNode);
#endif
    }
#if SELT_HAS_OPENCV
    if (hasMeasureNode) {
        GraphDiagnostic d;
        d.severity = GraphDiagnosticSeverity::Info;
        d.code = QStringLiteral("calibration_hint");
        d.message = QStringLiteral("测量/定位结果默认单位为 px；物理单位需要有效标定快照，不会伪造 mm");
        diags.append(d);
    }
#endif
    return diags;
}

} // namespace Selt
