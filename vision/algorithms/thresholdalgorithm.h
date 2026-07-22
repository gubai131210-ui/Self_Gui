#ifndef THRESHOLDALGORITHM_H
#define THRESHOLDALGORITHM_H

#include "vision/model/visionimage.h"

#include <QString>

enum class ThresholdMode {
    Binary,
    BinaryInv
};

enum class ThresholdStrategy {
    Manual,
    Otsu,
    Adaptive
};

struct ThresholdOptions
{
    ThresholdStrategy strategy{ThresholdStrategy::Manual};
    int threshold{128};
    int maxValue{255};
    ThresholdMode mode{ThresholdMode::Binary};
    int adaptiveBlockSize{31}; // odd
    double adaptiveC{5.0};
    /// Actual threshold used (Manual = threshold; Otsu = computed; Adaptive = -1).
    int usedThreshold{-1};
    QString usedStrategy;
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

    static bool apply(const VisionImage &input,
                      VisionImage &output,
                      ThresholdOptions &options,
                      QString *errorMessage = nullptr);

    static ThresholdStrategy parseStrategy(const QString &name);
    static QString strategyName(ThresholdStrategy strategy);
};

#endif // THRESHOLDALGORITHM_H
