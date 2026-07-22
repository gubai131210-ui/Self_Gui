#ifndef CALIPERALGORITHM_H
#define CALIPERALGORITHM_H

#include "vision/model/visionimage.h"

#include <QPointF>
#include <QString>
#include <QVector>

struct CaliperOptions
{
    double sampleStep{1.0};          // px along caliper axis
    double smoothSigma{0.0};         // Gaussian on profile; 0 = off
    QString polarity{QStringLiteral("any")}; // any | dark_to_light | light_to_dark
    double gradientThreshold{1.0};
    double minEdgeGap{4.0};          // px between dual edges
    double secondPeakRatio{0.35};
};

struct FitOptions
{
    QString mode{QStringLiteral("algebraic")}; // algebraic | ransac
    double residualThreshold{2.0};
    int maxIterations{100};
    double minInlierRatio{0.6};
    int minInliers{3};
};

struct FitCircleResult
{
    QPointF center;
    double radius{0.0};
    double residualRms{0.0};
    int inlierCount{0};
    double confidence{0.0};
};

struct FitLineResult
{
    QPointF start;
    QPointF end;
    double angleDeg{0.0};
    double residualRms{0.0};
    int inlierCount{0};
    double confidence{0.0};
};

class CaliperAlgorithm
{
public:
    /// Sample edge along a caliper strip centered at `center`, oriented by `angleDeg`.
    /// On success with two edges, `distance` is the gap; with one edge, distance stays 0.
    static bool sample(const VisionImage &input,
                       const QPointF &center,
                       double angleDeg,
                       double length,
                       double width,
                       QVector<QPointF> &edgePoints,
                       double &distance,
                       VisionImage &overlay,
                       QString *error = nullptr);

    static bool sample(const VisionImage &input,
                       const QPointF &center,
                       double angleDeg,
                       double length,
                       double width,
                       const CaliperOptions &options,
                       QVector<QPointF> &edgePoints,
                       double &distance,
                       double &confidence,
                       VisionImage &overlay,
                       QString *error = nullptr);
};

class FitCircleAlgorithm
{
public:
    static bool fit(const QVector<QPointF> &points,
                    QPointF &center,
                    double &radius,
                    double &residual,
                    QString *error = nullptr);

    static bool fit(const QVector<QPointF> &points,
                    const FitOptions &options,
                    FitCircleResult &result,
                    QString *error = nullptr);
};

class FitLineAlgorithm
{
public:
    static bool fit(const QVector<QPointF> &points,
                    QPointF &start,
                    QPointF &end,
                    double &angleDeg,
                    QString *error = nullptr);

    static bool fit(const QVector<QPointF> &points,
                    const FitOptions &options,
                    FitLineResult &result,
                    QString *error = nullptr);
};

class AngleMeasureAlgorithm
{
public:
    static bool fromLines(const QPointF &l1Start,
                          const QPointF &l1End,
                          const QPointF &l2Start,
                          const QPointF &l2End,
                          double &angleDeg,
                          QString *error = nullptr);
};

class ParallelDistanceAlgorithm
{
public:
    /// Distance from midpoint of line2 to the infinite line through line1.
    static bool fromLines(const QPointF &l1Start,
                          const QPointF &l1End,
                          const QPointF &l2Start,
                          const QPointF &l2End,
                          double &distance,
                          QString *error = nullptr);
};

class PerpendicularityAlgorithm
{
public:
    /// Absolute deviation from 90 degrees between two lines.
    static bool fromLines(const QPointF &l1Start,
                          const QPointF &l1End,
                          const QPointF &l2Start,
                          const QPointF &l2End,
                          double &angleDeg,
                          double &deviationDeg,
                          QString *error = nullptr);
};

class PositionDeviationAlgorithm
{
public:
    static bool fromPoints(const QPointF &actual,
                           const QPointF &reference,
                           double &dx,
                           double &dy,
                           double &distance,
                           QString *error = nullptr);
};

#endif // CALIPERALGORITHM_H
