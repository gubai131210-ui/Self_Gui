#ifndef OPERATOROUTCOME_H
#define OPERATOROUTCOME_H

#include "vision/model/modulestatus.h"
#include "vision/runtime/diagnosticcodes.h"
#include "vision/runtime/inodeexecutor.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

namespace Selt {

/// Fine-grained industrial operator outcome (maps onto ModuleStatus + diagnosticCode).
enum class OperatorOutcome {
    Success,
    SuccessWithWarning,
    NoCandidate,
    RejectedByQuality,
    InvalidGeometry,
    InvalidRoi,
    Timeout,
    Failed
};

inline QString operatorOutcomeToString(OperatorOutcome outcome)
{
    switch (outcome) {
    case OperatorOutcome::Success:
        return QStringLiteral("Success");
    case OperatorOutcome::SuccessWithWarning:
        return QStringLiteral("SuccessWithWarning");
    case OperatorOutcome::NoCandidate:
        return QStringLiteral("NoCandidate");
    case OperatorOutcome::RejectedByQuality:
        return QStringLiteral("RejectedByQuality");
    case OperatorOutcome::InvalidGeometry:
        return QStringLiteral("InvalidGeometry");
    case OperatorOutcome::InvalidRoi:
        return QStringLiteral("InvalidRoi");
    case OperatorOutcome::Timeout:
        return QStringLiteral("Timeout");
    case OperatorOutcome::Failed:
    default:
        return QStringLiteral("Failed");
    }
}

inline ModuleStatus moduleStatusForOutcome(OperatorOutcome outcome)
{
    switch (outcome) {
    case OperatorOutcome::Success:
        return ModuleStatus::Success;
    case OperatorOutcome::SuccessWithWarning:
        return ModuleStatus::SuccessWithWarning;
    case OperatorOutcome::NoCandidate:
    case OperatorOutcome::RejectedByQuality:
    case OperatorOutcome::InvalidGeometry:
    case OperatorOutcome::InvalidRoi:
    case OperatorOutcome::Timeout:
    case OperatorOutcome::Failed:
    default:
        return ModuleStatus::Failed;
    }
}

inline QString diagnosticCodeForOutcome(OperatorOutcome outcome)
{
    switch (outcome) {
    case OperatorOutcome::Success:
        return {};
    case OperatorOutcome::SuccessWithWarning:
        return DiagnosticCodes::lowConfidence();
    case OperatorOutcome::NoCandidate:
        return DiagnosticCodes::noCandidate();
    case OperatorOutcome::RejectedByQuality:
        return DiagnosticCodes::rejectedByQuality();
    case OperatorOutcome::InvalidGeometry:
        return DiagnosticCodes::invalidGeometry();
    case OperatorOutcome::InvalidRoi:
        return DiagnosticCodes::invalidRoi();
    case OperatorOutcome::Timeout:
        return DiagnosticCodes::timeout();
    case OperatorOutcome::Failed:
    default:
        return DiagnosticCodes::measureFailed();
    }
}

struct OperatorResultMeta
{
    OperatorOutcome outcome{OperatorOutcome::Success};
    QString failureStage;
    double qualityScore{1.0};
    QJsonObject autoSnapshot;
    QString message;
};

inline void applyOperatorMeta(ExecutionResult &result, const OperatorResultMeta &meta)
{
    result.status = moduleStatusForOutcome(meta.outcome);
    result.failureStage = meta.failureStage;
    result.qualityScore = meta.qualityScore;
    result.autoSnapshot = meta.autoSnapshot;
    if (result.diagnosticCode.isEmpty())
        result.diagnosticCode = diagnosticCodeForOutcome(meta.outcome);
    if (result.errorMessage.isEmpty() && !meta.message.isEmpty()
        && (result.status == ModuleStatus::Failed
            || result.status == ModuleStatus::SuccessWithWarning)) {
        result.errorMessage = meta.message;
    }
    if (!meta.autoSnapshot.isEmpty() || !meta.failureStage.isEmpty()) {
        if (!meta.autoSnapshot.isEmpty()) {
            result.outputs.insert(
                QStringLiteral("autoSnapshot"),
                DataValue(QString::fromUtf8(
                    QJsonDocument(meta.autoSnapshot).toJson(QJsonDocument::Compact))));
        }
        result.outputs.insert(QStringLiteral("failureStage"), DataValue(meta.failureStage));
        result.outputs.insert(QStringLiteral("qualityScore"), DataValue(meta.qualityScore));
        result.outputs.insert(QStringLiteral("outcome"),
                              DataValue(operatorOutcomeToString(meta.outcome)));
    }
}

} // namespace Selt

#endif // OPERATOROUTCOME_H
