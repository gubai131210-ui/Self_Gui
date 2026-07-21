#include "thresholdalgorithm.h"

#include <opencv2/imgproc.hpp>

bool ThresholdAlgorithm::apply(const VisionImage &input,
                               VisionImage &output,
                               int threshold,
                               int maxValue,
                               ThresholdMode mode,
                               QString *errorMessage)
{
    if (input.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("二值化输入图像为空");
        return false;
    }

    cv::Mat gray;
    const cv::Mat &src = input.mat();
    if (src.channels() == 1) {
        gray = src;
    } else if (src.channels() == 3) {
        cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    } else if (src.channels() == 4) {
        cv::cvtColor(src, gray, cv::COLOR_BGRA2GRAY);
    } else {
        if (errorMessage)
            *errorMessage = QStringLiteral("二值化不支持的通道数");
        return false;
    }

    const int type = (mode == ThresholdMode::BinaryInv) ? cv::THRESH_BINARY_INV : cv::THRESH_BINARY;
    cv::Mat binary;
    cv::threshold(gray, binary, threshold, maxValue, type);
    output = VisionImage(binary, input.sourcePath());
    return true;
}
