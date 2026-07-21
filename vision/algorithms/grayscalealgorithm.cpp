#include "grayscalealgorithm.h"

#include <opencv2/imgproc.hpp>

bool GrayscaleAlgorithm::convert(const VisionImage &input, VisionImage &output, QString *errorMessage)
{
    if (input.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("灰度化输入图像为空");
        return false;
    }

    cv::Mat gray;
    const cv::Mat &src = input.mat();
    if (src.channels() == 1) {
        gray = src.clone();
    } else if (src.channels() == 3) {
        cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    } else if (src.channels() == 4) {
        cv::cvtColor(src, gray, cv::COLOR_BGRA2GRAY);
    } else {
        if (errorMessage)
            *errorMessage = QStringLiteral("不支持的图像通道数: %1").arg(src.channels());
        return false;
    }

    output = VisionImage(gray, input.sourcePath());
    return true;
}
