#ifndef SUBPIXELALGORITHMS_H
#define SUBPIXELALGORITHMS_H

#include "vision/model/visionimage.h"

#include <QPointF>
#include <QString>

struct SubpixelOptions
{
    int windowSize{5};          // odd
    double terminateEps{0.01};
    int maxIterations{40};
    QString mode{QStringLiteral("corner")}; // corner | peak
};

class SubpixelRefineAlgorithm
{
public:
    /// Refine a coarse pixel location using local neighborhood fitting.
    /// mode=corner uses cornerSubPix; mode=peak uses 3x3 quadratic peak fit on gray patch.
    static bool refine(const VisionImage &image,
                       const QPointF &coarse,
                       QPointF &refined,
                       double &confidence,
                       const SubpixelOptions &options = {},
                       QString *error = nullptr);
};

#endif // SUBPIXELALGORITHMS_H
