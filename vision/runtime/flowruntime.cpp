#include "vision/runtime/flowruntime.h"

#include "vision/model/portexposure.h"
#include "vision/model/regiondata.h"
#include "vision/model/roi.h"
#include "vision/registry/visionnodeids.h"
#include "vision/registry/visionnoderegistry.h"
#include "vision/runtime/bindingresolver.h"
#include "vision/runtime/flowexecutionsnapshot.h"
#include "vision/runtime/nodeexecutorregistry.h"
#include "vision/runtime/portvaluestore.h"
#include "vision/runtime/runtimediagnostic.h"
#include "vision/runtime/diagnosticcodes.h"
#include "vision/validation/graphvalidator.h"

#include <QElapsedTimer>
#include <QHash>
#include <QSet>

#include <exception>
#include <opencv2/core.hpp>

namespace Selt {
namespace {

DataValue extractMeasurementField(const MeasurementResult &m, const QString &portId)
{
    if (portId == QLatin1String("width") || portId == QLatin1String("measurement.width"))
        return DataValue(m.width);
    if (portId == QLatin1String("height") || portId == QLatin1String("measurement.height"))
        return DataValue(m.height);
    if (portId == QLatin1String("area") || portId == QLatin1String("measurement.area"))
        return DataValue(m.area);
    if (portId == QLatin1String("angle") || portId == QLatin1String("measurement.angle"))
        return DataValue(m.angle);
    return DataValue(m);
}

void storeExecutionOutputs(const NodeModel &node, const ModuleDescriptor &desc,
                           const ExecutionResult &result, PortValueStore *store)
{
    for (auto it = result.outputs.cbegin(); it != result.outputs.cend(); ++it) {
        store->set(node.id, it.key(), it.value());
        if (it.key() == QLatin1String("image")) {
            store->set(node.id, QStringLiteral("out"), it.value());
        }
    }
    if (result.measurement.valid) {
        store->set(node.id, QStringLiteral("measurement"), DataValue(result.measurement));
        store->set(node.id, QStringLiteral("width"), DataValue(result.measurement.width));
        store->set(node.id, QStringLiteral("height"), DataValue(result.measurement.height));
        store->set(node.id, QStringLiteral("area"), DataValue(result.measurement.area));
    }
    if (!result.overlays.isEmpty())
        store->set(node.id, QStringLiteral("overlay"), DataValue(result.overlays));

    for (const ModulePortDef &port : desc.ports) {
        if (port.isInput)
            continue;
        store->declareType(node.id, port.id, port.dataType);
        if (port.dataType == DataTypeId::Image && result.outputs.contains(QStringLiteral("image")))
            store->set(node.id, port.id, result.outputs.value(QStringLiteral("image")));
        else if (port.dataType == DataTypeId::Measurement && result.measurement.valid)
            store->set(node.id, port.id, DataValue(result.measurement));
        else if (port.dataType == DataTypeId::Overlay && !result.overlays.isEmpty())
            store->set(node.id, port.id, DataValue(result.overlays));
        else if (port.dataType == DataTypeId::Real && result.measurement.valid)
            store->set(node.id, port.id, extractMeasurementField(result.measurement, port.id));
    }
}

bool collectNodeInputs(const Document &document, const NodeModel &node, const ModuleDescriptor &desc,
                       const PortValueStore &ports, ExecutionRequest *req, QString *error)
{
    auto inputPortAlreadyWired = [&](const QString &portId) {
        for (const ConnectionModel &existing : document.connections()) {
            if (existing.targetNodeId != node.id)
                continue;
            if (existing.targetPortId == portId)
                return true;
            if (portId == QLatin1String("in")
                && (existing.targetPortId == QLatin1String("image")
                    || existing.targetPortId == QLatin1String("in"))) {
                return true;
            }
        }
        return false;
    };

    // Map wired connections onto semantic input keys expected by executors.
    for (const ConnectionModel &conn : document.connections()) {
        if (conn.targetNodeId != node.id)
            continue;
        if (!ports.containsAliased(conn.sourceNodeId, conn.sourcePortId)
            && !ports.containsAliased(conn.sourceNodeId, QStringLiteral("image"))) {
            if (error) {
                *error = QStringLiteral("上游输出不可用: %1.%2")
                             .arg(conn.sourceNodeId, conn.sourcePortId);
            }
            return false;
        }
        DataValue value = ports.valueAliased(conn.sourceNodeId, conn.sourcePortId);
        if (value.isNull())
            value = ports.valueAliased(conn.sourceNodeId, QStringLiteral("image"));

        QString semanticKey = conn.targetPortId;
        if (semanticKey == QLatin1String("in"))
            semanticKey = QStringLiteral("image");

        // Legacy graphs stacked input ports at the same position; image flow often
        // landed on the optional "template" port instead of required "in".
        if (node.type == VisionNodeIds::templateMatch()
            && conn.targetPortId == QLatin1String("template")) {
            const NodeModel sourceNode = document.node(conn.sourceNodeId);
            if (sourceNode.type != VisionNodeIds::templateSource()
                && !inputPortAlreadyWired(QStringLiteral("in"))) {
                semanticKey = QStringLiteral("image");
            }
        }

        if (const ModulePortDef *port = desc.findPort(conn.targetPortId)) {
            req->inputDefinitions.insert(semanticKey, port->toTypedPortDef());
            // Keep distinct Image ports (e.g. "template") from collapsing onto "image".
            if (port->dataType == DataTypeId::Image
                && (port->id == QLatin1String("in") || port->id == QLatin1String("image"))) {
                semanticKey = QStringLiteral("image");
            }
        }
        req->inputs.insert(semanticKey, value);
    }

    // ROI priority: upstream ROI/Region > node parameter ROI > empty.
    auto elevateRoiParams = [&](const RoiRect &roi) {
        QJsonObject params = req->parameters;
        params.insert(QStringLiteral("roi"), roi.toJson());
        req->parameters = params;
    };

    if (req->inputs.contains(QStringLiteral("roi"))) {
        const DataValue roiValue = req->inputs.value(QStringLiteral("roi"));
        if (!roiValue.isNull() && roiValue.type() == DataTypeId::Roi) {
            elevateRoiParams(roiValue.toRoi());
        }
    } else if (req->inputs.contains(QStringLiteral("region"))) {
        const DataValue regionValue = req->inputs.value(QStringLiteral("region"));
        if (!regionValue.isNull() && regionValue.type() == DataTypeId::Region) {
            const RegionData region = regionValue.toRegion();
            QRectF box;
            if (!region.regions.isEmpty())
                box = region.regions.first().boundingRect;
            if (box.isValid() && box.width() > 0 && box.height() > 0) {
                RoiRect roi;
                roi.rect = box;
                roi.enabled = true;
                elevateRoiParams(roi);
                req->inputs.insert(QStringLiteral("roi"), DataValue(roi));
            }
        }
    }

    // Required inputs without connections fail (except image loader).
    for (const ModulePortDef &port : desc.ports) {
        if (!port.isInput || !port.required)
            continue;
        const QString semantic = (port.id == QLatin1String("in")) ? QStringLiteral("image") : port.id;
        if (req->inputs.contains(semantic) || req->inputs.contains(port.id))
            continue;
        if (node.type == VisionNodeIds::imageLoader())
            continue;
        if (error) {
            *error = QStringLiteral("缺少必需输入: %1").arg(port.name.isEmpty() ? port.id : port.name);
        }
        return false;
    }
    return true;
}

} // namespace

QStringList FlowRuntime::topologicalOrder(const Document &document, QString *error)
{
    QHash<QString, QStringList> outgoing;
    QHash<QString, int> inDegree;
    QSet<QString> visionIds;

    for (const NodeModel &node : document.nodes()) {
        if (!VisionNodeIds::isVisionType(node.type))
            continue;
        visionIds.insert(node.id);
        inDegree.insert(node.id, 0);
    }

    for (const ConnectionModel &conn : document.connections()) {
        if (!visionIds.contains(conn.sourceNodeId) || !visionIds.contains(conn.targetNodeId))
            continue;
        outgoing[conn.sourceNodeId].append(conn.targetNodeId);
        inDegree[conn.targetNodeId] = inDegree.value(conn.targetNodeId) + 1;
    }

    QStringList queue;
    for (const QString &id : visionIds) {
        if (inDegree.value(id) == 0)
            queue.append(id);
    }
    queue.sort();

    QStringList order;
    while (!queue.isEmpty()) {
        queue.sort();
        const QString current = queue.takeFirst();
        order.append(current);
        QStringList nexts = outgoing.value(current);
        nexts.sort();
        for (const QString &next : nexts) {
            inDegree[next] = inDegree.value(next) - 1;
            if (inDegree.value(next) == 0)
                queue.append(next);
        }
    }

    if (order.size() != visionIds.size()) {
        if (error)
            *error = QStringLiteral("流程存在环路或不可达节点");
        return {};
    }
    return order;
}

bool FlowRuntime::executeOnce(const Document &document, VisionContext &context, CancellationToken *token)
{
    ProjectVariableStore emptyVars;
    return executeOnce(document, context, emptyVars, token);
}

bool FlowRuntime::executeOnce(const Document &document, VisionContext &context,
                              const ProjectVariableStore &variables, CancellationToken *token)
{
    RuntimeExecuteOptions options;
    options.token = token;
    return executeOnce(document, context, variables, options);
}

bool FlowRuntime::executeOnce(const Document &document, VisionContext &context,
                              const ProjectVariableStore &variables,
                              const ExecutionScope &scope,
                              CancellationToken *token)
{
    RuntimeExecuteOptions options;
    options.token = token;
    options.scope = scope;
    return executeOnce(document, context, variables, options);
}

bool FlowRuntime::executeOnce(const Document &document, VisionContext &context,
                              const ProjectVariableStore &variables,
                              const RuntimeExecuteOptions &options)
{
    const FlowExecutionSnapshot snapshot =
        FlowExecutionSnapshot::create(document, options.scope);
    const Document &executionDocument = snapshot.document();
    CancellationToken *token = options.token;
    const RuntimeProgressFn &onProgress = options.onProgress;

    context = VisionContext{};
    context.mode = ExecutionMode::Once;
    context.executionScope = options.scope.description();
    context.snapshotCreatedAt = snapshot.createdAt();
    for (const NodeModel &node : document.nodes()) {
        if (!snapshot.nodeIds().contains(node.id))
            context.skippedNodeIds.append(node.id);
    }
    QElapsedTimer timer;
    timer.start();

    const QVector<GraphDiagnostic> preflight =
        GraphValidator::validateDocument(executionDocument);
    context.diagnostics = preflight;
    if (GraphValidator::hasErrors(preflight)) {
        context.setError(QStringLiteral("静态校验失败"));
        context.appendLog(QStringLiteral("静态校验失败:"));
        for (const GraphDiagnostic &d : preflight) {
            if (d.severity != GraphDiagnosticSeverity::Error)
                continue;
            context.runtimeDiagnostics.append(
                RuntimeDiagnostic::make(RuntimeFailureKind::Validation, d.message, d.nodeId,
                                        d.code, d.portId, d.parameterKey));
        }
        for (const QString &line : GraphValidator::toMessages(preflight))
            context.appendLog(line);
        context.elapsedMs = timer.elapsed();
        return false;
    }
    for (const GraphDiagnostic &d : preflight) {
        if (d.severity == GraphDiagnosticSeverity::Warning)
            context.appendLog(QStringLiteral("[警告] %1").arg(d.message));
    }

    QString topoError;
    const QStringList order = topologicalOrder(executionDocument, &topoError);
    context.executionOrder = order;
    if (order.isEmpty()) {
        context.setError(topoError.isEmpty() ? QStringLiteral("流程中没有视觉节点") : topoError);
        return false;
    }

    ExecutionContext execCtx;
    if (options.liveFrame && !options.liveFrame->isEmpty())
        execCtx.setLiveFrame(*options.liveFrame);
    if (options.resources)
        execCtx.setResourceStore(options.resources);
    if (options.hasCalibration)
        execCtx.setCalibration(options.calibration);

    VariableScopeSnapshot scopeSnapshot = options.hasVariableScopes
        ? options.variableScopes
        : VariableScopeSnapshot::fromStores(&variables);
    execCtx.setVariableScopeSnapshot(&scopeSnapshot);
    const VariableScopeChain scopeChain = scopeSnapshot.chain();

    context.frameId = options.frameId;
    context.deviceId = options.deviceId;
    context.sourceDescription = options.sourceDescription;
    context.batchId = options.batchId;
    context.frameGrabbedAt = options.frameGrabbedAt;
    context.retainImageSnapshots = options.retainImageSnapshots;
    if (options.hasCalibration && options.calibration.valid)
        context.calibrationId = options.calibration.calibrationId;

    PortValueStore portStore;
    VisionImage currentImage;

    auto commitModuleResult = [&](const QString &id, ModuleRunResult result) {
        if (!context.retainImageSnapshots)
            result.outputImage = VisionImage{};
        context.moduleResults.insert(id, result);
        context.runRecords.clear();
        for (const QString &orderedId : order) {
            if (context.moduleResults.contains(orderedId))
                context.runRecords.append(context.moduleResults.value(orderedId));
        }
    };

    auto reportProgress = [&](const QString &nodeId, ModuleStatus status) {
        if (onProgress)
            onProgress(nodeId, status);
    };

    for (const QString &nodeId : order) {
        try {
        if (token && token->isCancelled()) {
            ModuleRunResult cancelled;
            cancelled.nodeId = nodeId;
            cancelled.status = ModuleStatus::Failed;
            cancelled.failureKind = runtimeFailureKindToString(RuntimeFailureKind::Cancelled);
            cancelled.errorMessage = QStringLiteral("执行已取消");
            commitModuleResult(nodeId, cancelled);
            const RuntimeDiagnostic rd = RuntimeDiagnostic::make(
                RuntimeFailureKind::Cancelled, QStringLiteral("执行已取消"), nodeId);
            context.runtimeDiagnostics.append(rd);
            context.diagnostics.append(rd.toGraphDiagnostic());
            context.setError(QStringLiteral("执行已取消"), nodeId);
            reportProgress(nodeId, ModuleStatus::Failed);
            context.elapsedMs = timer.elapsed();
            return false;
        }

        const NodeModel node = executionDocument.node(nodeId);
        ModuleRunResult moduleResult;
        moduleResult.nodeId = nodeId;
        moduleResult.type = node.type;
        moduleResult.displayName = VisionNodeRegistry::displayName(node.type);

        if (!node.enabled) {
            moduleResult.status = ModuleStatus::Disabled;
            moduleResult.errorMessage = QStringLiteral("节点已禁用");
            moduleResult.failureKind = runtimeFailureKindToString(RuntimeFailureKind::Disabled);
            commitModuleResult(nodeId, moduleResult);
            context.runtimeDiagnostics.append(
                RuntimeDiagnostic::make(RuntimeFailureKind::Disabled,
                                        moduleResult.errorMessage, nodeId));
            context.appendLog(QStringLiteral("[%1] 已跳过（禁用）").arg(moduleResult.displayName));
            reportProgress(nodeId, ModuleStatus::Disabled);
            continue;
        }

        if (node.breakpoint) {
            moduleResult.status = ModuleStatus::Failed;
            moduleResult.errorMessage = QStringLiteral("命中断点，执行已暂停");
            moduleResult.failureKind = runtimeFailureKindToString(RuntimeFailureKind::Cancelled);
            commitModuleResult(nodeId, moduleResult);
            context.runtimeDiagnostics.append(
                RuntimeDiagnostic::make(RuntimeFailureKind::Cancelled,
                                        moduleResult.errorMessage, nodeId, QStringLiteral("breakpoint")));
            context.setError(moduleResult.errorMessage, nodeId);
            reportProgress(nodeId, ModuleStatus::Failed);
            context.appendLog(QStringLiteral("[断点] %1").arg(nodeId));
            context.elapsedMs = timer.elapsed();
            return false;
        }

        moduleResult.status = ModuleStatus::Running;
        reportProgress(nodeId, ModuleStatus::Running);
        const ModuleDescriptor desc = VisionNodeRegistry::descriptor(node.type);

        QString paramError;
        QString failedKey;
        QJsonObject legacyParams;
        QHash<QString, DataValue> typedParams;
        if (!BindingResolver::resolveNodeParameters(node, desc.parameters, portStore, scopeChain,
                                                    &legacyParams, &typedParams, &paramError, &failedKey)) {
            moduleResult.status = ModuleStatus::Failed;
            moduleResult.errorMessage = paramError;
            moduleResult.failureKind = runtimeFailureKindToString(RuntimeFailureKind::Binding);
            commitModuleResult(nodeId, moduleResult);
            const RuntimeDiagnostic rd = RuntimeDiagnostic::make(
                RuntimeFailureKind::Binding, paramError, nodeId, QStringLiteral("binding_resolve"),
                {}, failedKey);
            context.runtimeDiagnostics.append(rd);
            context.diagnostics.append(rd.toGraphDiagnostic());
            context.setError(paramError, nodeId);
            reportProgress(nodeId, ModuleStatus::Failed);
            context.elapsedMs = timer.elapsed();
            return false;
        }

        if (!VisionNodeRegistry::validateParameters(node.type, legacyParams, &paramError)) {
            moduleResult.status = ModuleStatus::Failed;
            moduleResult.errorMessage = paramError;
            moduleResult.failureKind = runtimeFailureKindToString(RuntimeFailureKind::Parameter);
            commitModuleResult(nodeId, moduleResult);
            const RuntimeDiagnostic rd = RuntimeDiagnostic::make(
                RuntimeFailureKind::Parameter, paramError, nodeId, QStringLiteral("parameter"));
            context.runtimeDiagnostics.append(rd);
            context.diagnostics.append(rd.toGraphDiagnostic());
            context.setError(paramError, nodeId);
            reportProgress(nodeId, ModuleStatus::Failed);
            context.elapsedMs = timer.elapsed();
            return false;
        }

        ExecutionRequest req;
        req.nodeId = nodeId;
        req.typeId = node.type;
        req.parameters = legacyParams;
        req.typedParameters = typedParams;

        QString inputError;
        if (!collectNodeInputs(executionDocument, node, desc, portStore, &req, &inputError)) {
            if (options.scope.kind != ExecutionScopeKind::FullFlow
                && inputError.startsWith(QStringLiteral("缺少必需输入"))) {
                inputError = QStringLiteral("范围外缺少输入: %1").arg(inputError);
            }
            moduleResult.status = ModuleStatus::Failed;
            moduleResult.errorMessage = inputError;
            moduleResult.failureKind = runtimeFailureKindToString(RuntimeFailureKind::Binding);
            commitModuleResult(nodeId, moduleResult);
            const RuntimeDiagnostic rd = RuntimeDiagnostic::make(
                RuntimeFailureKind::Binding, inputError, nodeId, QStringLiteral("missing_input"));
            context.runtimeDiagnostics.append(rd);
            context.diagnostics.append(rd.toGraphDiagnostic());
            context.setError(inputError, nodeId);
            reportProgress(nodeId, ModuleStatus::Failed);
            context.elapsedMs = timer.elapsed();
            return false;
        }

        for (auto it = req.inputs.cbegin(); it != req.inputs.cend(); ++it)
            moduleResult.inputSummary.insert(it.key(), it.value().debugString());

        if (!req.inputs.contains(QStringLiteral("image")) && !currentImage.isEmpty())
            req.inputs.insert(QStringLiteral("image"), DataValue(currentImage));

        NodeExecutorPtr executor = NodeExecutorRegistry::instance().create(node.type);
        if (!executor) {
            moduleResult.status = ModuleStatus::Failed;
            moduleResult.errorMessage = QStringLiteral("未注册执行器: %1").arg(node.type);
            moduleResult.failureKind = runtimeFailureKindToString(RuntimeFailureKind::Execution);
            commitModuleResult(nodeId, moduleResult);
            context.setError(moduleResult.errorMessage, nodeId);
            reportProgress(nodeId, ModuleStatus::Failed);
            context.elapsedMs = timer.elapsed();
            return false;
        }

        if (token && token->isCancelled()) {
            moduleResult.status = ModuleStatus::Failed;
            moduleResult.failureKind = runtimeFailureKindToString(RuntimeFailureKind::Cancelled);
            moduleResult.errorMessage = QStringLiteral("节点已禁用");
            commitModuleResult(nodeId, moduleResult);
            context.setError(moduleResult.errorMessage, nodeId);
            reportProgress(nodeId, ModuleStatus::Failed);
            context.elapsedMs = timer.elapsed();
            return false;
        }

        ExecutionResult result;
        try {
            result = executor->execute(req, execCtx);
        } catch (const cv::Exception &e) {
            result.status = ModuleStatus::Failed;
            result.errorMessage = QStringLiteral("OpenCV 异常: %1")
                                      .arg(QString::fromLocal8Bit(e.what()));
            result.diagnosticCode = QStringLiteral("opencv_exception");
            result.elapsedMs = 0;
        } catch (const std::exception &e) {
            result.status = ModuleStatus::Failed;
            result.errorMessage =
                QStringLiteral("执行异常: %1").arg(QString::fromLocal8Bit(e.what()));
            result.diagnosticCode = QStringLiteral("executor_exception");
            result.elapsedMs = 0;
        } catch (...) {
            result.status = ModuleStatus::Failed;
            result.errorMessage = QStringLiteral("未知执行异常");
            result.diagnosticCode = QStringLiteral("executor_unknown_exception");
            result.elapsedMs = 0;
        }
        moduleResult.status = result.status;
        moduleResult.errorMessage = result.errorMessage;
        moduleResult.elapsedMs = result.elapsedMs;
        moduleResult.overlays = result.overlays;
        moduleResult.measurement = result.measurement;
        moduleResult.failureKind = result.status == ModuleStatus::Success
            ? QString()
            : runtimeFailureKindToString(RuntimeFailureKind::Execution);
        // 保留诊断码（含 Success 下的 no_target / low_confidence），供运行监视展示。
        moduleResult.diagnosticCode = result.diagnosticCode;

        for (auto it = result.outputs.cbegin(); it != result.outputs.cend(); ++it) {
            const QString typeId = Selt::dataTypeIdToString(it.value().type());
            moduleResult.outputTypes.insert(it.key(), typeId);
            moduleResult.outputSummary.insert(it.key(), it.value().debugString());

            PortValueRecord record;
            record.portId = it.key();
            record.typeId = typeId;
            record.valueSummary = it.value().displaySummary();
            record.valueDetail = it.value().displayDetail();
            record.status = QStringLiteral("ok");
            if (const ModulePortDef *port = desc.findOutputPort(it.key())) {
                record.displayName = port->name.isEmpty() ? it.key() : port->name;
                record.category = port->group.isEmpty()
                    ? Selt::defaultPortGroup(port->dataType, false)
                    : port->group;
            } else {
                record.displayName = it.key();
                record.category = Selt::defaultPortGroup(it.value().type(), false);
            }
            if (result.diagnosticCode == QLatin1String("backend_missing")
                || result.diagnosticCode == QLatin1String("capability_limited")) {
                record.status = QStringLiteral("capability_missing");
            } else if (result.status == ModuleStatus::Failed) {
                record.status = QStringLiteral("fail");
            } else if (!result.diagnosticCode.isEmpty()
                       && result.diagnosticCode != QLatin1String("none")) {
                record.status = QStringLiteral("warn");
            }
            moduleResult.outputPortRecords.append(record);
        }
        if (!result.diagnosticCode.isEmpty())
            moduleResult.outputSummary.insert(QStringLiteral("diagnosticCode"), result.diagnosticCode);
        if (result.outputs.contains(QStringLiteral("image"))) {
            currentImage = result.outputs.value(QStringLiteral("image")).toImage();
            moduleResult.outputImage = currentImage.clone();
            ++context.imageCopyCount;
        }
        if (result.outputs.contains(QStringLiteral("debug"))) {
            const VisionImage dbg = result.outputs.value(QStringLiteral("debug")).toImage();
            if (!dbg.isEmpty())
                moduleResult.debugImage = dbg.clone();
        }

        storeExecutionOutputs(node, desc, result, &portStore);

        if (result.status != ModuleStatus::Success) {
            commitModuleResult(nodeId, moduleResult);
            const RuntimeDiagnostic rd = RuntimeDiagnostic::make(
                RuntimeFailureKind::Execution,
                result.errorMessage.isEmpty() ? QStringLiteral("节点执行失败") : result.errorMessage,
                nodeId,
                result.diagnosticCode.isEmpty()
                    ? DiagnosticCodes::measureFailed()
                    : result.diagnosticCode);
            context.runtimeDiagnostics.append(rd);
            context.diagnostics.append(rd.toGraphDiagnostic());
            context.setError(result.errorMessage.isEmpty()
                                 ? QStringLiteral("节点执行失败")
                                 : result.errorMessage,
                             nodeId);
            reportProgress(nodeId, ModuleStatus::Failed);
            context.elapsedMs = timer.elapsed();
            return false;
        }

        if (node.type == VisionNodeIds::imageLoader())
            context.originalImage = currentImage.clone();
        if (node.type == VisionNodeIds::threshold())
            context.binaryImage = currentImage.clone();
        if (result.measurement.valid) {
            context.measurement = result.measurement;
            context.resultImage = currentImage.clone();
            context.recentMeasurements.append(result.measurement);
            while (context.recentMeasurements.size() > 32)
                context.recentMeasurements.removeFirst();
        }
        context.currentImage = currentImage.clone();
        commitModuleResult(nodeId, moduleResult);
        context.appendLog(QStringLiteral("[%1] 完成 (%2 ms)")
                              .arg(moduleResult.displayName)
                              .arg(moduleResult.elapsedMs));
        reportProgress(nodeId, ModuleStatus::Success);
        } catch (const std::exception &e) {
            context.setError(QStringLiteral("执行异常: %1").arg(QString::fromLocal8Bit(e.what())),
                              nodeId);
            context.elapsedMs = timer.elapsed();
            return false;
        } catch (...) {
            context.setError(QStringLiteral("未知执行异常"), nodeId);
            context.elapsedMs = timer.elapsed();
            return false;
        }
    }

    context.elapsedMs = timer.elapsed();
    if (!scopeSnapshot.runtimeValues.isEmpty()) {
        context.appendLog(QStringLiteral("运行时变量写回："));
        for (auto it = scopeSnapshot.runtimeValues.constBegin();
             it != scopeSnapshot.runtimeValues.constEnd(); ++it) {
            context.appendLog(QStringLiteral("  %1 = %2")
                                  .arg(it.key(), it.value().debugString()));
        }
    }
    context.appendLog(QStringLiteral("流程完成, 耗时 %1 ms").arg(context.elapsedMs));
    return true;
}

} // namespace Selt
