#include "thresholdalgorithm.h"

#include <QtGlobal>
#include <opencv2/imgproc.hpp>

ThresholdStrategy ThresholdAlgorithm::parseStrategy(const QString &name)
{
    const QString n = name.trimmed().toLower();
    if (n == QLatin1String("otsu"))
        return ThresholdStrategy::Otsu;
    if (n == QLatin1String("adaptive"))
        return ThresholdStrategy::Adaptive;
    return ThresholdStrategy::Manual;
}

QString ThresholdAlgorithm::strategyName(ThresholdStrategy strategy)
{
    switch (strategy) {
    case ThresholdStrategy::Otsu:
        return QStringLiteral("otsu");
    case ThresholdStrategy::Adaptive:
        return QStringLiteral("adaptive");
    case ThresholdStrategy::Manual:
    default:
        return QStringLiteral("manual");
    }
}

bool ThresholdAlgorithm::apply(const VisionImage &input,
                               VisionImage &output,
                               int threshold,
                               int maxValue,
                               ThresholdMode mode,
                               QString *errorMessage)
{
    ThresholdOptions options;
    options.strategy = ThresholdStrategy::Manual;
    options.threshold = threshold;
    options.maxValue = maxValue;
    options.mode = mode;
    return apply(input, output, options, errorMessage);
}

bool ThresholdAlgorithm::apply(const VisionImage &input,
                               VisionImage &output,
                               ThresholdOptions &options,
                               QString *errorMessage)
{
    options.usedThreshold = -1;
    options.usedStrategy = strategyName(options.strategy);
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
    if (gray.depth() != CV_8U)
        gray.convertTo(gray, CV_8U);

    const int type = (options.mode == ThresholdMode::BinaryInv) ? cv::THRESH_BINARY_INV
                                                                : cv::THRESH_BINARY;
    cv::Mat binary;
    if (options.strategy == ThresholdStrategy::Adaptive) {
        int block = options.adaptiveBlockSize;
        if (block % 2 == 0)
            ++block;
        block = qMax(3, block);
        cv::adaptiveThreshold(gray, binary, options.maxValue, cv::ADAPTIVE_THRESH_GAUSSIAN_C,
                              type, block, options.adaptiveC);
        options.usedThreshold = -1;
    } else if (options.strategy == ThresholdStrategy::Otsu) {
        options.usedThreshold = int(cv::threshold(gray, binary, 0, options.maxValue,
                                                  type | cv::THRESH_OTSU));
    } else {
        options.usedThreshold = options.threshold;
        cv::threshold(gray, binary, options.threshold, options.maxValue, type);
    }
    output = VisionImage(binary, input.sourcePath());
    return true;
}
