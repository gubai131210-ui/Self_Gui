#ifndef LINEMEASUREMENTALGORITHM_H
#define LINEMEASUREMENTALGORITHM_H

#include "vision/model/measurementresult.h"
#include "vision/model/visionimage.h"

#include <QString>

class LineMeasurementAlgorithm
{
public:
    static bool measure(const VisionImage &input,
                        MeasurementResult &result,
                        VisionImage &overlayImage,
                        int cannyLow = 50,
                        int cannyHigh = 150,
                        bool drawOverlay = true,
                        QString *errorMessage = nullptr);
};

#endif // LINEMEASUREMENTALGORITHM_H
