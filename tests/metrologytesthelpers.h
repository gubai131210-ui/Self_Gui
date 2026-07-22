#ifndef METROLOGYTESTHELPERS_H
#define METROLOGYTESTHELPERS_H

#include "vision/model/visionimage.h"

#include <QPointF>
#include <QString>
#include <QVector>
#include <cmath>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <random>

namespace Selt {
namespace MetrologyTest {

struct CircleGroundTruth {
    QPointF center{60.0, 60.0};
    double radius{25.0};
};

struct BarGroundTruth {
    QPointF center{65.0, 50.0};
    double widthPx{70.0};
    double heightPx{60.0};
    double angleDeg{0.0};
};

struct LineGroundTruth {
    QPointF start{10.0, 40.0};
    QPointF end{110.0, 40.0};
};

inline VisionImage makeFilledCircle(int size, const CircleGroundTruth &gt)
{
    cv::Mat img(size, size, CV_8UC1, cv::Scalar(0));
    cv::circle(img,
               cv::Point(int(std::lround(gt.center.x())), int(std::lround(gt.center.y()))),
               int(std::lround(gt.radius)), cv::Scalar(255), cv::FILLED);
    return VisionImage(img);
}

inline VisionImage makeVerticalBar(int rows, int cols, const BarGroundTruth &gt)
{
    cv::Mat img(rows, cols, CV_8UC1, cv::Scalar(10));
    const int x0 = int(std::lround(gt.center.x() - gt.widthPx * 0.5));
    const int y0 = int(std::lround(gt.center.y() - gt.heightPx * 0.5));
    cv::rectangle(img,
                  cv::Rect(x0, y0, int(std::lround(gt.widthPx)), int(std::lround(gt.heightPx))),
                  cv::Scalar(240), cv::FILLED);
    return VisionImage(img);
}

inline VisionImage makeHorizontalLine(int rows, int cols, const LineGroundTruth &gt, int thickness = 2)
{
    cv::Mat img(rows, cols, CV_8UC1, cv::Scalar(0));
    cv::line(img,
             cv::Point(int(std::lround(gt.start.x())), int(std::lround(gt.start.y()))),
             cv::Point(int(std::lround(gt.end.x())), int(std::lround(gt.end.y()))),
             cv::Scalar(255), thickness);
    return VisionImage(img);
}

inline VisionImage makeRotatedRectangle(int size, const QPointF &center, double w, double h, double angleDeg)
{
    cv::Mat img(size, size, CV_8UC1, cv::Scalar(0));
    cv::RotatedRect rr(cv::Point2f(float(center.x()), float(center.y())),
                       cv::Size2f(float(w), float(h)), float(angleDeg));
    cv::Point2f pts[4];
    rr.points(pts);
    std::vector<cv::Point> poly;
    for (const cv::Point2f &p : pts)
        poly.emplace_back(cv::Point(cvRound(p.x), cvRound(p.y)));
    cv::fillConvexPoly(img, poly, cv::Scalar(255));
    return VisionImage(img);
}

inline void addGaussianNoise(cv::Mat &mat, double sigma, unsigned seed = 42)
{
    if (mat.empty() || !(sigma > 0.0))
        return;
    cv::Mat noise(mat.size(), CV_32F);
    cv::randn(noise, 0.0, sigma);
    cv::Mat work;
    mat.convertTo(work, CV_32F);
    work += noise;
    work.convertTo(mat, mat.type());
}

inline void addSaltPepperNoise(cv::Mat &mat, double amount, unsigned seed = 7)
{
    if (mat.empty() || !(amount > 0.0))
        return;
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> uni(0.0, 1.0);
    for (int y = 0; y < mat.rows; ++y) {
        uchar *row = mat.ptr<uchar>(y);
        for (int x = 0; x < mat.cols; ++x) {
            const double r = uni(rng);
            if (r < amount * 0.5)
                row[x] = 0;
            else if (r < amount)
                row[x] = 255;
        }
    }
}

inline void applyBrightnessOffset(cv::Mat &mat, int offset)
{
    mat.convertTo(mat, -1, 1.0, offset);
}

inline void applyLowContrast(cv::Mat &mat, double alpha = 0.35, double beta = 80.0)
{
    mat.convertTo(mat, -1, alpha, beta);
}

inline VisionImage withGaussianNoise(const VisionImage &src, double sigma, unsigned seed = 42)
{
    cv::Mat mat = src.mat().clone();
    addGaussianNoise(mat, sigma, seed);
    return VisionImage(mat, src.sourcePath());
}

inline VisionImage scaledCopy(const VisionImage &src, int targetSize)
{
    cv::Mat out;
    cv::resize(src.mat(), out, cv::Size(targetSize, targetSize), 0, 0, cv::INTER_NEAREST);
    return VisionImage(out, src.sourcePath());
}

inline double absError(double measured, double expected)
{
    return std::abs(measured - expected);
}

inline double relativeError(double measured, double expected)
{
    if (std::abs(expected) < 1e-12)
        return absError(measured, expected);
    return absError(measured, expected) / std::abs(expected);
}

struct RepeatStats {
    double mean{0.0};
    double stddev{0.0};
    int count{0};
};

inline RepeatStats computeRepeatStats(const QVector<double> &values)
{
    RepeatStats s;
    s.count = values.size();
    if (values.isEmpty())
        return s;
    double sum = 0.0;
    for (double v : values)
        sum += v;
    s.mean = sum / values.size();
    double var = 0.0;
    for (double v : values) {
        const double d = v - s.mean;
        var += d * d;
    }
    s.stddev = std::sqrt(var / values.size());
    return s;
}

} // namespace MetrologyTest
} // namespace Selt

#endif // METROLOGYTESTHELPERS_H
