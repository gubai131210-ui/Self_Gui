#include "rectanglemeasurementalgorithm.h"

#include <algorithm>
#include <opencv2/imgproc.hpp>

bool RectangleMeasurementAlgorithm::measure(const VisionImage &input,
                                            MeasurementResult &result,
                                            VisionImage &overlayImage,
                                            double minArea,
                                            bool drawOverlay,
                                            QString *errorMessage)
{
    RectangleMeasureOptions options;
    options.minArea = minArea;
    options.drawOverlay = drawOverlay;
    options.threshold.strategy = ThresholdStrategy::Manual;
    options.threshold.threshold = 128;
    return measure(input, result, overlayImage, options, errorMessage);
}

bool RectangleMeasurementAlgorithm::measure(const VisionImage &input,
                                            MeasurementResult &result,
                                            VisionImage &overlayImage,
                                            RectangleMeasureOptions &options,
                                            QString *errorMessage)
{
    if (input.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("测量输入图像为空");
        return false;
    }

    const cv::Mat &src = input.mat();
    VisionImage tempBinary;
    // Preserve prior behavior for already-binary single-channel images when using Manual.
    if (src.channels() == 1 && options.threshold.strategy == ThresholdStrategy::Manual
        && options.threshold.threshold == 128) {
        tempBinary = VisionImage(src.clone(), input.sourcePath());
        options.threshold.usedStrategy = QStringLiteral("passthrough");
        options.threshold.usedThreshold = -1;
    } else {
        if (!ThresholdAlgorithm::apply(input, tempBinary, options.threshold, errorMessage))
            return false;
    }
    const cv::Mat binary = tempBinary.mat();

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary.clone(), contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);
    if (contours.empty()) {
        result.valid = false;
        result.message = QStringLiteral("未找到轮廓（策略=%1,阈值=%2）")
                             .arg(options.threshold.usedStrategy)
                             .arg(options.threshold.usedThreshold);
        if (errorMessage)
            *errorMessage = result.message;
        return false;
    }

    double bestArea = 0.0;
    int bestIndex = -1;
    for (int i = 0; i < static_cast<int>(contours.size()); ++i) {
        const double area = cv::contourArea(contours.at(i));
        if (area >= options.minArea && area > bestArea) {
            bestArea = area;
            bestIndex = i;
        }
    }

    if (bestIndex < 0) {
        result.valid = false;
        result.message = QStringLiteral("没有满足最小面积的轮廓（策略=%1,阈值=%2）")
                             .arg(options.threshold.usedStrategy)
                             .arg(options.threshold.usedThreshold);
        if (errorMessage)
            *errorMessage = result.message;
        return false;
    }

    const std::vector<cv::Point> &contour = contours.at(bestIndex);
    const cv::RotatedRect rotated = cv::minAreaRect(contour);
    const cv::Rect rect = rotated.boundingRect();
    const double cx = rotated.center.x;
    const double cy = rotated.center.y;
    const double rw = rotated.size.width;
    const double rh = rotated.size.height;

    result.valid = true;
    result.unit = QStringLiteral("px");
    result.confidence = 1.0;
    result.measurementType = QStringLiteral("rectangle");
    result.decisionState = QStringLiteral("unknown");
    result.area = std::max({bestArea, rw * rh, static_cast<double>(rect.width) * rect.height});
    result.perimeter = cv::arcLength(contour, true);
    result.boundingRect = QRectF(rect.x, rect.y, rect.width, rect.height);
    result.center = QPointF(cx, cy);
    result.width = rw;
    result.height = rh;
    result.angle = rotated.angle;
    result.message = QStringLiteral("矩形测量成功（策略=%1,阈值=%2）")
                         .arg(options.threshold.usedStrategy)
                         .arg(options.threshold.usedThreshold);

    cv::Mat display;
    if (src.channels() == 1)
        cv::cvtColor(src, display, cv::COLOR_GRAY2BGR);
    else if (src.channels() == 3)
        display = src.clone();
    else
        cv::cvtColor(src, display, cv::COLOR_BGRA2BGR);
    if (options.drawOverlay) {
        cv::Point2f pts[4];
        rotated.points(pts);
        for (int i = 0; i < 4; ++i)
            cv::line(display, pts[i], pts[(i + 1) % 4], cv::Scalar(0, 255, 0), 2);
    }
    overlayImage = VisionImage(display, input.sourcePath());
    return true;
}
