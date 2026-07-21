#ifndef GRAYSCALEALGORITHM_H
#define GRAYSCALEALGORITHM_H

#include "vision/model/visionimage.h"

#include <QString>

class GrayscaleAlgorithm
{
public:
    static bool convert(const VisionImage &input, VisionImage &output, QString *errorMessage = nullptr);
};

#endif // GRAYSCALEALGORITHM_H
