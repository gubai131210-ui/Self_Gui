#ifndef IMAGEEXECUTORHELPERS_H
#define IMAGEEXECUTORHELPERS_H

#include "vision/algorithms/roiapplier.h"
#include "vision/model/autoparameter.h"
#include "vision/model/calibrationmodel.h"
#include "vision/model/measurementresult.h"
#include "vision/model/regiondata.h"
#include "vision/runtime/diagnosticcodes.h"
#include "vision/runtime/inodeexecutor.h"

#include <QElapsedTimer>
#include <QJsonObject>

namespace Selt {

/// Apply ROI from request parameters. Disabled/absent ROI returns the original image.
/// Invalid/out-of-bounds ROI returns an empty image and optionally fills error/code.
inline RoiApplyMode resolveRoiApplyMode(const QJsonObject &params)
{
    const QString mode = params.value(QStringLiteral("roiApplyMode")).toString(QStringLiteral("mask"));
    if (mode == QLatin1String("crop_masked") || mode == QLatin1String("cropMasked"))
        return RoiApplyMode::CropMasked;
    if (mode == QLatin1String("crop"))
        return RoiApplyMode::Crop;
    return RoiApplyMode::MaskOutside;
}

/// Priority: upstream ROI input > Region bounding box > parameter ROI > empty.
inline ExtendedRoi resolveEffectiveRoi(const ExecutionRequest &request)
{
    if (request.inputs.contains(QStringLiteral("roi"))) {
        const DataValue value = request.inputs.value(QStringLiteral("roi"));
        if (!value.isNull() && value.type() == DataTypeId::Roi) {
            ExtendedRoi roi = ExtendedRoi::fromLegacyRect(value.toRoi());
            if (roi.isValid())
                return roi;
        }
    }
    if (request.inputs.contains(QStringLiteral("region"))) {
        const DataValue value = request.inputs.value(QStringLiteral("region"));
        if (!value.isNull() && value.type() == DataTypeId::Region) {
            const RegionData region = value.toRegion();
            if (!region.regions.isEmpty() && region.regions.first().boundingRect.isValid()) {
                RoiRect legacy;
                legacy.rect = region.regions.first().boundingRect;
                legacy.enabled = true;
                return ExtendedRoi::fromLegacyRect(legacy);
            }
        }
    }
    // Prefer elevated/legacy `roi` over stale `extendedRoi` when FlowRuntime marked it.
    const bool preferParamRoi =
        request.parameters.value(QStringLiteral("_preferParameterRoi")).toBool(false)
        || request.parameters.value(QStringLiteral("_upstreamRoi")).toBool(false);
    return RoiApplier::parseFromParameters(request.parameters, preferParamRoi);
}

inline RoiApplyResult applyRoiResultFromRequest(const VisionImage &input,
                                                const QJsonObject &params,
                                                QString *error = nullptr,
                                                QString *diagnosticCode = nullptr)
{
    const RoiApplyResult applied =
        RoiApplier::applyFromParameters(input, params, resolveRoiApplyMode(params));
    if (!applied.ok) {
        if (error)
            *error = applied.errorMessage;
        if (diagnosticCode)
            *diagnosticCode = applied.diagnosticCode;
    }
    return applied;
}

inline VisionImage applyRoiFromRequest(const VisionImage &input,
                                       const QJsonObject &params,
                                       QString *error = nullptr,
                                       QString *diagnosticCode = nullptr)
{
    return applyRoiResultFromRequest(input, params, error, diagnosticCode).image;
}

inline RoiApplyResult applyEffectiveRoiResult(const VisionImage &input,
                                              const ExecutionRequest &request,
                                              QString *error = nullptr,
                                              QString *diagnosticCode = nullptr)
{
    const RoiApplyResult applied =
        RoiApplier::apply(input, resolveEffectiveRoi(request), resolveRoiApplyMode(request.parameters));
    if (!applied.ok) {
        if (error)
            *error = applied.errorMessage;
        if (diagnosticCode)
            *diagnosticCode = applied.diagnosticCode;
    }
    return applied;
}

inline VisionImage applyEffectiveRoi(const VisionImage &input,
                                     const ExecutionRequest &request,
                                     QString *error = nullptr,
                                     QString *diagnosticCode = nullptr)
{
    return applyEffectiveRoiResult(input, request, error, diagnosticCode).image;
}

/// Load image input and apply ROI. Distinguishes empty image vs invalid ROI diagnostics.
inline VisionImage requireImageWithRoi(const ExecutionRequest &request,
                                       QString *error = nullptr,
                                       QString *diagnosticCode = nullptr)
{
    VisionImage input = request.inputs.value(QStringLiteral("image")).toImage();
    if (input.isEmpty())
        input = request.inputs.value(QStringLiteral("in")).toImage();
    if (input.isEmpty()) {
        if (error)
            *error = QStringLiteral("输入图像为空");
        if (diagnosticCode)
            *diagnosticCode = DiagnosticCodes::imageEmpty();
        return {};
    }
    return applyEffectiveRoi(input, request, error, diagnosticCode);
}

inline VisionImage requireImageWithRoi(const ExecutionRequest &request,
                                       QPointF *originOffsetOut,
                                       QString *error = nullptr,
                                       QString *diagnosticCode = nullptr)
{
    VisionImage input = request.inputs.value(QStringLiteral("image")).toImage();
    if (input.isEmpty())
        input = request.inputs.value(QStringLiteral("in")).toImage();
    if (input.isEmpty()) {
        if (error)
            *error = QStringLiteral("输入图像为空");
        if (diagnosticCode)
            *diagnosticCode = DiagnosticCodes::imageEmpty();
        if (originOffsetOut)
            *originOffsetOut = QPointF(0.0, 0.0);
        return {};
    }
    const RoiApplyResult applied = applyEffectiveRoiResult(input, request, error, diagnosticCode);
    if (originOffsetOut)
        *originOffsetOut = applied.originOffset;
    return applied.image;
}

inline void remapRegionToGlobal(RegionData &region, const QPointF &originOffset)
{
    if (qFuzzyIsNull(originOffset.x()) && qFuzzyIsNull(originOffset.y()))
        return;
    for (RegionStats &s : region.regions) {
        s.centroid += originOffset;
        s.boundingRect.translate(originOffset);
    }
}

inline void remapPointList(QVector<QPointF> &points, const QPointF &originOffset)
{
    if (qFuzzyIsNull(originOffset.x()) && qFuzzyIsNull(originOffset.y()))
        return;
    for (QPointF &p : points)
        p += originOffset;
}

inline MeasurementResult applyCalibrationIfAny(const MeasurementResult &measure, ExecutionContext &context)
{
    if (!context.hasCalibration())
        return measure;
    return context.calibration().applyTo(measure);
}

/// Finalize a length-based measurement: source id, calibration (or keep px when absent).
/// Does not invent physical units without a valid calibration snapshot.
/// When calibration is missing, sets diagnosticCode=calibration_missing on the returned
/// MeasurementResult via message, and caller may also set ExecutionResult.diagnosticCode.
inline MeasurementResult finalizeLengthMeasurement(MeasurementResult measure,
                                                   const ExecutionRequest &request,
                                                   ExecutionContext &context,
                                                   QString *diagnosticCodeOut = nullptr)
{
    measure.sourceNodeId = request.nodeId;
    if (measure.decisionState.isEmpty())
        measure.decisionState = QStringLiteral("unknown");
    if (!measure.valid) {
        measure.confidence = 0.0;
        return measure;
    }
    if (context.hasCalibration() && context.calibration().valid) {
        measure = applyCalibrationIfAny(measure, context);
    } else {
        if (measure.unit.isEmpty())
            measure.unit = QStringLiteral("px");
        measure.calibrationId.clear();
        if (measure.message.isEmpty())
            measure.message = QStringLiteral("未标定：结果为像素单位");
        if (diagnosticCodeOut && diagnosticCodeOut->isEmpty())
            *diagnosticCodeOut = DiagnosticCodes::calibrationMissing();
    }
    return measure;
}

/// Angle / dimensionless measurements: never rewrite unit via length calibration.
inline MeasurementResult finalizeAngularMeasurement(MeasurementResult measure,
                                                    const ExecutionRequest &request)
{
    measure.sourceNodeId = request.nodeId;
    if (measure.decisionState.isEmpty())
        measure.decisionState = QStringLiteral("unknown");
    if (!measure.valid)
        measure.confidence = 0.0;
    if (measure.unit.isEmpty())
        measure.unit = QStringLiteral("deg");
    return measure;
}

inline double resolveRealInput(const ExecutionRequest &request, const QString &key, double fallback)
{
    if (request.typedParameters.contains(key) && !request.typedParameters.value(key).isNull())
        return request.typedParameters.value(key).toReal(fallback);
    if (request.inputs.contains(key) && !request.inputs.value(key).isNull())
        return request.inputs.value(key).toReal(fallback);
    return request.parameters.value(key).toDouble(fallback);
}

/// Like resolveRealInput, but Unset/Auto/0-sentinel uses autoValue instead of a hard fallback.
inline double resolveAutoReal(const ExecutionRequest &request,
                              const QString &key,
                              double userFallback,
                              double autoValue)
{
    if (request.typedParameters.contains(key) && !request.typedParameters.value(key).isNull()) {
        const double v = request.typedParameters.value(key).toReal(userFallback);
        if (paramBindingMode(request.parameters, key) == ParamBindingMode::Strict)
            return v;
        if (isAutoSentinelDouble(v))
            return autoValue;
        return v;
    }
    if (request.inputs.contains(key) && !request.inputs.value(key).isNull())
        return request.inputs.value(key).toReal(autoValue);
    return resolveAutoDouble(request.parameters, key, userFallback, autoValue);
}

inline int resolveIntParam(const ExecutionRequest &request, const QString &key, int fallback)
{
    if (request.typedParameters.contains(key) && !request.typedParameters.value(key).isNull())
        return request.typedParameters.value(key).toInt(fallback);
    return request.parameters.value(key).toInt(fallback);
}

/// Negative / Unset / Auto → autoValue; Strict keeps user value including 0.
inline int resolveAutoInt(const ExecutionRequest &request,
                          const QString &key,
                          int userFallback,
                          int autoValue)
{
    if (request.typedParameters.contains(key) && !request.typedParameters.value(key).isNull()) {
        const int v = request.typedParameters.value(key).toInt(userFallback);
        if (paramBindingMode(request.parameters, key) == ParamBindingMode::Strict)
            return v;
        if (isAutoSentinelInt(v))
            return autoValue;
        return v;
    }
    return Selt::resolveAutoInt(request.parameters, key, userFallback, autoValue);
}

inline bool resolveBoolParam(const ExecutionRequest &request, const QString &key, bool fallback)
{
    if (request.typedParameters.contains(key) && !request.typedParameters.value(key).isNull())
        return request.typedParameters.value(key).toBool(fallback);
    return request.parameters.value(key).toBool(fallback);
}

inline QString resolveStringParam(const ExecutionRequest &request, const QString &key,
                                  const QString &fallback = {})
{
    if (request.typedParameters.contains(key) && !request.typedParameters.value(key).isNull()) {
        const QString text = request.typedParameters.value(key).toString();
        return text.isEmpty() ? fallback : text;
    }
    const QString text = request.parameters.value(key).toString();
    return text.isEmpty() ? fallback : text;
}

inline QString resolveAutoStringParam(const ExecutionRequest &request,
                                      const QString &key,
                                      const QString &userFallback,
                                      const QString &autoValue)
{
    if (request.typedParameters.contains(key) && !request.typedParameters.value(key).isNull()) {
        const QString text = request.typedParameters.value(key).toString();
        if (text.isEmpty()
            || text.compare(QLatin1String("auto"), Qt::CaseInsensitive) == 0) {
            if (paramBindingMode(request.parameters, key) != ParamBindingMode::Strict)
                return autoValue;
        }
        return text.isEmpty() ? userFallback : text;
    }
    return resolveAutoString(request.parameters, key, userFallback, autoValue);
}

/// True when key is absent or explicitly Auto — callers should not treat as hard constraint.
inline bool isParamAutoOrUnset(const ExecutionRequest &request, const QString &key)
{
    const ParamBindingMode mode = paramBindingMode(request.parameters, key);
    return mode == ParamBindingMode::Auto || mode == ParamBindingMode::Unset;
}

inline ExecutionResult failResult(const QString &message, const QString &code, qint64 elapsedMs)
{
    ExecutionResult r;
    r.status = ModuleStatus::Failed;
    r.errorMessage = message;
    r.diagnosticCode = code;
    r.elapsedMs = elapsedMs;
    r.measurement.valid = false;
    r.measurement.confidence = 0.0;
    return r;
}

inline bool ensureOddKernel(int &ksize, QString *error = nullptr)
{
    if (ksize < 1) {
        if (error)
            *error = QStringLiteral("核大小必须为正奇数");
        return false;
    }
    if (ksize % 2 == 0)
        ++ksize;
    return true;
}

} // namespace Selt

#endif // IMAGEEXECUTORHELPERS_H
