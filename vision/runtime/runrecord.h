#pragma once

#include "vision/model/modulestatus.h"
#include "vision/model/overlayitems.h"
#include "vision/model/visioncontext.h"
#include "vision/model/visionimage.h"

#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace Selt {

/// Controls whether heavy image payloads are retained in run records.
enum class RunRecordImagePolicy {
    DebugCopy,      ///< Keep output image snapshots for inspector / debug.
    ProductionLite  ///< Skip image clones; keep summaries / overlays metadata only.
};

/// Lightweight per-node run record shared by monitor, preview, InspectionPack and ResultSink.
struct RunRecord
{
    QString nodeId;
    QString typeId;
    QString displayName;
    ModuleStatus status{ModuleStatus::Idle};
    QHash<QString, QString> inputSummary;
    QHash<QString, QString> outputSummary;
    QHash<QString, QString> outputTypes;
    VisionImage imageSnapshot;
    OverlayList overlays;
    qint64 elapsedMs{0};
    QString diagnosticCode;
    QString failureKind;
    QString errorMessage;

    QJsonObject toJson(bool includeImage = false) const;
    static RunRecord fromJson(const QJsonObject &obj);
    static RunRecord fromModuleResult(const ModuleRunResult &result,
                                      RunRecordImagePolicy policy = RunRecordImagePolicy::DebugCopy);
};

QVector<RunRecord> buildRunRecords(const VisionContext &context,
                                   RunRecordImagePolicy policy = RunRecordImagePolicy::DebugCopy);

} // namespace Selt
