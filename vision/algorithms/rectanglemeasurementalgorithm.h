#ifndef RECTANGLEMEASUREMENTALGORITHM_H
#define RECTANGLEMEASUREMENTALGORITHM_H

#include "vision/algorithms/thresholdalgorithm.h"
#include "vision/model/measurementresult.h"
#include "vision/model/visionimage.h"

#include <QString>

struct RectangleMeasureOptions
{
    double minArea{100.0};
    bool drawOverlay{true};
    ThresholdOptions threshold;
};

class RectangleMeasurementAlgorithm
{
public:
    static bool measure(const VisionImage &input,
                        MeasurementResult &result,
                        VisionImage &overlayImage,
                        double minArea = 100.0,
                        bool drawOverlay = true,
                        QString *errorMessage = nullptr);

    static bool measure(const VisionImage &input,
                        MeasurementResult &result,
                        VisionImage &overlayImage,
                        RectangleMeasureOptions &options,
                        QString *errorMessage = nullptr);
};

#endif // RECTANGLEMEASUREMENTALGORITHM_H
