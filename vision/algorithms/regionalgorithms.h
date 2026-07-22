#ifndef REGIONALGORITHMS_H
#define REGIONALGORITHMS_H

#include "vision/model/regiondata.h"
#include "vision/model/visionimage.h"

#include <QPointF>
#include <QString>
#include <QVector>

class MaskCombineAlgorithm
{
public:
    /// op: "and" | "or" | "xor" | "not"  — for "not", only image a is used
    static bool apply(const VisionImage &a,
                      const VisionImage &b,
                      VisionImage &out,
                      const QString &op,
                      QString *error = nullptr);
};

class FindContoursAlgorithm
{
public:
    static bool apply(const VisionImage &input,
                      QVector<QVector<QPointF>> &contours,
                      VisionImage &overlay,
                      QString *error = nullptr);
};

class ConnectedComponentsAlgorithm
{
public:
    static bool apply(const VisionImage &input,
                      VisionImage &labelImage,
                      int &count,
                      QVector<QPointF> &centroids,
                      QVector<double> &areas,
                      QString *error = nullptr);
};

class BlobAnalyzeAlgorithm
{
public:
    static bool apply(const VisionImage &input,
                      double minArea,
                      double maxArea,
                      double minCircularity,
                      QVector<QPointF> &outCenters,
                      QVector<double> &outAreas,
                      VisionImage &overlay,
                      QString *error = nullptr);

    struct Options {
        double minArea{0.0};
        double maxArea{0.0}; // 0 = unlimited
        double minCircularity{0.0};
        double maxCircularity{1.0};
        double minAspectRatio{0.0};
        double maxAspectRatio{1e9};
        double minExtent{0.0};      // rectangularity / extent
        double minSolidity{0.0};
        double minPerimeter{0.0};
        double maxPerimeter{0.0}; // 0 = unlimited
        double minElongation{1.0};
        double maxElongation{0.0}; // 0 = unlimited
        double minCx{-1e9};
        double maxCx{1e9};
        double minCy{-1e9};
        double maxCy{1e9};
        double minWidth{0.0};
        double maxWidth{0.0}; // 0 = unlimited
        double minHeight{0.0};
        double maxHeight{0.0}; // 0 = unlimited
        int maxCount{100};
        /// area | circularity | x | y | elongation | perimeter
        QString sortBy{QStringLiteral("area")};
        /// 选出写入主输出(center/area/confidence)的目标索引；-1 表示取排序后第一个。
        int selectIndex{0};
        /// auto | otsu | manual | relative
        QString thresholdMode{QStringLiteral("auto")};
        /// dark | light | any  — object polarity relative to background
        QString polarity{QStringLiteral("dark")};
        int manualThreshold{128};
        /// Remap output centers/boxes into source image coordinates.
        QPointF originOffset{0.0, 0.0};
        /// Optional ROI mask in the same coordinate space as `input` (non-zero = valid).
        VisionImage validMask;
    };

    static bool apply(const VisionImage &input,
                      const Options &options,
                      QVector<QPointF> &outCenters,
                      QVector<double> &outAreas,
                      QVector<double> &outScores,
                      VisionImage &overlay,
                      QString *error = nullptr);

    static bool apply(const VisionImage &input,
                      const Options &options,
                      RegionData &outRegion,
                      QVector<double> &outScores,
                      VisionImage &overlay,
                      int *selectedIndex,
                      QString *error = nullptr);

    /// Same as apply, but also returns unfiltered candidate count before range filters.
    static bool apply(const VisionImage &input,
                      const Options &options,
                      RegionData &outRegion,
                      QVector<double> &outScores,
                      VisionImage &overlay,
                      int *selectedIndex,
                      int *unfilteredCount,
                      QString *error = nullptr);
};

class RegionFillAlgorithm
{
public:
    /// Fill external contours on a binary/gray image (holes filled).
    static bool apply(const VisionImage &input,
                      VisionImage &out,
                      VisionImage &overlay,
                      QString *error = nullptr);
};

#endif // REGIONALGORITHMS_H
