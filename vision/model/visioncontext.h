#ifndef VISIONCONTEXT_H
#define VISIONCONTEXT_H

#include "measurementresult.h"
#include "modulestatus.h"
#include "overlayitems.h"
#include "visionimage.h"

#include <QHash>
#include <QStringList>

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
    QStringList executionOrder;

    void appendLog(const QString &line);
    void setError(const QString &message, const QString &nodeId = QString());
    bool hasError() const { return !errorMessage.isEmpty(); }
    ModuleRunResult moduleResult(const QString &nodeId) const;
    VisionImage imageForNode(const QString &nodeId) const;
    OverlayList overlaysForNode(const QString &nodeId) const;
};

#endif // VISIONCONTEXT_H
