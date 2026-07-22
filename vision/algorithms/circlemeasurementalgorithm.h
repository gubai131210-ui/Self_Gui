#ifndef CIRCLEMEASUREMENTALGORITHM_H
#define CIRCLEMEASUREMENTALGORITHM_H

#include "vision/algorithms/thresholdalgorithm.h"
#include "vision/model/measurementresult.h"
#include "vision/model/visionimage.h"

#include <QString>

struct CircleMeasureOptions
{
    double minRadius{5.0};
    bool drawOverlay{true};
    ThresholdOptions threshold;
};

class CircleMeasurementAlgorithm
{
public:
    static bool measure(const VisionImage &input,
                        MeasurementResult &result,
                        VisionImage &overlayImage,
                        double minRadius = 5.0,
                        bool drawOverlay = true,
                        QString *errorMessage = nullptr);

    static bool measure(const VisionImage &input,
                        MeasurementResult &result,
                        VisionImage &overlayImage,
                        CircleMeasureOptions &options,
                        QString *errorMessage = nullptr);
};

#endif // CIRCLEMEASUREMENTALGORITHM_H
