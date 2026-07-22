#ifndef VISIONCONTEXT_H
#define VISIONCONTEXT_H

#include "measurementresult.h"
#include "modulestatus.h"
#include "overlayitems.h"
#include "visionimage.h"
#include "vision/runtime/runtimediagnostic.h"
#include "vision/validation/graphdiagnostic.h"
#include <QDateTime>

#include <QHash>
#include <QStringList>
#include <QVector>

struct ModuleRunResult
{
    QString nodeId;
    QString type;
    QString displayName;
    ModuleStatus status{ModuleStatus::Idle};
    VisionImage outputImage;
    OverlayList overlays;
    MeasurementResult measurement;
    QString errorMessage;
    /// parameter | binding | execution | cancelled | disabled | device | validation
    QString failureKind;
    /// Stable machine code (e.g. image_empty); independent of failureKind taxonomy.
    QString diagnosticCode;
    QHash<QString, QString> inputSummary;
    QHash<QString, QString> outputSummary;
    qint64 elapsedMs{0};
};

enum class ExecutionMode {
    Once
};

struct VisionContext
{
    VisionImage originalImage;
    VisionImage currentImage;
    VisionImage binaryImage;
    VisionImage resultImage;
    MeasurementResult measurement;
    QString errorMessage;
    QString failedNodeId;
    QStringList log;
    qint64 elapsedMs{0};
    ExecutionMode mode{ExecutionMode::Once};
    QHash<QString, ModuleRunResult> moduleResults;
    /// Ordered lightweight run records derived from moduleResults (same semantic channel).
    QVector<ModuleRunResult> runRecords;
    QStringList executionOrder;
    QVector<Selt::GraphDiagnostic> diagnostics;
    QVector<Selt::RuntimeDiagnostic> runtimeDiagnostics;
    QString executionScope;
    QDateTime snapshotCreatedAt;
    QStringList skippedNodeIds;
    qint64 imageCopyCount{0};
    qint64 cacheHitCount{0};

    /// Industrial run trace metadata.
    qint64 frameId{0};
    QString deviceId;
    QString sourceDescription;
    QString calibrationId;
    QString batchId;
    QDateTime frameGrabbedAt;
    QVector<MeasurementResult> recentMeasurements;
    /// DebugCopy keeps images in ModuleRunResult; ProductionLite may clear snapshots later.
    bool retainImageSnapshots{true};

    void appendLog(const QString &line);
    void setError(const QString &message, const QString &nodeId = QString());
    bool hasError() const { return !errorMessage.isEmpty(); }
    ModuleRunResult moduleResult(const QString &nodeId) const;
    VisionImage imageForNode(const QString &nodeId) const;
    OverlayList overlaysForNode(const QString &nodeId) const;
};

#endif // VISIONCONTEXT_H
