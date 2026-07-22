#include "vision/algorithms/locatealgorithms.h"

#include "vision/algorithms/opencvguard.h"

#include <QLineF>
#include <QtGlobal>
#include <algorithm>
#include <cmath>
#include <opencv2/features2d.hpp>
#include <opencv2/imgproc.hpp>
#include <vector>

#if defined(SELT_HAS_OPENCV_CALIB3D) && SELT_HAS_OPENCV_CALIB3D
#  include <opencv2/calib3d.hpp>
#endif

namespace {

bool requireInput(const VisionImage &input, QString *error, const QString &msg = QStringLiteral("输入图像为空"))
{
    if (input.isEmpty()) {
        if (error)
            *error = msg;
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

cv::Mat ensureGray8u(const cv::Mat &src)
{
    cv::Mat gray = toGrayMat(src);
    if (gray.depth() != CV_8U)
        gray.convertTo(gray, CV_8U);
    return gray;
}

double lineLength(const LocateLine2D &line)
{
    return QLineF(line.start, line.end).length();
}

double rectIoU(const cv::Rect &a, const cv::Rect &b)
{
    const cv::Rect inter = a & b;
    if (inter.area() <= 0)
        return 0.0;
    const double uni = double(a.area() + b.area() - inter.area());
    return uni > 0.0 ? inter.area() / uni : 0.0;
}

} // namespace

bool HoughLinesAlgorithm::apply(const VisionImage &input,
                                QVector<LocateLine2D> &linesOut,
                                double cannyLow,
                                double cannyHigh,
                                VisionImage &overlay,
                                QString *error)
{
    HoughLinesOptions options;
    options.cannyLow = cannyLow;
    options.cannyHigh = cannyHigh;
    return apply(input, linesOut, options, overlay, error);
}

bool HoughLinesAlgorithm::apply(const VisionImage &input,
                                QVector<LocateLine2D> &linesOut,
                                const HoughLinesOptions &options,
                                VisionImage &overlay,
                                QString *error)
{
    if (!requireInput(input, error))
        return false;

    linesOut.clear();
    cv::Mat display;
    if (!Selt::runOpenCv([&]() {
            const cv::Mat gray = ensureGray8u(input.mat());
            cv::Mat edges;
            cv::Canny(gray, edges, options.cannyLow, options.cannyHigh);
            std::vector<cv::Vec4i> lines;
            cv::HoughLinesP(edges, lines, 1.0, CV_PI / 180.0,
                            qMax(1, options.accumulatorThreshold),
                            options.minLineLength,
                            options.maxLineGap);

            display = toBgrDisplay(input.mat());
            linesOut.reserve(static_cast<int>(lines.size()));
            double maxLen = 1.0;
            for (const cv::Vec4i &l : lines) {
                LocateLine2D line;
                line.start = QPointF(l[0], l[1]);
                line.end = QPointF(l[2], l[3]);
                line.confidence = 1.0;
                maxLen = qMax(maxLen, lineLength(line));
                linesOut.append(line);
            }
            for (LocateLine2D &line : linesOut)
                line.confidence = qBound(0.0, 1.0, lineLength(line) / maxLen);

            if (options.sortBy == QLatin1String("score")) {
                std::sort(linesOut.begin(), linesOut.end(),
                          [](const LocateLine2D &a, const LocateLine2D &b) {
                              return a.confidence > b.confidence;
                          });
            } else {
                std::sort(linesOut.begin(), linesOut.end(),
                          [](const LocateLine2D &a, const LocateLine2D &b) {
                              return lineLength(a) > lineLength(b);
                          });
            }
            if (options.maxCount > 0 && linesOut.size() > options.maxCount)
                linesOut.resize(options.maxCount);

            for (const LocateLine2D &line : linesOut) {
                cv::line(display,
                         cv::Point(int(std::lround(line.start.x())), int(std::lround(line.start.y()))),
                         cv::Point(int(std::lround(line.end.x())), int(std::lround(line.end.y()))),
                         cv::Scalar(0, 255, 255), 2);
            }
        }, error)) {
        return false;
    }
    overlay = VisionImage(display, input.sourcePath());
    return true;
}

bool HoughCirclesAlgorithm::apply(const VisionImage &input,
                                  QVector<LocateCircle2D> &circlesOut,
                                  double minRadius,
                                  double maxRadius,
                                  VisionImage &overlay,
                                  QString *error)
{
    HoughCirclesOptions options;
    options.minRadius = minRadius;
    options.maxRadius = maxRadius;
    return apply(input, circlesOut, options, overlay, error);
}

bool HoughCirclesAlgorithm::apply(const VisionImage &input,
                                  QVector<LocateCircle2D> &circlesOut,
                                  const HoughCirclesOptions &options,
                                  VisionImage &overlay,
                                  QString *error)
{
    if (!requireInput(input, error))
        return false;

    circlesOut.clear();
    const double rMin = qMax(1.0, options.minRadius);
    const double rMax = options.maxRadius > rMin ? options.maxRadius : 0.0;

    cv::Mat display;
    if (!Selt::runOpenCv([&]() {
            const cv::Mat gray = ensureGray8u(input.mat());
            cv::Mat blurred;
            cv::GaussianBlur(gray, blurred, cv::Size(9, 9), 2.0, 2.0);
            std::vector<cv::Vec3f> circles;
            const double minDist = options.minDist > 0.0 ? options.minDist : gray.rows / 8.0;
            cv::HoughCircles(blurred, circles, cv::HOUGH_GRADIENT, qMax(0.5, options.dp), minDist,
                             options.param1, options.param2,
                             static_cast<int>(std::lround(rMin)),
                             rMax > 0.0 ? static_cast<int>(std::lround(rMax)) : 0);

            display = toBgrDisplay(input.mat());
            circlesOut.reserve(static_cast<int>(circles.size()));
            for (const cv::Vec3f &c : circles) {
                LocateCircle2D circle;
                circle.center = QPointF(c[0], c[1]);
                circle.radius = c[2];
                circle.confidence = 1.0;
                circlesOut.append(circle);
            }
            std::sort(circlesOut.begin(), circlesOut.end(),
                      [](const LocateCircle2D &a, const LocateCircle2D &b) {
                          return a.radius > b.radius;
                      });
            if (options.maxCount > 0 && circlesOut.size() > options.maxCount)
                circlesOut.resize(options.maxCount);
            if (!circlesOut.isEmpty()) {
                const double maxR = qMax(1.0, circlesOut.first().radius);
                for (LocateCircle2D &c : circlesOut)
                    c.confidence = qBound(0.0, 1.0, c.radius / maxR);
            }

            for (const LocateCircle2D &c : circlesOut) {
                cv::circle(display,
                           cv::Point(static_cast<int>(std::lround(c.center.x())),
                                     static_cast<int>(std::lround(c.center.y()))),
                           static_cast<int>(std::lround(c.radius)), cv::Scalar(0, 255, 0), 2);
                cv::circle(display,
                           cv::Point(static_cast<int>(std::lround(c.center.x())),
                                     static_cast<int>(std::lround(c.center.y()))),
                           3, cv::Scalar(0, 0, 255), -1);
            }
        }, error)) {
        return false;
    }
    overlay = VisionImage(display, input.sourcePath());
    return true;
}

bool FeatureMatchAlgorithm::apply(const VisionImage &image,
                                  const VisionImage &templ,
                                  QPointF &matchPoint,
                                  double &score,
                                  VisionImage &overlay,
                                  QString *error)
{
    int inliers = 0;
    return apply(image, templ, FeatureMatchOptions{}, matchPoint, score, inliers, overlay, error);
}

bool FeatureMatchAlgorithm::apply(const VisionImage &image,
                                  const VisionImage &templ,
                                  const FeatureMatchOptions &options,
                                  QPointF &matchPoint,
                                  double &score,
                                  int &inlierCount,
                                  VisionImage &overlay,
                                  QString *error)
{
    if (!requireInput(image, error, QStringLiteral("搜索图像为空")))
        return false;
    if (!requireInput(templ, error, QStringLiteral("模板图像为空")))
        return false;

    matchPoint = QPointF();
    score = 0.0;
    inlierCount = 0;
    cv::Mat display;
    if (!Selt::runOpenCv([&]() {
            const cv::Mat grayImage = ensureGray8u(image.mat());
            const cv::Mat grayTempl = ensureGray8u(templ.mat());

            cv::Ptr<cv::ORB> orb = cv::ORB::create(800);
            std::vector<cv::KeyPoint> kpImage;
            std::vector<cv::KeyPoint> kpTempl;
            cv::Mat descImage;
            cv::Mat descTempl;
            orb->detectAndCompute(grayImage, cv::noArray(), kpImage, descImage);
            orb->detectAndCompute(grayTempl, cv::noArray(), kpTempl, descTempl);

            display = toBgrDisplay(image.mat());
            if (descImage.empty() || descTempl.empty() || kpImage.empty() || kpTempl.empty()) {
                score = 0.0;
                matchPoint = QPointF();
                return;
            }

            cv::BFMatcher matcher(cv::NORM_HAMMING);
            std::vector<std::vector<cv::DMatch>> knn;
            matcher.knnMatch(descTempl, descImage, knn, 2);
            std::vector<cv::DMatch> good;
            for (const std::vector<cv::DMatch> &pair : knn) {
                if (pair.size() < 2)
                    continue;
                if (pair[0].distance < options.loweRatio * pair[1].distance)
                    good.push_back(pair[0]);
            }
            if (int(good.size()) < options.minMatches) {
                // Fallback: top-distance matches for compatibility on weak scenes.
                cv::BFMatcher cross(cv::NORM_HAMMING, true);
                cross.match(descTempl, descImage, good);
                std::sort(good.begin(), good.end(),
                          [](const cv::DMatch &a, const cv::DMatch &b) { return a.distance < b.distance; });
                if (good.size() > 20)
                    good.resize(20);
            }
            if (good.empty()) {
                score = 0.0;
                matchPoint = QPointF();
                return;
            }

            std::vector<cv::Point2f> srcPts;
            std::vector<cv::Point2f> dstPts;
            srcPts.reserve(good.size());
            dstPts.reserve(good.size());
            for (const cv::DMatch &m : good) {
                srcPts.push_back(kpTempl.at(size_t(m.queryIdx)).pt);
                dstPts.push_back(kpImage.at(size_t(m.trainIdx)).pt);
            }

            double sumX = 0.0;
            double sumY = 0.0;
            double sumDist = 0.0;
            int used = 0;

#if defined(SELT_HAS_OPENCV_CALIB3D) && SELT_HAS_OPENCV_CALIB3D
            cv::Mat inlierMask;
            cv::Mat H;
            if (srcPts.size() >= 4) {
                try {
                    H = cv::findHomography(srcPts, dstPts, cv::RANSAC,
                                           options.ransacReprojThreshold, inlierMask);
                } catch (const cv::Exception &) {
                    H = cv::Mat();
                    inlierMask = cv::Mat();
                }
            }
            if (!H.empty() && !inlierMask.empty()) {
                for (int i = 0; i < int(dstPts.size()); ++i) {
                    if (inlierMask.at<uchar>(i) == 0)
                        continue;
                    sumX += dstPts[size_t(i)].x;
                    sumY += dstPts[size_t(i)].y;
                    sumDist += good[size_t(i)].distance;
                    ++used;
                    cv::circle(display, dstPts[size_t(i)], 3, cv::Scalar(0, 255, 255), -1);
                }
                // Prefer homography-mapped template center when available.
                std::vector<cv::Point2f> corners{
                    cv::Point2f(0.f, 0.f),
                    cv::Point2f(float(templ.width()), 0.f),
                    cv::Point2f(float(templ.width()), float(templ.height())),
                    cv::Point2f(0.f, float(templ.height()))};
                std::vector<cv::Point2f> mapped;
                cv::perspectiveTransform(corners, mapped, H);
                if (mapped.size() == 4) {
                    matchPoint = QPointF((mapped[0].x + mapped[1].x + mapped[2].x + mapped[3].x) * 0.25,
                                         (mapped[0].y + mapped[1].y + mapped[2].y + mapped[3].y) * 0.25);
                }
            }
#else
            Q_UNUSED(srcPts);
            Q_UNUSED(dstPts);
#endif
            // Fallback / no-calib3d path: robust centroid of the best Lowe matches.
            if (used == 0) {
                for (const cv::DMatch &m : good) {
                    const cv::Point2f &pt = kpImage.at(size_t(m.trainIdx)).pt;
                    sumX += pt.x;
                    sumY += pt.y;
                    sumDist += m.distance;
                    ++used;
                    cv::circle(display, pt, 3, cv::Scalar(0, 255, 255), -1);
                }
                matchPoint = QPointF(sumX / used, sumY / used);
            } else if (!(std::isfinite(matchPoint.x()) && std::isfinite(matchPoint.y()))) {
                matchPoint = QPointF(sumX / used, sumY / used);
            }

            inlierCount = used;
            const double meanDist = sumDist / qMax(1, used);
            const double inlierRatio = double(used) / qMax<size_t>(1, good.size());
            score = qBound(0.0, 1.0, (1.0 - meanDist / 256.0) * 0.5 + inlierRatio * 0.5);
            if (inlierRatio < options.minInlierRatio && used < options.minMatches)
                score = qMin(score, 0.4);

            const int tw = templ.width();
            const int th = templ.height();
            cv::rectangle(display,
                          cv::Rect(static_cast<int>(std::lround(matchPoint.x() - tw * 0.5)),
                                   static_cast<int>(std::lround(matchPoint.y() - th * 0.5)),
                                   tw, th),
                          cv::Scalar(0, 200, 255), 2);
            cv::circle(display,
                       cv::Point(static_cast<int>(std::lround(matchPoint.x())),
                                 static_cast<int>(std::lround(matchPoint.y()))),
                       4, cv::Scalar(0, 0, 255), -1);
        }, error)) {
        return false;
    }
    overlay = VisionImage(display, image.sourcePath());
    return true;
}

bool TemplateMatchMultiAlgorithm::apply(const VisionImage &image,
                                        const VisionImage &templ,
                                        double minScore,
                                        int maxCount,
                                        QVector<QPointF> &peaks,
                                        QVector<double> &scores,
                                        VisionImage &overlay,
                                        QString *error)
{
    TemplateMatchMultiOptions options;
    options.minScore = minScore;
    options.maxCount = maxCount;
    return apply(image, templ, options, peaks, scores, overlay, error);
}

bool TemplateMatchMultiAlgorithm::apply(const VisionImage &image,
                                        const VisionImage &templ,
                                        const TemplateMatchMultiOptions &options,
                                        QVector<QPointF> &peaks,
                                        QVector<double> &scores,
                                        VisionImage &overlay,
                                        QString *error,
                                        QVector<double> *outScales,
                                        QVector<double> *outAngles)
{
    if (!requireInput(image, error, QStringLiteral("搜索图像为空")))
        return false;
    if (!requireInput(templ, error, QStringLiteral("模板图像为空")))
        return false;

    peaks.clear();
    scores.clear();
    if (outScales)
        outScales->clear();
    if (outAngles)
        outAngles->clear();

    QVector<double> scales;
    const double sMin = qMin(options.scaleMin, options.scaleMax);
    const double sMax = qMax(options.scaleMin, options.scaleMax);
    const double sStep = qMax(0.05, options.scaleStep);
    if (qAbs(sMax - sMin) < 1e-6)
        scales.append(sMin <= 0.0 ? 1.0 : sMin);
    else {
        for (double s = sMin; s <= sMax + 1e-6; s += sStep)
            scales.append(s);
        if (scales.isEmpty())
            scales.append(1.0);
    }

    QVector<double> angles;
    const double aMin = qMin(options.angleMin, options.angleMax);
    const double aMax = qMax(options.angleMin, options.angleMax);
    const double aStep = qMax(1.0, options.angleStep);
    if (qAbs(aMax - aMin) < 1e-6)
        angles.append(aMin);
    else {
        for (double a = aMin; a <= aMax + 1e-6; a += aStep)
            angles.append(a);
        if (angles.isEmpty())
            angles.append(0.0);
    }

    cv::Mat display;
    if (!Selt::runOpenCv([&]() {
            const cv::Mat grayImage = ensureGray8u(image.mat());
            const cv::Mat grayTempl = ensureGray8u(templ.mat());
            display = toBgrDisplay(image.mat());

            struct Candidate {
                QPointF center;
                double score{0.0};
                double scale{1.0};
                double angleDeg{0.0};
                cv::Rect box;
            };
            QVector<Candidate> candidates;

            for (double scale : scales) {
                for (double angleDeg : angles) {
                    cv::Mat warped;
                    const cv::Point2f center(grayTempl.cols * 0.5f, grayTempl.rows * 0.5f);
                    cv::Mat rot = cv::getRotationMatrix2D(center, angleDeg, scale);
                    cv::Size warpedSize(int(std::lround(grayTempl.cols * scale)),
                                       int(std::lround(grayTempl.rows * scale)));
                    warpedSize.width = qMax(1, warpedSize.width);
                    warpedSize.height = qMax(1, warpedSize.height);
                    // Keep rotation around template center with scale baked into matrix.
                    rot.at<double>(0, 2) += (warpedSize.width * 0.5) - center.x;
                    rot.at<double>(1, 2) += (warpedSize.height * 0.5) - center.y;
                    cv::warpAffine(grayTempl, warped, rot, warpedSize,
                                   cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0));
                    if (warped.cols < 3 || warped.rows < 3)
                        continue;
                    if (warped.cols > grayImage.cols || warped.rows > grayImage.rows)
                        continue;

                    cv::Mat result;
                    cv::matchTemplate(grayImage, warped, result, cv::TM_CCOEFF_NORMED);
                    cv::Mat work = result.clone();
                    const int tw = warped.cols;
                    const int th = warped.rows;
                    for (int n = 0; n < options.maxCount * 2; ++n) {
                        double maxVal = 0.0;
                        cv::Point maxLoc;
                        cv::minMaxLoc(work, nullptr, &maxVal, nullptr, &maxLoc);
                        if (maxVal < options.minScore)
                            break;
                        Candidate c;
                        c.score = maxVal;
                        c.scale = scale;
                        c.angleDeg = angleDeg;
                        c.center = QPointF(maxLoc.x, maxLoc.y);
                        c.box = cv::Rect(maxLoc.x, maxLoc.y, tw, th);
                        candidates.append(c);
                        const int suppress = qMax(tw, th) / 2;
                        cv::rectangle(work,
                                      cv::Rect(qMax(0, maxLoc.x - suppress),
                                               qMax(0, maxLoc.y - suppress),
                                               suppress * 2 + 1, suppress * 2 + 1)
                                          & cv::Rect(0, 0, work.cols, work.rows),
                                      cv::Scalar(0), cv::FILLED);
                    }
                }
            }

            std::sort(candidates.begin(), candidates.end(),
                      [](const Candidate &a, const Candidate &b) { return a.score > b.score; });

            const double halfDiag = 0.5 * std::hypot(templ.width(), templ.height());
            const double centerNms = options.nmsCenterDistance > 0.0 ? options.nmsCenterDistance
                                                                    : halfDiag;
            QVector<Candidate> kept;
            for (const Candidate &c : candidates) {
                bool suppressed = false;
                for (const Candidate &k : kept) {
                    if (rectIoU(c.box, k.box) > options.nmsIoU)
                        suppressed = true;
                    if (QLineF(c.center, k.center).length() < centerNms)
                        suppressed = true;
                    if (suppressed)
                        break;
                }
                if (!suppressed)
                    kept.append(c);
                if (kept.size() >= options.maxCount)
                    break;
            }

            for (const Candidate &c : kept) {
                peaks.append(c.center);
                scores.append(c.score);
                if (outScales)
                    outScales->append(c.scale);
                if (outAngles)
                    outAngles->append(c.angleDeg);
                cv::rectangle(display, c.box, cv::Scalar(0, 200, 255), 2);
                cv::circle(display,
                           cv::Point(int(std::lround(c.center.x())), int(std::lround(c.center.y()))),
                           3, cv::Scalar(0, 0, 255), -1);
            }
        }, error)) {
        return false;
    }
    overlay = VisionImage(display, image.sourcePath());
    return true;
}
