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
        double minArea{50.0};
        double maxArea{1e18};
        double minCircularity{0.0};
        double minAspectRatio{0.0};
        double maxAspectRatio{1e9};
        double minExtent{0.0};      // rectangularity / extent
        double minSolidity{0.0};
        int maxCount{100};
        /// area | circularity | x | y
        QString sortBy{QStringLiteral("area")};
        /// 选出写入主输出(center/area/confidence)的目标索引；-1 表示取排序后第一个。
        int selectIndex{0};
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
