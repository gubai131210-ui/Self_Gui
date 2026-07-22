#ifndef ROIAPPLIER_H
#define ROIAPPLIER_H

#include "vision/model/extendedroi.h"
#include "vision/model/visionimage.h"

#include <QJsonObject>
#include <QPointF>
#include <QString>

namespace Selt {

enum class RoiApplyMode {
    MaskOutside, // keep full image size; outside ROI becomes black
    Crop,        // crop to ROI bounding box (rectangle only semantics for shape bbox)
    CropMasked   // crop to bbox then keep only ROI shape pixels (preferred for Blob)
};

struct RoiApplyResult
{
    bool ok{false};
    VisionImage image;
    QString errorMessage;
    QString diagnosticCode;
    ExtendedRoi appliedRoi;
    /// Top-left of cropped region in source image coordinates (0,0 for MaskOutside).
    QPointF originOffset;
};

class RoiApplier
{
public:
    /// Prefer extendedRoi JSON; fall back to legacy roi JSON.
    static ExtendedRoi parseFromParameters(const QJsonObject &params);

    /// Prefer upstream `roi` parameter when `preferParameterRoi` is set, else extendedRoi first.
    static ExtendedRoi parseFromParameters(const QJsonObject &params, bool preferParameterRoi);

    static RoiApplyResult apply(const VisionImage &input,
                                const ExtendedRoi &roi,
                                RoiApplyMode mode = RoiApplyMode::MaskOutside);

    static RoiApplyResult applyFromParameters(const VisionImage &input,
                                              const QJsonObject &params,
                                              RoiApplyMode mode = RoiApplyMode::MaskOutside);
};

} // namespace Selt

#endif // ROIAPPLIER_H
