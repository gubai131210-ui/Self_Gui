#include "circlemeasurementalgorithm.h"

#include <algorithm>
#include <opencv2/imgproc.hpp>

bool CircleMeasurementAlgorithm::measure(const VisionImage &input,
                                         MeasurementResult &result,
                                         VisionImage &overlayImage,
                                         double minRadius,
                                         bool drawOverlay,
                                         QString *errorMessage)
{
    CircleMeasureOptions options;
    options.minRadius = minRadius;
    options.drawOverlay = drawOverlay;
    options.threshold.strategy = ThresholdStrategy::Manual;
    options.threshold.threshold = 128;
    return measure(input, result, overlayImage, options, errorMessage);
}

bool CircleMeasurementAlgorithm::measure(const VisionImage &input,
                                         MeasurementResult &result,
                                         VisionImage &overlayImage,
                                         CircleMeasureOptions &options,
                                         QString *errorMessage)
{
    if (input.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("测量输入图像为空");
        return false;
    }

    cv::Mat gray;
    const cv::Mat &src = input.mat();
    if (src.channels() == 1)
        gray = src;
    else if (src.channels() == 3)
        cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    else
        cv::cvtColor(src, gray, cv::COLOR_BGRA2GRAY);

    VisionImage tempBinary;
    QString thresholdError;
    if (!ThresholdAlgorithm::apply(VisionImage(gray), tempBinary, options.threshold, &thresholdError)) {
        if (errorMessage)
            *errorMessage = thresholdError;
        return false;
    }
    const cv::Mat binary = tempBinary.mat();

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);
    if (contours.empty()) {
        result.valid = false;
        result.message = QStringLiteral("未找到轮廓（策略=%1,阈值=%2）")
                             .arg(options.threshold.usedStrategy)
                             .arg(options.threshold.usedThreshold);
        if (errorMessage)
            *errorMessage = result.message;
        return false;
    }

    double bestScore = 0.0;
    cv::Point2f bestCenter;
    float bestRadius = 0.0f;
    int bestIndex = -1;
    for (int i = 0; i < static_cast<int>(contours.size()); ++i) {
        if (contours.at(i).size() < 5)
            continue;
        cv::Point2f center;
        float radius = 0.0f;
        cv::minEnclosingCircle(contours.at(i), center, radius);
        if (radius < options.minRadius)
            continue;
        const double area = cv::contourArea(contours.at(i));
        const double circleArea = CV_PI * radius * radius;
        const double score = circleArea > 0.0 ? area / circleArea : 0.0;
        if (score > bestScore) {
            bestScore = score;
            bestCenter = center;
            bestRadius = radius;
            bestIndex = i;
        }
    }

    if (bestIndex < 0) {
        result.valid = false;
        result.message = QStringLiteral("没有满足最小半径的圆（策略=%1,阈值=%2）")
                             .arg(options.threshold.usedStrategy)
                             .arg(options.threshold.usedThreshold);
        if (errorMessage)
            *errorMessage = result.message;
        return false;
    }

    result.valid = true;
    result.unit = QStringLiteral("px");
    result.confidence = std::clamp(bestScore, 0.0, 1.0);
    result.measurementType = QStringLiteral("circle");
    result.decisionState = QStringLiteral("unknown");
    result.width = bestRadius * 2.0;
    result.height = bestRadius * 2.0;
    result.area = CV_PI * bestRadius * bestRadius;
    result.perimeter = 2.0 * CV_PI * bestRadius;
    result.center = QPointF(bestCenter.x, bestCenter.y);
    result.boundingRect = QRectF(bestCenter.x - bestRadius, bestCenter.y - bestRadius,
                                 bestRadius * 2.0, bestRadius * 2.0);
    result.angle = 0.0;
    result.message = QStringLiteral("圆测量成功（策略=%1,阈值=%2）")
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
        cv::circle(display, bestCenter, static_cast<int>(bestRadius), cv::Scalar(0, 255, 0), 2);
        cv::circle(display, bestCenter, 3, cv::Scalar(0, 0, 255), -1);
    }
    overlayImage = VisionImage(display, input.sourcePath());
    return true;
}
