#ifndef PREPROCESSALGORITHMS_H
#define PREPROCESSALGORITHMS_H

#include "vision/model/visionimage.h"

#include <QString>

class GaussianBlurAlgorithm
{
public:
    static bool apply(const VisionImage &input,
                      VisionImage &out,
                      int ksizeX,
                      int ksizeY,
                      double sigmaX,
                      double sigmaY,
                      QString *error = nullptr);
};

class MedianBlurAlgorithm
{
public:
    static bool apply(const VisionImage &input,
                      VisionImage &out,
                      int ksize,
                      QString *error = nullptr);
};

class BilateralFilterAlgorithm
{
public:
    static bool apply(const VisionImage &input,
                      VisionImage &out,
                      int d,
                      double sigmaColor,
                      double sigmaSpace,
                      QString *error = nullptr);
};

class OtsuThresholdAlgorithm
{
public:
    static bool apply(const VisionImage &input,
                      VisionImage &out,
                      double maxValue,
                      bool invert,
                      QString *error = nullptr);
};

class AdaptiveThresholdAlgorithm
{
public:
    enum class Method {
        AdaptiveMean,
        AdaptiveGaussian
    };

    static bool apply(const VisionImage &input,
                      VisionImage &out,
                      double maxValue,
                      Method method,
                      int blockSize,
                      double C,
                      bool invert,
                      QString *error = nullptr);
};

class SobelAlgorithm
{
public:
    static bool apply(const VisionImage &input,
                      VisionImage &out,
                      int dx,
                      int dy,
                      int ksize,
                      double scale,
                      QString *error = nullptr);
};

class ColorConvertAlgorithm
{
public:
    /// mode: "gray" | "hsv" | "lab" | "bgr"
    static bool apply(const VisionImage &input,
                      VisionImage &out,
                      const QString &mode,
                      QString *error = nullptr);
};

class GeometricTransformAlgorithm
{
public:
    /// op: "rotate90" | "rotate180" | "rotate270" | "flipX" | "flipY" | "flipXY"
    static bool apply(const VisionImage &input,
                      VisionImage &out,
                      const QString &op,
                      QString *error = nullptr);
};

class GammaCorrectAlgorithm
{
public:
    static bool apply(const VisionImage &input,
                      VisionImage &out,
                      double gamma,
                      QString *error = nullptr);
};

class ContrastBrightnessAlgorithm
{
public:
    static bool apply(const VisionImage &input,
                      VisionImage &out,
                      double alpha,
                      double beta,
                      QString *error = nullptr);
};

class ClaheAlgorithm
{
public:
    static bool apply(const VisionImage &input,
                      VisionImage &out,
                      double clipLimit,
                      int tileSize,
                      QString *error = nullptr);
};

class SharpenAlgorithm
{
public:
    static bool apply(const VisionImage &input,
                      VisionImage &out,
                      double amount,
                      double sigma,
                      QString *error = nullptr);
};

#endif // PREPROCESSALGORITHMS_H
