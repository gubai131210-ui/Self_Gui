#ifndef IMAGEFILTERALGORITHMS_H
#define IMAGEFILTERALGORITHMS_H

#include "vision/model/visionimage.h"

#include <QString>

class BlurAlgorithm
{
public:
    static bool apply(const VisionImage &input, VisionImage &output, int ksize, QString *error = nullptr);
};

class CannyAlgorithm
{
public:
    static bool apply(const VisionImage &input, VisionImage &output,
                      double threshold1, double threshold2, QString *error = nullptr,
                      int apertureSize = 3);
};

class MorphologyAlgorithm
{
public:
    enum class Op { Erode, Dilate, Open, Close };
    static bool apply(const VisionImage &input, VisionImage &output, Op op, int ksize,
                      QString *error = nullptr, int iterations = 1);
};

class TemplateMatchAlgorithm
{
public:
    static bool match(const VisionImage &image, const VisionImage &templ,
                      QPointF *peak, double *score, QString *error = nullptr);
};

class ResizeAlgorithm
{
public:
    static bool apply(const VisionImage &input, VisionImage &output,
                      int width, int height, QString *error = nullptr);
};

#endif
