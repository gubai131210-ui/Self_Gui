#include "vision/runtime/runrecord.h"

#include "vision/model/modulestatus.h"

#include <QJsonArray>

namespace Selt {
namespace {

/// Machine-readable status codes for RunRecord JSON (not UI display names).
QString runRecordStatusCode(ModuleStatus status)
{
    switch (status) {
    case ModuleStatus::Running:
        return QStringLiteral("running");
    case ModuleStatus::Success:
        return QStringLiteral("success");
    case ModuleStatus::SuccessWithWarning:
        return QStringLiteral("success_with_warning");
    case ModuleStatus::Failed:
        return QStringLiteral("failed");
    case ModuleStatus::Disabled:
        return QStringLiteral("disabled");
    case ModuleStatus::Idle:
    default:
        return QStringLiteral("idle");
    }
}

ModuleStatus runRecordStatusFromCode(const QString &text)
{
    if (text == QLatin1String("running"))
        return ModuleStatus::Running;
    if (text == QLatin1String("success"))
        return ModuleStatus::Success;
    if (text == QLatin1String("success_with_warning"))
        return ModuleStatus::SuccessWithWarning;
    if (text == QLatin1String("failed"))
        return ModuleStatus::Failed;
    if (text == QLatin1String("disabled"))
        return ModuleStatus::Disabled;
    return ModuleStatus::Idle;
}

QJsonObject hashToJson(const QHash<QString, QString> &hash)
{
    QJsonObject obj;
    for (auto it = hash.cbegin(); it != hash.cend(); ++it)
        obj.insert(it.key(), it.value());
    return obj;
}

QHash<QString, QString> hashFromJson(const QJsonObject &obj)
{
    QHash<QString, QString> hash;
    for (auto it = obj.begin(); it != obj.end(); ++it)
        hash.insert(it.key(), it.value().toString());
    return hash;
}

} // namespace

QJsonObject RunRecord::toJson(bool includeImage) const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("nodeId"), nodeId);
    obj.insert(QStringLiteral("typeId"), typeId);
    obj.insert(QStringLiteral("displayName"), displayName);
    obj.insert(QStringLiteral("status"), runRecordStatusCode(status));
    obj.insert(QStringLiteral("inputSummary"), hashToJson(inputSummary));
    obj.insert(QStringLiteral("outputSummary"), hashToJson(outputSummary));
    obj.insert(QStringLiteral("outputTypes"), hashToJson(outputTypes));
    obj.insert(QStringLiteral("elapsedMs"), static_cast<double>(elapsedMs));
    obj.insert(QStringLiteral("diagnosticCode"), diagnosticCode);
    obj.insert(QStringLiteral("failureKind"), failureKind);
    obj.insert(QStringLiteral("errorMessage"), errorMessage);
    obj.insert(QStringLiteral("overlayCount"), overlays.size());
    if (includeImage && !imageSnapshot.isEmpty())
        obj.insert(QStringLiteral("hasImage"), true);
    else
        obj.insert(QStringLiteral("hasImage"), !imageSnapshot.isEmpty());
    return obj;
}

RunRecord RunRecord::fromJson(const QJsonObject &obj)
{
    RunRecord record;
    record.nodeId = obj.value(QStringLiteral("nodeId")).toString();
    record.typeId = obj.value(QStringLiteral("typeId")).toString();
    record.displayName = obj.value(QStringLiteral("displayName")).toString();
    record.status = runRecordStatusFromCode(obj.value(QStringLiteral("status")).toString());
    record.inputSummary = hashFromJson(obj.value(QStringLiteral("inputSummary")).toObject());
    record.outputSummary = hashFromJson(obj.value(QStringLiteral("outputSummary")).toObject());
    record.outputTypes = hashFromJson(obj.value(QStringLiteral("outputTypes")).toObject());
    record.elapsedMs = static_cast<qint64>(obj.value(QStringLiteral("elapsedMs")).toDouble());
    record.diagnosticCode = obj.value(QStringLiteral("diagnosticCode")).toString();
    record.failureKind = obj.value(QStringLiteral("failureKind")).toString();
    record.errorMessage = obj.value(QStringLiteral("errorMessage")).toString();
    return record;
}

RunRecord RunRecord::fromModuleResult(const ModuleRunResult &result, RunRecordImagePolicy policy)
{
    RunRecord record;
    record.nodeId = result.nodeId;
    record.typeId = result.type;
    record.displayName = result.displayName;
    record.status = result.status;
    record.inputSummary = result.inputSummary;
    record.outputSummary = result.outputSummary;
    record.outputTypes = result.outputTypes;
    record.overlays = result.overlays;
    record.elapsedMs = result.elapsedMs;
    record.diagnosticCode = result.diagnosticCode;
    record.failureKind = result.failureKind;
    record.errorMessage = result.errorMessage;
    if (policy == RunRecordImagePolicy::DebugCopy && !result.outputImage.isEmpty())
        record.imageSnapshot = result.outputImage.clone();
    return record;
}

QVector<RunRecord> buildRunRecords(const VisionContext &context, RunRecordImagePolicy policy)
{
    QVector<RunRecord> records;
    records.reserve(context.executionOrder.size());
    for (const QString &nodeId : context.executionOrder) {
        if (!context.moduleResults.contains(nodeId))
            continue;
        records.append(RunRecord::fromModuleResult(context.moduleResults.value(nodeId), policy));
    }
    if (records.isEmpty()) {
        for (auto it = context.moduleResults.cbegin(); it != context.moduleResults.cend(); ++it)
            records.append(RunRecord::fromModuleResult(it.value(), policy));
    }
    return records;
}

} // namespace Selt
