#ifndef THRESHOLDALGORITHM_H
#define THRESHOLDALGORITHM_H

#include "vision/model/visionimage.h"

#include <QString>

enum class ThresholdMode {
    Binary,
    BinaryInv
};

class ThresholdAlgorithm
{
public:
    static bool apply(const VisionImage &input,
                      VisionImage &output,
                      int threshold = 128,
                      int maxValue = 255,
                      ThresholdMode mode = ThresholdMode::Binary,
                      QString *errorMessage = nullptr);
};

#endif // THRESHOLDALGORITHM_H
