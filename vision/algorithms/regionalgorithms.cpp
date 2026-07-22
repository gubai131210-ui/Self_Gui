#include "vision/algorithms/regionalgorithms.h"

#include "vision/algorithms/opencvguard.h"

#include <QtGlobal>
#include <algorithm>
#include <cmath>
#include <opencv2/imgproc.hpp>
#include <vector>

namespace {

bool requireInput(const VisionImage &input, QString *error)
{
    if (input.isEmpty()) {
        if (error)
            *error = QStringLiteral("输入图像为空");
        return false;
    }
    return true;
}

cv::Mat toGrayMat(const cv::Mat &src)
{
    cv::Mat gray;
    if (src.channels() == 1)
        gray = src;
    else if (src.channels() == 3)
        cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    else if (src.channels() == 4)
        cv::cvtColor(src, gray, cv::COLOR_BGRA2GRAY);
    else
        cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    return gray;
}

cv::Mat toBinary8u(const cv::Mat &src)
{
    cv::Mat gray = toGrayMat(src);
    cv::Mat binary;
    if (gray.type() != CV_8UC1)
        gray.convertTo(gray, CV_8U);
    // Prefer Otsu for continuous gray; keep threshold(0) only for already sparse masks.
    const int total = gray.rows * gray.cols;
    const int nonZero = cv::countNonZero(gray);
    double minV = 0.0;
    double maxV = 0.0;
    cv::minMaxLoc(gray, &minV, &maxV);
    const bool looksBinary = (minV == 0.0 && maxV == 255.0)
        && (nonZero == 0 || nonZero == total || nonZero < total * 0.95);
    if (looksBinary) {
        cv::threshold(gray, binary, 0, 255, cv::THRESH_BINARY);
    } else {
        cv::threshold(gray, binary, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
    }
    return binary;
}

cv::Mat toBinary8uMasked(const cv::Mat &src,
                         const cv::Mat &validMask,
                         const QString &thresholdMode,
                         const QString &polarity,
                         int manualThreshold)
{
    cv::Mat gray = toGrayMat(src);
    if (gray.type() != CV_8UC1)
        gray.convertTo(gray, CV_8U);

    cv::Mat mask;
    if (!validMask.empty()) {
        if (validMask.size() != gray.size())
            cv::resize(validMask, mask, gray.size(), 0, 0, cv::INTER_NEAREST);
        else
            mask = validMask;
        if (mask.type() != CV_8UC1)
            mask.convertTo(mask, CV_8U);
    }

    const int total = gray.rows * gray.cols;
    const int nonZero = mask.empty() ? cv::countNonZero(gray) : cv::countNonZero(mask);
    double minV = 0.0;
    double maxV = 0.0;
    if (mask.empty()) {
        cv::minMaxLoc(gray, &minV, &maxV);
    } else {
        cv::minMaxLoc(gray, &minV, &maxV, nullptr, nullptr, mask);
    }
    const bool looksBinary = (minV == 0.0 && maxV == 255.0)
        && (nonZero == 0 || nonZero == total || nonZero < total * 0.95);

    const QString mode = thresholdMode.toLower();
    const QString pol = polarity.toLower();
    int threshType = (pol == QLatin1String("light")) ? cv::THRESH_BINARY : cv::THRESH_BINARY_INV;
    if (pol == QLatin1String("any") || pol.isEmpty())
        threshType = cv::THRESH_BINARY; // will flip later if needed via Otsu dual attempt

    cv::Mat binary;
    if (looksBinary && (mode == QLatin1String("auto") || mode.isEmpty())) {
        cv::threshold(gray, binary, 0, 255, cv::THRESH_BINARY);
    } else if (mode == QLatin1String("manual")) {
        cv::threshold(gray, binary, manualThreshold, 255, threshType);
    } else if (mode == QLatin1String("relative")) {
        const double mid = 0.5 * (minV + maxV);
        cv::threshold(gray, binary, mid, 255, threshType);
    } else {
        // Otsu only on valid pixels: temporarily set outside to median of inside to avoid bias.
        cv::Mat work = gray.clone();
        if (!mask.empty()) {
            const double meanInside = cv::mean(gray, mask)[0];
            work.setTo(cv::Scalar(meanInside), mask == 0);
        }
        if (pol == QLatin1String("any") || pol.isEmpty()) {
            cv::Mat binDark;
            cv::Mat binLight;
            cv::threshold(work, binDark, 0, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);
            cv::threshold(work, binLight, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
            if (!mask.empty()) {
                cv::bitwise_and(binDark, mask, binDark);
                cv::bitwise_and(binLight, mask, binLight);
            }
            // Prefer polarity that yields compact mid-sized blobs (not empty / not full).
            const int darkNz = cv::countNonZero(binDark);
            const int lightNz = cv::countNonZero(binLight);
            const int valid = mask.empty() ? total : cv::countNonZero(mask);
            const double darkFill = valid > 0 ? double(darkNz) / double(valid) : 1.0;
            const double lightFill = valid > 0 ? double(lightNz) / double(valid) : 1.0;
            auto bandScore = [](double fill) {
                // Ideal object fill is small-to-moderate; near-full is almost always background.
                if (fill < 0.005 || fill > 0.65)
                    return -1.0;
                return 1.0 - std::abs(fill - 0.18);
            };
            const double darkScore = bandScore(darkFill);
            const double lightScore = bandScore(lightFill);
            if (lightScore > darkScore)
                binary = binLight;
            else if (darkScore > lightScore)
                binary = binDark;
            else
                binary = (lightFill <= darkFill) ? binLight : binDark;
        } else {
            cv::threshold(work, binary, 0, 255, threshType | cv::THRESH_OTSU);
        }
    }

    if (!mask.empty())
        cv::bitwise_and(binary, mask, binary);
    return binary;
}

cv::Mat toBgrDisplay(const cv::Mat &src)
{
    cv::Mat display;
    if (src.channels() == 1)
        cv::cvtColor(src, display, cv::COLOR_GRAY2BGR);
    else if (src.channels() == 3)
        display = src.clone();
    else if (src.channels() == 4)
        cv::cvtColor(src, display, cv::COLOR_BGRA2BGR);
    else
        cv::cvtColor(src, display, cv::COLOR_GRAY2BGR);
    return display;
}

cv::Mat toMask8u(const VisionImage &image, QString *error)
{
    if (image.isEmpty()) {
        if (error)
            *error = QStringLiteral("掩膜图像为空");
        return {};
    }
    cv::Mat gray = toGrayMat(image.mat());
    if (gray.type() != CV_8UC1)
        gray.convertTo(gray, CV_8U);
    cv::Mat mask;
    cv::threshold(gray, mask, 0, 255, cv::THRESH_BINARY);
    return mask;
}

} // namespace

bool MaskCombineAlgorithm::apply(const VisionImage &a,
                                 const VisionImage &b,
                                 VisionImage &out,
                                 const QString &op,
                                 QString *error)
{
    if (!requireInput(a, error))
        return false;

    const QString o = op.trimmed().toLower();
    if (o != QLatin1String("and") && o != QLatin1String("or") && o != QLatin1String("xor")
        && o != QLatin1String("not")) {
        if (error)
            *error = QStringLiteral("不支持的掩膜运算: %1（可用 and/or/xor/not）").arg(op);
        return false;
    }
    if (o != QLatin1String("not") && b.isEmpty()) {
        if (error)
            *error = QStringLiteral("掩膜 B 为空");
        return false;
    }

    cv::Mat dst;
    if (!Selt::runOpenCv([&]() {
            cv::Mat maskA = toMask8u(a, nullptr);
            if (o == QLatin1String("not")) {
                cv::bitwise_not(maskA, dst);
                return;
            }

            cv::Mat maskB = toMask8u(b, nullptr);
            if (maskA.size() != maskB.size())
                cv::resize(maskB, maskB, maskA.size(), 0, 0, cv::INTER_NEAREST);

            if (o == QLatin1String("and"))
                cv::bitwise_and(maskA, maskB, dst);
            else if (o == QLatin1String("or"))
                cv::bitwise_or(maskA, maskB, dst);
            else
                cv::bitwise_xor(maskA, maskB, dst);
        }, error)) {
        return false;
    }
    out = VisionImage(dst, a.sourcePath());
    return true;
}

bool FindContoursAlgorithm::apply(const VisionImage &input,
                                  QVector<QVector<QPointF>> &contoursOut,
                                  VisionImage &overlay,
                                  QString *error)
{
    if (!requireInput(input, error))
        return false;

    contoursOut.clear();
    cv::Mat display;
    if (!Selt::runOpenCv([&]() {
            const cv::Mat binary = toBinary8u(input.mat());
            std::vector<std::vector<cv::Point>> contours;
            cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);

            contoursOut.reserve(static_cast<int>(contours.size()));
            for (const std::vector<cv::Point> &c : contours) {
                QVector<QPointF> pts;
                pts.reserve(static_cast<int>(c.size()));
                for (const cv::Point &p : c)
                    pts.append(QPointF(p.x, p.y));
                contoursOut.append(pts);
            }

            display = toBgrDisplay(input.mat());
            cv::drawContours(display, contours, -1, cv::Scalar(0, 255, 0), 2);
        }, error)) {
        return false;
    }
    overlay = VisionImage(display, input.sourcePath());
    return true;
}

bool ConnectedComponentsAlgorithm::apply(const VisionImage &input,
                                         VisionImage &labelImage,
                                         int &count,
                                         QVector<QPointF> &centroids,
                                         QVector<double> &areas,
                                         QString *error)
{
    if (!requireInput(input, error))
        return false;

    centroids.clear();
    areas.clear();
    count = 0;
    cv::Mat labels;
    if (!Selt::runOpenCv([&]() {
            const cv::Mat binary = toBinary8u(input.mat());
            cv::Mat stats;
            cv::Mat cents;
            const int n = cv::connectedComponentsWithStats(binary, labels, stats, cents, 8, CV_32S);
            // Label 0 is background.
            count = qMax(0, n - 1);
            centroids.reserve(count);
            areas.reserve(count);
            for (int label = 1; label < n; ++label) {
                const double area = stats.at<int>(label, cv::CC_STAT_AREA);
                const double cx = cents.at<double>(label, 0);
                const double cy = cents.at<double>(label, 1);
                centroids.append(QPointF(cx, cy));
                areas.append(area);
            }
        }, error)) {
        return false;
    }
    labelImage = VisionImage(labels, input.sourcePath());
    return true;
}

bool BlobAnalyzeAlgorithm::apply(const VisionImage &input,
                                 double minArea,
                                 double maxArea,
                                 double minCircularity,
                                 QVector<QPointF> &outCenters,
                                 QVector<double> &outAreas,
                                 VisionImage &overlay,
                                 QString *error)
{
    Options options;
    options.minArea = minArea;
    options.maxArea = maxArea;
    options.minCircularity = minCircularity;
    QVector<double> scores;
    return apply(input, options, outCenters, outAreas, scores, overlay, error);
}

bool BlobAnalyzeAlgorithm::apply(const VisionImage &input,
                                 const Options &options,
                                 QVector<QPointF> &outCenters,
                                 QVector<double> &outAreas,
                                 QVector<double> &outScores,
                                 VisionImage &overlay,
                                 QString *error)
{
    RegionData region;
    int selected = -1;
    if (!apply(input, options, region, outScores, overlay, &selected, error))
        return false;
    outCenters.clear();
    outAreas.clear();
    for (const RegionStats &s : region.regions) {
        outCenters.append(s.centroid);
        outAreas.append(s.area);
    }
    return true;
}

bool BlobAnalyzeAlgorithm::apply(const VisionImage &input,
                                 const Options &options,
                                 RegionData &outRegion,
                                 QVector<double> &outScores,
                                 VisionImage &overlay,
                                 int *selectedIndex,
                                 QString *error)
{
    return apply(input, options, outRegion, outScores, overlay, selectedIndex, nullptr, error);
}

bool BlobAnalyzeAlgorithm::apply(const VisionImage &input,
                                 const Options &options,
                                 RegionData &outRegion,
                                 QVector<double> &outScores,
                                 VisionImage &overlay,
                                 int *selectedIndex,
                                 int *unfilteredCount,
                                 QString *error)
{
    if (!requireInput(input, error))
        return false;

    outRegion = RegionData{};
    outScores.clear();
    if (selectedIndex)
        *selectedIndex = -1;
    if (unfilteredCount)
        *unfilteredCount = 0;

    const double areaMin = qMax(0.0, options.minArea);
    const double areaMax = options.maxArea > 0.0 ? options.maxArea : 1e18;
    const double circMin = qBound(0.0, options.minCircularity, 1.0);
    const double circMax = qBound(0.0, options.maxCircularity, 1.0);
    const double periMin = qMax(0.0, options.minPerimeter);
    const double periMax = options.maxPerimeter > 0.0 ? options.maxPerimeter : 1e18;
    const double elongMin = qMax(1.0, options.minElongation);
    const double elongMax = options.maxElongation > 0.0 ? options.maxElongation : 1e18;
    const double maxW = options.maxWidth > 0.0 ? options.maxWidth : 1e18;
    const double maxH = options.maxHeight > 0.0 ? options.maxHeight : 1e18;
    const double maxAspect = options.maxAspectRatio > 0.0 ? options.maxAspectRatio : 1e18;

    cv::Mat display;
    QVector<RegionStats> regions;
    QVector<double> scores;
    int rawCandidateCount = 0;

    if (!Selt::runOpenCv([&]() {
            cv::Mat maskMat;
            if (!options.validMask.isEmpty())
                maskMat = options.validMask.mat();
            const cv::Mat binary = toBinary8uMasked(input.mat(),
                                                   maskMat,
                                                   options.thresholdMode,
                                                   options.polarity,
                                                   options.manualThreshold);
            std::vector<std::vector<cv::Point>> contours;
            cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);
            display = toBgrDisplay(input.mat());

            struct Blob {
                RegionStats stats;
                double score{0.0};
                int contourIndex{0};
            };
            QVector<Blob> blobs;

            for (size_t i = 0; i < contours.size(); ++i) {
                const double area = cv::contourArea(contours[i]);
                if (area < 1.0)
                    continue;
                ++rawCandidateCount;

                if (area < areaMin || area > areaMax)
                    continue;
                const double peri = cv::arcLength(contours[i], true);
                if (peri < periMin || peri > periMax)
                    continue;
                const double circularity =
                    peri > 1e-6 ? (4.0 * CV_PI * area) / (peri * peri) : 0.0;
                if (circularity < circMin || circularity > circMax)
                    continue;

                const cv::RotatedRect rr = cv::minAreaRect(contours[i]);
                const double w = qMax(1e-6, double(rr.size.width));
                const double h = qMax(1e-6, double(rr.size.height));
                const double aspect = qMax(w, h) / qMin(w, h);
                if (aspect < options.minAspectRatio || aspect > maxAspect)
                    continue;
                const double extent = area / (w * h);
                if (extent < options.minExtent)
                    continue;
                std::vector<cv::Point> hull;
                cv::convexHull(contours[i], hull);
                const double hullArea = cv::contourArea(hull);
                const double solidity = hullArea > 1e-6 ? area / hullArea : 0.0;
                if (solidity < options.minSolidity)
                    continue;

                cv::Moments m = cv::moments(contours[i]);
                QPointF center(0.0, 0.0);
                if (std::abs(m.m00) > 1e-6)
                    center = QPointF(m.m10 / m.m00, m.m01 / m.m00);
                else {
                    const cv::Rect box = cv::boundingRect(contours[i]);
                    center = QPointF(box.x + box.width * 0.5, box.y + box.height * 0.5);
                }
                const QPointF globalCenter = center + options.originOffset;
                if (globalCenter.x() < options.minCx || globalCenter.x() > options.maxCx
                    || globalCenter.y() < options.minCy || globalCenter.y() > options.maxCy)
                    continue;

                double elongation = 1.0;
                if (std::abs(m.m00) > 1e-6) {
                    const double mu20 = m.mu20 / m.m00;
                    const double mu02 = m.mu02 / m.m00;
                    const double mu11 = m.mu11 / m.m00;
                    const double delta =
                        std::sqrt(4.0 * mu11 * mu11 + (mu20 - mu02) * (mu20 - mu02));
                    const double major = (mu20 + mu02 + delta) * 0.5;
                    const double minorM = (mu20 + mu02 - delta) * 0.5;
                    elongation = minorM > 1e-10 ? major / minorM : 1.0;
                }
                if (elongation < elongMin || elongation > elongMax)
                    continue;

                if (w < options.minWidth || w > maxW || h < options.minHeight || h > maxH)
                    continue;

                const cv::Rect aabb = cv::boundingRect(contours[i]);

                Blob b;
                b.stats.centroid = globalCenter;
                b.stats.area = area;
                b.stats.perimeter = peri;
                b.stats.circularity = circularity;
                b.stats.elongation = elongation;
                b.stats.aspectRatio = aspect;
                b.stats.extent = extent;
                b.stats.solidity = solidity;
                b.stats.width = w;
                b.stats.height = h;
                b.stats.boundingRect = QRectF(aabb.x + options.originOffset.x(),
                                              aabb.y + options.originOffset.y(),
                                              aabb.width,
                                              aabb.height);
                b.score = circularity;
                b.contourIndex = int(i);
                blobs.append(b);
            }

            if (options.sortBy == QLatin1String("circularity")) {
                std::sort(blobs.begin(), blobs.end(),
                          [](const Blob &a, const Blob &b) {
                              return a.stats.circularity > b.stats.circularity;
                          });
            } else if (options.sortBy == QLatin1String("x")) {
                std::sort(blobs.begin(), blobs.end(),
                          [](const Blob &a, const Blob &b) {
                              return a.stats.centroid.x() < b.stats.centroid.x();
                          });
            } else if (options.sortBy == QLatin1String("y")) {
                std::sort(blobs.begin(), blobs.end(),
                          [](const Blob &a, const Blob &b) {
                              return a.stats.centroid.y() < b.stats.centroid.y();
                          });
            } else if (options.sortBy == QLatin1String("elongation")) {
                std::sort(blobs.begin(), blobs.end(),
                          [](const Blob &a, const Blob &b) {
                              return a.stats.elongation > b.stats.elongation;
                          });
            } else if (options.sortBy == QLatin1String("perimeter")) {
                std::sort(blobs.begin(), blobs.end(),
                          [](const Blob &a, const Blob &b) {
                              return a.stats.perimeter > b.stats.perimeter;
                          });
            } else {
                std::sort(blobs.begin(), blobs.end(),
                          [](const Blob &a, const Blob &b) { return a.stats.area > b.stats.area; });
            }
            if (options.maxCount > 0 && blobs.size() > options.maxCount)
                blobs.resize(options.maxCount);

            for (int i = 0; i < blobs.size(); ++i) {
                Blob &b = blobs[i];
                b.stats.label = i + 1;
                regions.append(b.stats);
                scores.append(b.score);
                // Draw in local coordinates (overlay stays on cropped/masked image).
                const QPointF local = b.stats.centroid - options.originOffset;
                cv::drawContours(display, contours, b.contourIndex, cv::Scalar(0, 255, 0), 2);
                cv::circle(display,
                           cv::Point(static_cast<int>(std::lround(local.x())),
                                     static_cast<int>(std::lround(local.y()))),
                           3, cv::Scalar(0, 0, 255), -1);
            }
        }, error)) {
        return false;
    }

    if (unfilteredCount)
        *unfilteredCount = rawCandidateCount;

    outRegion.regions = regions;
    outRegion.labelCount = regions.size();
    outScores = scores;

    int sel = options.selectIndex;
    if (sel < 0)
        sel = 0;
    if (!regions.isEmpty() && sel >= 0 && sel < regions.size()) {
        if (selectedIndex)
            *selectedIndex = sel;
        const RegionStats &picked = regions.at(sel);
        const QPointF local = picked.centroid - options.originOffset;
        cv::circle(display,
                   cv::Point(static_cast<int>(std::lround(local.x())),
                             static_cast<int>(std::lround(local.y()))),
                   8, cv::Scalar(0, 165, 255), 2);
    } else if (selectedIndex) {
        *selectedIndex = -1;
    }

    overlay = VisionImage(display, input.sourcePath());
    return true;
}

bool RegionFillAlgorithm::apply(const VisionImage &input,
                                VisionImage &out,
                                VisionImage &overlay,
                                QString *error)
{
    if (input.isEmpty()) {
        if (error)
            *error = QStringLiteral("区域填充输入图像为空");
        return false;
    }
    cv::Mat filled;
    cv::Mat display;
    if (!Selt::runOpenCv([&]() {
            cv::Mat binary = toBinary8u(input.mat());
            std::vector<std::vector<cv::Point>> contours;
            cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
            filled = cv::Mat::zeros(binary.size(), CV_8UC1);
            cv::drawContours(filled, contours, -1, cv::Scalar(255), cv::FILLED);
            display = toBgrDisplay(filled);
            cv::drawContours(display, contours, -1, cv::Scalar(0, 255, 0), 2);
        }, error)) {
        return false;
    }
    out = VisionImage(filled, input.sourcePath());
    overlay = VisionImage(display, input.sourcePath());
    return true;
}
