#include "rectanglemeasurementalgorithm.h"

#include "thresholdalgorithm.h"

#include <opencv2/imgproc.hpp>

bool RectangleMeasurementAlgorithm::measure(const VisionImage &input,
                                            MeasurementResult &result,
                                            VisionImage &overlayImage,
                                            double minArea,
                                            bool drawOverlay,
                                            QString *errorMessage)
{
    if (input.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("测量输入图像为空");
        return false;
    }

    cv::Mat binary;
    const cv::Mat &src = input.mat();
    if (src.channels() == 1) {
        binary = src.clone();
    } else {
        VisionImage tempBinary;
        if (!ThresholdAlgorithm::apply(input, tempBinary, 128, 255, ThresholdMode::Binary, errorMessage))
            return false;
        binary = tempBinary.mat();
    }

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    if (contours.empty()) {
        result.valid = false;
        result.message = QStringLiteral("未找到轮廓");
        if (errorMessage)
            *errorMessage = result.message;
        return false;
    }

    double bestArea = 0.0;
    int bestIndex = -1;
    for (int i = 0; i < static_cast<int>(contours.size()); ++i) {
        const double area = cv::contourArea(contours.at(i));
        if (area >= minArea && area > bestArea) {
            bestArea = area;
            bestIndex = i;
        }
    }

    if (bestIndex < 0) {
        result.valid = false;
        result.message = QStringLiteral("没有满足最小面积的轮廓");
        if (errorMessage)
            *errorMessage = result.message;
        return false;
    }

    const std::vector<cv::Point> &contour = contours.at(bestIndex);
    const cv::Rect rect = cv::boundingRect(contour);
    const cv::Moments moments = cv::moments(contour);
    const double cx = moments.m00 > 0 ? moments.m10 / moments.m00 : rect.x + rect.width / 2.0;
    const double cy = moments.m00 > 0 ? moments.m01 / moments.m00 : rect.y + rect.height / 2.0;

    result.valid = true;
    result.area = bestArea;
    result.perimeter = cv::arcLength(contour, true);
    result.boundingRect = QRectF(rect.x, rect.y, rect.width, rect.height);
    result.center = QPointF(cx, cy);
    result.width = rect.width;
    result.height = rect.height;
    result.angle = 0.0;
    result.message = QStringLiteral("测量成功");

    cv::Mat display;
    if (src.channels() == 1)
        cv::cvtColor(src, display, cv::COLOR_GRAY2BGR);
    else if (src.channels() == 3)
        display = src.clone();
    else
        cv::cvtColor(src, display, cv::COLOR_BGRA2BGR);

    if (drawOverlay) {
        cv::drawContours(display, contours, bestIndex, cv::Scalar(0, 255, 0), 2);
        cv::rectangle(display, rect, cv::Scalar(0, 0, 255), 2);
        cv::circle(display, cv::Point(static_cast<int>(cx), static_cast<int>(cy)), 4, cv::Scalar(255, 0, 0), -1);
    }

    overlayImage = VisionImage(display, input.sourcePath());
    return true;
}
