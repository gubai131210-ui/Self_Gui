#ifndef ROIAPPLIER_H
#define ROIAPPLIER_H

#include "vision/model/extendedroi.h"
#include "vision/model/visionimage.h"

#include <QJsonObject>
#include <QString>

namespace Selt {

enum class RoiApplyMode {
    MaskOutside, // keep full image size; outside ROI becomes black
    Crop         // crop to ROI bounding box
};

struct RoiApplyResult
{
    bool ok{false};
    VisionImage image;
    QString errorMessage;
    QString diagnosticCode;
    ExtendedRoi appliedRoi;
};

class RoiApplier
{
public:
    /// Prefer extendedRoi JSON; fall back to legacy roi JSON.
    static ExtendedRoi parseFromParameters(const QJsonObject &params);

    static RoiApplyResult apply(const VisionImage &input,
                                const ExtendedRoi &roi,
                                RoiApplyMode mode = RoiApplyMode::MaskOutside);

    static RoiApplyResult applyFromParameters(const VisionImage &input,
                                              const QJsonObject &params,
                                              RoiApplyMode mode = RoiApplyMode::MaskOutside);
};

} // namespace Selt

#endif // ROIAPPLIER_H
