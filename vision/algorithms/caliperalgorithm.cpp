#include "vision/algorithms/caliperalgorithm.h"

#include "vision/algorithms/opencvguard.h"

#include <QLineF>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <opencv2/imgproc.hpp>
#include <random>
#include <vector>

namespace {

constexpr double kMinLineLength = 1e-3;
constexpr double kCollinearTolerancePx = 1.0;

cv::Mat toGray(const cv::Mat &src)
{
    cv::Mat gray;
    if (src.channels() == 1)
        gray = src;
    else if (src.channels() == 3)
        cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    else
        cv::cvtColor(src, gray, cv::COLOR_BGRA2GRAY);
    return gray;
}

double sampleBilinear(const cv::Mat &gray, double x, double y)
{
    if (x < 0.0 || y < 0.0 || x >= gray.cols - 1 || y >= gray.rows - 1)
        return 0.0;
    const int x0 = int(std::floor(x));
    const int y0 = int(std::floor(y));
    const double dx = x - x0;
    const double dy = y - y0;
    const double v00 = gray.at<uchar>(y0, x0);
    const double v10 = gray.at<uchar>(y0, x0 + 1);
    const double v01 = gray.at<uchar>(y0 + 1, x0);
    const double v11 = gray.at<uchar>(y0 + 1, x0 + 1);
    return (1.0 - dx) * (1.0 - dy) * v00 + dx * (1.0 - dy) * v10
        + (1.0 - dx) * dy * v01 + dx * dy * v11;
}

bool algebraicCircleFit(const std::vector<cv::Point2f> &pts, cv::Point2f *center, float *radius)
{
    if (!center || !radius || pts.size() < 3)
        return false;

    cv::Mat A(static_cast<int>(pts.size()), 3, CV_64F);
    cv::Mat b(static_cast<int>(pts.size()), 1, CV_64F);
    for (int i = 0; i < static_cast<int>(pts.size()); ++i) {
        const double x = pts[static_cast<size_t>(i)].x;
        const double y = pts[static_cast<size_t>(i)].y;
        A.at<double>(i, 0) = x;
        A.at<double>(i, 1) = y;
        A.at<double>(i, 2) = 1.0;
        b.at<double>(i, 0) = -(x * x + y * y);
    }
    cv::Mat sol;
    if (!cv::solve(A, b, sol, cv::DECOMP_SVD))
        return false;

    const double D = sol.at<double>(0, 0);
    const double E = sol.at<double>(1, 0);
    const double F = sol.at<double>(2, 0);
    const double cx = -D * 0.5;
    const double cy = -E * 0.5;
    const double r2 = cx * cx + cy * cy - F;
    if (!(r2 > 0.0) || !std::isfinite(cx) || !std::isfinite(cy) || !std::isfinite(r2))
        return false;

    *center = cv::Point2f(static_cast<float>(cx), static_cast<float>(cy));
    *radius = static_cast<float>(std::sqrt(r2));
    return *radius > 0.0f;
}

bool pointsNearlyCollinear(const QVector<QPointF> &points, double tolerancePx)
{
    if (points.size() < 3)
        return false;

    int baseI = 0;
    int baseJ = 1;
    double bestSpan = 0.0;
    for (int i = 0; i < points.size(); ++i) {
        for (int j = i + 1; j < points.size(); ++j) {
            const double span = QLineF(points.at(i), points.at(j)).length();
            if (span > bestSpan) {
                bestSpan = span;
                baseI = i;
                baseJ = j;
            }
        }
    }
    if (bestSpan < kMinLineLength)
        return true;

    const QPointF a = points.at(baseI);
    const QPointF b = points.at(baseJ);
    const QPointF ab = b - a;
    for (const QPointF &p : points) {
        const double cross = (p.x() - a.x()) * ab.y() - (p.y() - a.y()) * ab.x();
        if (std::abs(cross) / bestSpan > tolerancePx)
            return false;
    }
    return true;
}

double circleResidualRms(const QVector<QPointF> &points, const QPointF &center, double radius)
{
    if (points.isEmpty())
        return 0.0;
    double sumSq = 0.0;
    for (const QPointF &p : points) {
        const double d = QLineF(center, p).length() - radius;
        sumSq += d * d;
    }
    return std::sqrt(sumSq / points.size());
}

double refinePeakIndex(const QVector<double> &profile, int idx)
{
    if (idx <= 0 || idx >= profile.size() - 1)
        return double(idx);
    const double y0 = profile.at(idx - 1);
    const double y1 = profile.at(idx);
    const double y2 = profile.at(idx + 1);
    const double denom = (y0 - 2.0 * y1 + y2);
    if (std::abs(denom) < 1e-9)
        return double(idx);
    const double delta = 0.5 * (y0 - y2) / denom;
    return double(idx) + qBound(-1.0, delta, 1.0);
}

void smoothProfile(QVector<double> &profile, double sigma)
{
    if (!(sigma > 0.0) || profile.size() < 3)
        return;
    const int radius = qMax(1, int(std::ceil(sigma * 3.0)));
    QVector<double> kernel(2 * radius + 1);
    double sum = 0.0;
    for (int i = -radius; i <= radius; ++i) {
        const double v = std::exp(-(i * i) / (2.0 * sigma * sigma));
        kernel[i + radius] = v;
        sum += v;
    }
    for (double &k : kernel)
        k /= sum;
    QVector<double> out(profile.size(), 0.0);
    for (int i = 0; i < profile.size(); ++i) {
        double acc = 0.0;
        for (int k = -radius; k <= radius; ++k) {
            const int j = qBound(0, i + k, profile.size() - 1);
            acc += profile.at(j) * kernel.at(k + radius);
        }
        out[i] = acc;
    }
    profile = out;
}

bool fitLineL2(const QVector<QPointF> &points, FitLineResult *result, QString *cvError)
{
    if (!result)
        return false;
    std::vector<cv::Point2f> pts;
    pts.reserve(size_t(points.size()));
    for (const QPointF &p : points)
        pts.emplace_back(float(p.x()), float(p.y()));
    cv::Vec4f line;
    cv::fitLine(pts, line, cv::DIST_L2, 0, 0.01, 0.01);
    const QPointF dir(line[0], line[1]);
    const QPointF origin(line[2], line[3]);
    const double dirLen = std::hypot(dir.x(), dir.y());
    if (dirLen < 1e-9) {
        if (cvError)
            *cvError = QStringLiteral("拟合直线方向退化");
        return false;
    }
    double minT = 0.0;
    double maxT = 0.0;
    bool first = true;
    double sumSq = 0.0;
    for (const QPointF &p : points) {
        const double t = QPointF::dotProduct(p - origin, dir);
        if (first) {
            minT = maxT = t;
            first = false;
        } else {
            minT = qMin(minT, t);
            maxT = qMax(maxT, t);
        }
        const QPointF proj = origin + dir * t;
        const double d = QLineF(p, proj).length();
        sumSq += d * d;
    }
    result->start = origin + dir * minT;
    result->end = origin + dir * maxT;
    if (QLineF(result->start, result->end).length() < kMinLineLength) {
        if (cvError)
            *cvError = QStringLiteral("拟合直线长度退化");
        return false;
    }
    result->angleDeg = std::atan2(dir.y(), dir.x()) * 180.0 / CV_PI;
    result->residualRms = std::sqrt(sumSq / points.size());
    result->inlierCount = points.size();
    result->confidence = qBound(0.0, 1.0, 1.0 - result->residualRms / 5.0);
    return true;
}

} // namespace

bool CaliperAlgorithm::sample(const VisionImage &input,
                              const QPointF &center,
                              double angleDeg,
                              double length,
                              double width,
                              QVector<QPointF> &edgePoints,
                              double &distance,
                              VisionImage &overlay,
                              QString *error)
{
    double confidence = 0.0;
    return sample(input, center, angleDeg, length, width, CaliperOptions{},
                  edgePoints, distance, confidence, overlay, error);
}

bool CaliperAlgorithm::sample(const VisionImage &input,
                              const QPointF &center,
                              double angleDeg,
                              double length,
                              double width,
                              const CaliperOptions &options,
                              QVector<QPointF> &edgePoints,
                              double &distance,
                              double &confidence,
                              VisionImage &overlay,
                              QString *error)
{
    edgePoints.clear();
    distance = 0.0;
    confidence = 0.0;
    if (input.isEmpty()) {
        if (error)
            *error = QStringLiteral("卡尺输入图像为空");
        return false;
    }
    if (!(std::isfinite(center.x()) && std::isfinite(center.y()))) {
        if (error)
            *error = QStringLiteral("卡尺中心无效");
        return false;
    }
    if (length < 2.0 || width < 1.0) {
        if (error)
            *error = QStringLiteral("卡尺长度/宽度无效");
        return false;
    }

    const double step = qMax(0.25, options.sampleStep);
    const QString polarity = options.polarity.toLower();

    QString cvError;
    bool ok = false;
    const bool ran = Selt::runOpenCv([&]() {
        const cv::Mat gray = toGray(input.mat());
        const double rad = angleDeg * CV_PI / 180.0;
        const double c = std::cos(rad);
        const double s = std::sin(rad);
        const int halfLen = qMax(1, int(std::lround(length * 0.5)));
        const int samples = qMax(8, int(std::lround(length / step)) + 1);
        QVector<double> profile;
        QVector<double> signedProfile;
        profile.reserve(samples);
        signedProfile.reserve(samples);
        QVector<QPointF> samplePts;
        samplePts.reserve(samples);

        for (int i = 0; i < samples; ++i) {
            const double t = -halfLen + (length * i) / double(samples - 1);
            const double x = center.x() + t * c;
            const double y = center.y() + t * s;
            samplePts.append(QPointF(x, y));
            double sumAbs = 0.0;
            double sumSigned = 0.0;
            int count = 0;
            const int halfW = qMax(0, int(std::lround(width * 0.5)));
            for (int w = -halfW; w <= halfW; ++w) {
                const double sx = x - w * s;
                const double sy = y + w * c;
                if (sx < 1.0 || sy < 1.0 || sx >= gray.cols - 1 || sy >= gray.rows - 1)
                    continue;
                const double gx = sampleBilinear(gray, sx + 1.0, sy) - sampleBilinear(gray, sx - 1.0, sy);
                const double gy = sampleBilinear(gray, sx, sy + 1.0) - sampleBilinear(gray, sx, sy - 1.0);
                const double proj = gx * c + gy * s;
                sumAbs += std::abs(proj);
                sumSigned += proj;
                ++count;
            }
            profile.append(count > 0 ? sumAbs / count : 0.0);
            signedProfile.append(count > 0 ? sumSigned / count : 0.0);
        }

        smoothProfile(profile, options.smoothSigma);

        struct Peak {
            int index{0};
            double mag{0.0};
        };
        QVector<Peak> peaks;
        for (int i = 1; i < profile.size() - 1; ++i) {
            if (profile.at(i) < options.gradientThreshold)
                continue;
            if (profile.at(i) < profile.at(i - 1) || profile.at(i) < profile.at(i + 1))
                continue;
            if (polarity == QLatin1String("dark_to_light") && signedProfile.at(i) < 0.0)
                continue;
            if (polarity == QLatin1String("light_to_dark") && signedProfile.at(i) > 0.0)
                continue;
            peaks.append(Peak{i, profile.at(i)});
        }
        std::sort(peaks.begin(), peaks.end(),
                  [](const Peak &a, const Peak &b) { return a.mag > b.mag; });
        if (peaks.isEmpty()) {
            cvError = QStringLiteral("卡尺未找到有效边缘");
            return;
        }

        const Peak best = peaks.first();
        const double bestIdx = refinePeakIndex(profile, best.index);
        const double bestT = -halfLen + (length * bestIdx) / double(samples - 1);
        edgePoints.append(QPointF(center.x() + bestT * c, center.y() + bestT * s));
        confidence = qBound(0.0, 1.0, best.mag / (best.mag + 20.0));

        const int minGapSamples = qMax(1, int(std::lround(options.minEdgeGap / step)));
        Peak second{-1, 0.0};
        for (const Peak &p : peaks) {
            if (std::abs(p.index - best.index) < minGapSamples)
                continue;
            if (p.mag > second.mag)
                second = p;
        }
        if (second.index >= 0 && second.mag > best.mag * options.secondPeakRatio) {
            const double secondIdx = refinePeakIndex(profile, second.index);
            const double secondT = -halfLen + (length * secondIdx) / double(samples - 1);
            edgePoints.append(QPointF(center.x() + secondT * c, center.y() + secondT * s));
            distance = QLineF(edgePoints.first(), edgePoints.last()).length();
            confidence = qBound(0.0, 1.0, 0.5 * (confidence + second.mag / (second.mag + 20.0)));
        }

        cv::Mat display;
        if (input.mat().channels() == 1)
            cv::cvtColor(input.mat(), display, cv::COLOR_GRAY2BGR);
        else if (input.mat().channels() == 3)
            display = input.mat().clone();
        else
            cv::cvtColor(input.mat(), display, cv::COLOR_BGRA2BGR);
        const cv::Point p0(int(center.x() - halfLen * c), int(center.y() - halfLen * s));
        const cv::Point p1(int(center.x() + halfLen * c), int(center.y() + halfLen * s));
        cv::line(display, p0, p1, cv::Scalar(0, 255, 255), 1);
        for (const QPointF &ep : edgePoints)
            cv::circle(display, cv::Point(int(ep.x()), int(ep.y())), 3, cv::Scalar(0, 0, 255), -1);
        overlay = VisionImage(display, input.sourcePath());
        ok = true;
    }, error);

    if (!ran)
        return false;
    if (!ok) {
        if (error && error->isEmpty())
            *error = cvError.isEmpty() ? QStringLiteral("卡尺采样失败") : cvError;
        else if (error && !cvError.isEmpty())
            *error = cvError;
        return false;
    }
    return true;
}

bool FitCircleAlgorithm::fit(const QVector<QPointF> &points,
                             QPointF &center,
                             double &radius,
                             double &residual,
                             QString *error)
{
    FitCircleResult result;
    if (!fit(points, FitOptions{}, result, error)) {
        center = {};
        radius = 0.0;
        residual = 0.0;
        return false;
    }
    center = result.center;
    radius = result.radius;
    residual = result.residualRms;
    return true;
}

bool FitCircleAlgorithm::fit(const QVector<QPointF> &points,
                             const FitOptions &options,
                             FitCircleResult &result,
                             QString *error)
{
    result = {};
    if (points.size() < 3) {
        if (error)
            *error = QStringLiteral("拟合圆至少需要 3 个点");
        return false;
    }
    if (pointsNearlyCollinear(points, kCollinearTolerancePx)) {
        if (error)
            *error = QStringLiteral("点近似共线，无法拟合圆");
        return false;
    }

    QString cvError;
    bool ok = false;
    const bool ran = Selt::runOpenCv([&]() {
        if (options.mode.toLower() != QLatin1String("ransac")) {
            std::vector<cv::Point2f> pts;
            pts.reserve(size_t(points.size()));
            for (const QPointF &p : points)
                pts.emplace_back(float(p.x()), float(p.y()));
            cv::Point2f c;
            float r = 0.0f;
            if (!algebraicCircleFit(pts, &c, &r)) {
                cvError = QStringLiteral("代数圆拟合失败");
                return;
            }
            result.center = QPointF(c.x, c.y);
            result.radius = double(r);
            result.residualRms = circleResidualRms(points, result.center, result.radius);
            result.inlierCount = points.size();
            result.confidence = qBound(0.0, 1.0, 1.0 - result.residualRms / qMax(3.0, result.radius * 0.35));
            ok = result.radius > 0.0 && std::isfinite(result.radius)
                && result.residualRms < qMax(3.0, result.radius * 0.35);
            if (!ok)
                cvError = QStringLiteral("拟合圆残差过大或半径无效");
            return;
        }

        std::mt19937 rng(42);
        std::uniform_int_distribution<int> dist(0, points.size() - 1);
        FitCircleResult best;
        const int iterations = qMax(1, options.maxIterations);
        for (int iter = 0; iter < iterations; ++iter) {
            QVector<QPointF> sample;
            sample.reserve(3);
            for (int k = 0; k < 3; ++k)
                sample.append(points.at(dist(rng)));
            if (pointsNearlyCollinear(sample, kCollinearTolerancePx))
                continue;
            std::vector<cv::Point2f> pts;
            for (const QPointF &p : sample)
                pts.emplace_back(float(p.x()), float(p.y()));
            cv::Point2f c;
            float r = 0.0f;
            if (!algebraicCircleFit(pts, &c, &r))
                continue;
            QVector<QPointF> inliers;
            for (const QPointF &p : points) {
                if (std::abs(QLineF(QPointF(c.x, c.y), p).length() - double(r))
                    <= options.residualThreshold)
                    inliers.append(p);
            }
            if (inliers.size() < qMax(options.minInliers, 3))
                continue;
            if (double(inliers.size()) / points.size() < options.minInlierRatio)
                continue;
            std::vector<cv::Point2f> inPts;
            for (const QPointF &p : inliers)
                inPts.emplace_back(float(p.x()), float(p.y()));
            cv::Point2f fc;
            float fr = 0.0f;
            if (!algebraicCircleFit(inPts, &fc, &fr))
                continue;
            FitCircleResult candidate;
            candidate.center = QPointF(fc.x, fc.y);
            candidate.radius = double(fr);
            candidate.inlierCount = inliers.size();
            candidate.residualRms = circleResidualRms(inliers, candidate.center, candidate.radius);
            candidate.confidence = qBound(0.0, 1.0,
                                         double(inliers.size()) / points.size()
                                             * (1.0 - candidate.residualRms / qMax(1.0, options.residualThreshold * 2.0)));
            if (candidate.inlierCount > best.inlierCount
                || (candidate.inlierCount == best.inlierCount
                    && candidate.residualRms < best.residualRms))
                best = candidate;
        }
        if (best.inlierCount < qMax(options.minInliers, 3) || !(best.radius > 0.0)) {
            cvError = QStringLiteral("RANSAC 内点不足或拟合失败");
            return;
        }
        result = best;
        ok = true;
    }, error);
    if (!ran)
        return false;
    if (!ok) {
        if (error)
            *error = cvError.isEmpty() ? QStringLiteral("拟合圆失败") : cvError;
        result = {};
        return false;
    }
    return true;
}

bool FitLineAlgorithm::fit(const QVector<QPointF> &points,
                           QPointF &start,
                           QPointF &end,
                           double &angleDeg,
                           QString *error)
{
    FitLineResult result;
    if (!fit(points, FitOptions{}, result, error)) {
        start = {};
        end = {};
        angleDeg = 0.0;
        return false;
    }
    start = result.start;
    end = result.end;
    angleDeg = result.angleDeg;
    return true;
}

bool FitLineAlgorithm::fit(const QVector<QPointF> &points,
                           const FitOptions &options,
                           FitLineResult &result,
                           QString *error)
{
    result = {};
    if (points.size() < 2) {
        if (error)
            *error = QStringLiteral("拟合直线至少需要 2 个点");
        return false;
    }

    bool ok = false;
    QString cvError;
    const bool ran = Selt::runOpenCv([&]() {
        if (options.mode.toLower() != QLatin1String("ransac") || points.size() < 3) {
            ok = fitLineL2(points, &result, &cvError);
            return;
        }

        std::mt19937 rng(7);
        std::uniform_int_distribution<int> dist(0, points.size() - 1);
        FitLineResult best;
        const int iterations = qMax(1, options.maxIterations);
        for (int iter = 0; iter < iterations; ++iter) {
            const QPointF a = points.at(dist(rng));
            const QPointF b = points.at(dist(rng));
            if (QLineF(a, b).length() < kMinLineLength)
                continue;
            const QPointF dir = b - a;
            const double denom = QLineF(a, b).length();
            QVector<QPointF> inliers;
            for (const QPointF &p : points) {
                const double cross = std::abs((p.x() - a.x()) * dir.y() - (p.y() - a.y()) * dir.x()) / denom;
                if (cross <= options.residualThreshold)
                    inliers.append(p);
            }
            if (inliers.size() < qMax(options.minInliers, 2))
                continue;
            if (double(inliers.size()) / points.size() < options.minInlierRatio)
                continue;
            FitLineResult candidate;
            if (!fitLineL2(inliers, &candidate, nullptr))
                continue;
            candidate.inlierCount = inliers.size();
            candidate.confidence = qBound(0.0, 1.0, double(inliers.size()) / points.size());
            if (candidate.inlierCount > best.inlierCount
                || (candidate.inlierCount == best.inlierCount
                    && candidate.residualRms < best.residualRms))
                best = candidate;
        }
        if (best.inlierCount < qMax(options.minInliers, 2)) {
            cvError = QStringLiteral("RANSAC 直线内点不足");
            return;
        }
        result = best;
        ok = true;
    }, error);
    if (!ran)
        return false;
    if (!ok) {
        if (error)
            *error = cvError.isEmpty() ? QStringLiteral("拟合直线失败") : cvError;
        return false;
    }
    return true;
}

bool AngleMeasureAlgorithm::fromLines(const QPointF &l1Start,
                                      const QPointF &l1End,
                                      const QPointF &l2Start,
                                      const QPointF &l2End,
                                      double &angleDeg,
                                      QString *error)
{
    angleDeg = 0.0;
    const QPointF v1 = l1End - l1Start;
    const QPointF v2 = l2End - l2Start;
    const double n1 = std::hypot(v1.x(), v1.y());
    const double n2 = std::hypot(v2.x(), v2.y());
    if (n1 < kMinLineLength || n2 < kMinLineLength) {
        if (error)
            *error = QStringLiteral("角度测量输入线段无效");
        return false;
    }
    double cosA = QPointF::dotProduct(v1, v2) / (n1 * n2);
    cosA = qBound(-1.0, cosA, 1.0);
    angleDeg = std::acos(cosA) * 180.0 / CV_PI;
    if (angleDeg > 90.0)
        angleDeg = 180.0 - angleDeg;
    return true;
}

bool ParallelDistanceAlgorithm::fromLines(const QPointF &l1Start,
                                          const QPointF &l1End,
                                          const QPointF &l2Start,
                                          const QPointF &l2End,
                                          double &distance,
                                          QString *error)
{
    distance = 0.0;
    const QLineF ref(l1Start, l1End);
    if (ref.length() < kMinLineLength) {
        if (error)
            *error = QStringLiteral("平行距参考直线无效");
        return false;
    }
    if (QLineF(l2Start, l2End).length() < kMinLineLength) {
        if (error)
            *error = QStringLiteral("平行距目标直线无效");
        return false;
    }
    const QPointF mid = (l2Start + l2End) * 0.5;
    distance = std::abs((mid.y() - l1Start.y()) * (l1End.x() - l1Start.x())
                        - (mid.x() - l1Start.x()) * (l1End.y() - l1Start.y()))
        / ref.length();
    return std::isfinite(distance);
}

bool PerpendicularityAlgorithm::fromLines(const QPointF &l1Start,
                                          const QPointF &l1End,
                                          const QPointF &l2Start,
                                          const QPointF &l2End,
                                          double &angleDeg,
                                          double &deviationDeg,
                                          QString *error)
{
    angleDeg = 0.0;
    deviationDeg = 0.0;
    if (!AngleMeasureAlgorithm::fromLines(l1Start, l1End, l2Start, l2End, angleDeg, error))
        return false;
    // Smallest angle to 90 degrees in [0, 90].
    double a = std::fmod(std::abs(angleDeg), 180.0);
    if (a > 90.0)
        a = 180.0 - a;
    deviationDeg = std::abs(a - 90.0);
    return std::isfinite(deviationDeg);
}

bool PositionDeviationAlgorithm::fromPoints(const QPointF &actual,
                                            const QPointF &reference,
                                            double &dx,
                                            double &dy,
                                            double &distance,
                                            QString *error)
{
    Q_UNUSED(error);
    dx = actual.x() - reference.x();
    dy = actual.y() - reference.y();
    distance = qHypot(dx, dy);
    return std::isfinite(distance);
}
