#include "linemeasurementalgorithm.h"

#include <cmath>
#include <opencv2/imgproc.hpp>

bool LineMeasurementAlgorithm::measure(const VisionImage &input,
                                       MeasurementResult &result,
                                       VisionImage &overlayImage,
                                       int cannyLow,
                                       int cannyHigh,
                                       bool drawOverlay,
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

    cv::Mat edges;
    cv::Canny(gray, edges, cannyLow, cannyHigh);
    std::vector<cv::Vec4i> lines;
    cv::HoughLinesP(edges, lines, 1, CV_PI / 180.0, 40, 20, 10);
    if (lines.empty()) {
        result.valid = false;
        result.message = QStringLiteral("未检测到线段");
        if (errorMessage)
            *errorMessage = result.message;
        return false;
    }

    double bestLength = 0.0;
    cv::Vec4i best = lines.front();
    for (const cv::Vec4i &line : lines) {
        const double dx = line[2] - line[0];
        const double dy = line[3] - line[1];
        const double length = std::hypot(dx, dy);
        if (length > bestLength) {
            bestLength = length;
            best = line;
        }
    }

    const double dx = best[2] - best[0];
    const double dy = best[3] - best[1];
    const double angleDeg = std::atan2(dy, dx) * 180.0 / CV_PI;

    result.valid = true;
    result.unit = QStringLiteral("px");
    result.confidence = 1.0;
    result.measurementType = QStringLiteral("line");
    result.decisionState = QStringLiteral("unknown");
    result.width = bestLength;
    result.height = 0.0;
    result.area = 0.0;
    result.perimeter = bestLength;
    result.angle = angleDeg;
    result.center = QPointF((best[0] + best[2]) * 0.5, (best[1] + best[3]) * 0.5);
    result.boundingRect = QRectF(QPointF(best[0], best[1]), QPointF(best[2], best[3])).normalized();
    result.message = QStringLiteral("线段测量成功");

    cv::Mat display;
    if (src.channels() == 1)
        cv::cvtColor(src, display, cv::COLOR_GRAY2BGR);
    else if (src.channels() == 3)
        display = src.clone();
    else
        cv::cvtColor(src, display, cv::COLOR_BGRA2BGR);
    if (drawOverlay) {
        cv::line(display, cv::Point(best[0], best[1]), cv::Point(best[2], best[3]),
                 cv::Scalar(0, 255, 255), 2);
        cv::circle(display, cv::Point(static_cast<int>(result.center.x()),
                                      static_cast<int>(result.center.y())),
                   3, cv::Scalar(0, 0, 255), -1);
    }
    overlayImage = VisionImage(display, input.sourcePath());
    return true;
}
