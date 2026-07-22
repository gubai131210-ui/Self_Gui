#include "vision/algorithms/subpixelalgorithms.h"

#include "vision/algorithms/opencvguard.h"

#include <QtMath>
#include <cmath>
#include <opencv2/imgproc.hpp>
#include <vector>

namespace {

cv::Mat toGray8u(const cv::Mat &src)
{
    cv::Mat gray;
    if (src.channels() == 1)
        gray = src;
    else if (src.channels() == 3)
        cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    else
        cv::cvtColor(src, gray, cv::COLOR_BGRA2GRAY);
    if (gray.depth() != CV_8U)
        gray.convertTo(gray, CV_8U);
    return gray;
}

bool quadraticPeakFit(const cv::Mat &gray, const QPointF &coarse, QPointF *out)
{
    if (!out)
        return false;
    const int cx = int(std::lround(coarse.x()));
    const int cy = int(std::lround(coarse.y()));
    if (cx < 1 || cy < 1 || cx >= gray.cols - 1 || cy >= gray.rows - 1)
        return false;

    const double z00 = gray.at<uchar>(cy - 1, cx - 1);
    const double z10 = gray.at<uchar>(cy - 1, cx);
    const double z20 = gray.at<uchar>(cy - 1, cx + 1);
    const double z01 = gray.at<uchar>(cy, cx - 1);
    const double z11 = gray.at<uchar>(cy, cx);
    const double z21 = gray.at<uchar>(cy, cx + 1);
    const double z02 = gray.at<uchar>(cy + 1, cx - 1);
    const double z12 = gray.at<uchar>(cy + 1, cx);
    const double z22 = gray.at<uchar>(cy + 1, cx + 1);

    const double dx = 0.5 * (z21 - z01);
    const double dy = 0.5 * (z12 - z10);
    const double dxx = z21 - 2.0 * z11 + z01;
    const double dyy = z12 - 2.0 * z11 + z10;
    const double dxy = 0.25 * (z22 - z20 - z02 + z00);
    const double det = dxx * dyy - dxy * dxy;
    if (std::abs(det) < 1e-6)
        return false;
    const double ox = -(dyy * dx - dxy * dy) / det;
    const double oy = -(dxx * dy - dxy * dx) / det;
    if (std::abs(ox) > 1.0 || std::abs(oy) > 1.0)
        return false;
    *out = QPointF(cx + ox, cy + oy);
    return std::isfinite(out->x()) && std::isfinite(out->y());
}

} // namespace

bool SubpixelRefineAlgorithm::refine(const VisionImage &image,
                                     const QPointF &coarse,
                                     QPointF &refined,
                                     double &confidence,
                                     const SubpixelOptions &options,
                                     QString *error)
{
    refined = coarse;
    confidence = 0.0;
    if (image.isEmpty()) {
        if (error)
            *error = QStringLiteral("亚像素输入图像为空");
        return false;
    }
    if (!(std::isfinite(coarse.x()) && std::isfinite(coarse.y()))) {
        if (error)
            *error = QStringLiteral("粗定位点无效");
        return false;
    }

    int win = options.windowSize;
    if (win % 2 == 0)
        ++win;
    win = qBound(3, win, 31);

    QString cvError;
    bool ok = false;
    const bool ran = Selt::runOpenCv([&]() {
        const cv::Mat gray = toGray8u(image.mat());
        if (coarse.x() < 1 || coarse.y() < 1
            || coarse.x() >= gray.cols - 1 || coarse.y() >= gray.rows - 1) {
            cvError = QStringLiteral("粗定位点越界");
            return;
        }

        if (options.mode == QLatin1String("peak")) {
            QPointF peak;
            if (!quadraticPeakFit(gray, coarse, &peak)) {
                cvError = QStringLiteral("峰值二次拟合失败");
                return;
            }
            refined = peak;
            confidence = 0.85;
            ok = true;
            return;
        }

        std::vector<cv::Point2f> pts{cv::Point2f(float(coarse.x()), float(coarse.y()))};
        cv::cornerSubPix(gray, pts,
                         cv::Size(win / 2, win / 2),
                         cv::Size(-1, -1),
                         cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::COUNT,
                                          options.maxIterations, options.terminateEps));
        if (pts.empty()) {
            cvError = QStringLiteral("cornerSubPix 无输出");
            return;
        }
        refined = QPointF(pts.front().x, pts.front().y);
        const double shift = std::hypot(refined.x() - coarse.x(), refined.y() - coarse.y());
        confidence = qBound(0.0, 1.0 - shift / qMax(1.0, win * 0.5), 1.0);
        ok = std::isfinite(refined.x()) && std::isfinite(refined.y());
        if (!ok)
            cvError = QStringLiteral("亚像素结果无效");
    }, error);

    if (!ran)
        return false;
    if (!ok) {
        if (error)
            *error = cvError.isEmpty() ? QStringLiteral("亚像素细化失败") : cvError;
        refined = coarse;
        confidence = 0.0;
        return false;
    }
    return true;
}
