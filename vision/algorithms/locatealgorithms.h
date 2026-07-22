#ifndef LOCATEALGORITHMS_H
#define LOCATEALGORITHMS_H

#include "vision/model/visionimage.h"

#include <QPointF>
#include <QString>
#include <QVector>

struct LocateLine2D
{
    QPointF start;
    QPointF end;
    double confidence{1.0};
};

struct LocateCircle2D
{
    QPointF center;
    double radius{0.0};
    double confidence{1.0};
};

struct HoughLinesOptions
{
    double cannyLow{50.0};
    double cannyHigh{150.0};
    int accumulatorThreshold{50};
    double minLineLength{30.0};
    double maxLineGap{10.0};
    int maxCount{50};
    QString sortBy{QStringLiteral("length")}; // length | score
};

struct HoughCirclesOptions
{
    double minRadius{5.0};
    double maxRadius{0.0};
    double dp{1.2};
    double minDist{0.0}; // 0 => rows/8
    double param1{100.0};
    double param2{30.0};
    int maxCount{50};
};

struct FeatureMatchOptions
{
    double loweRatio{0.75};
    int minMatches{8};
    double ransacReprojThreshold{3.0};
    double minInlierRatio{0.3};
};

struct TemplateMatchMultiOptions
{
    double minScore{0.6};
    int maxCount{5};
    double nmsIoU{0.3};
    double nmsCenterDistance{0.0}; // 0 => half template diagonal
    double scaleMin{1.0};
    double scaleMax{1.0};
    double scaleStep{0.1};
    double angleMin{0.0};
    double angleMax{0.0};
    double angleStep{15.0};
};

class HoughLinesAlgorithm
{
public:
    static bool apply(const VisionImage &input,
                      QVector<LocateLine2D> &lines,
                      double cannyLow,
                      double cannyHigh,
                      VisionImage &overlay,
                      QString *error = nullptr);

    static bool apply(const VisionImage &input,
                      QVector<LocateLine2D> &lines,
                      const HoughLinesOptions &options,
                      VisionImage &overlay,
                      QString *error = nullptr);
};

class HoughCirclesAlgorithm
{
public:
    static bool apply(const VisionImage &input,
                      QVector<LocateCircle2D> &circles,
                      double minRadius,
                      double maxRadius,
                      VisionImage &overlay,
                      QString *error = nullptr);

    static bool apply(const VisionImage &input,
                      QVector<LocateCircle2D> &circles,
                      const HoughCirclesOptions &options,
                      VisionImage &overlay,
                      QString *error = nullptr);
};

class FeatureMatchAlgorithm
{
public:
    /// ORB feature matching; matchPoint is the estimated template location in image.
    static bool apply(const VisionImage &image,
                      const VisionImage &templ,
                      QPointF &matchPoint,
                      double &score,
                      VisionImage &overlay,
                      QString *error = nullptr);

    static bool apply(const VisionImage &image,
                      const VisionImage &templ,
                      const FeatureMatchOptions &options,
                      QPointF &matchPoint,
                      double &score,
                      int &inlierCount,
                      VisionImage &overlay,
                      QString *error = nullptr);
};

class TemplateMatchMultiAlgorithm
{
public:
    /// Multi-peak template matching with simple NMS.
    static bool apply(const VisionImage &image,
                      const VisionImage &templ,
                      double minScore,
                      int maxCount,
                      QVector<QPointF> &peaks,
                      QVector<double> &scores,
                      VisionImage &overlay,
                      QString *error = nullptr);

    static bool apply(const VisionImage &image,
                      const VisionImage &templ,
                      const TemplateMatchMultiOptions &options,
                      QVector<QPointF> &peaks,
                      QVector<double> &scores,
                      VisionImage &overlay,
                      QString *error = nullptr,
                      QVector<double> *outScales = nullptr,
                      QVector<double> *outAngles = nullptr);
};

#endif // LOCATEALGORITHMS_H
