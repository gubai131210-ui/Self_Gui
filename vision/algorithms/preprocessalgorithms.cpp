#include "vision/algorithms/preprocessalgorithms.h"

#include "vision/algorithms/opencvguard.h"

#include <QtGlobal>
#include <opencv2/imgproc.hpp>

namespace {

int ensureOddKernel(int ksize)
{
    int k = qMax(1, ksize);
    if (k % 2 == 0)
        ++k;
    return k;
}

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

cv::Mat toBgrMat(const cv::Mat &src)
{
    cv::Mat bgr;
    if (src.channels() == 1)
        cv::cvtColor(src, bgr, cv::COLOR_GRAY2BGR);
    else if (src.channels() == 3)
        bgr = src;
    else if (src.channels() == 4)
        cv::cvtColor(src, bgr, cv::COLOR_BGRA2BGR);
    else
        cv::cvtColor(src, bgr, cv::COLOR_GRAY2BGR);
    return bgr;
}

} // namespace

bool GaussianBlurAlgorithm::apply(const VisionImage &input,
                                  VisionImage &out,
                                  int ksizeX,
                                  int ksizeY,
                                  double sigmaX,
                                  double sigmaY,
                                  QString *error)
{
    if (!requireInput(input, error))
        return false;

    const int kx = ensureOddKernel(ksizeX);
    const int ky = ensureOddKernel(ksizeY);
    cv::Mat dst;
    if (!Selt::runOpenCv([&]() {
            cv::GaussianBlur(input.mat(), dst, cv::Size(kx, ky), sigmaX, sigmaY);
        }, error)) {
        return false;
    }
    out = VisionImage(dst, input.sourcePath());
    return true;
}

bool MedianBlurAlgorithm::apply(const VisionImage &input,
                                VisionImage &out,
                                int ksize,
                                QString *error)
{
    if (!requireInput(input, error))
        return false;

    const int k = ensureOddKernel(ksize);
    cv::Mat dst;
    if (!Selt::runOpenCv([&]() {
            cv::medianBlur(input.mat(), dst, k);
        }, error)) {
        return false;
    }
    out = VisionImage(dst, input.sourcePath());
    return true;
}

bool BilateralFilterAlgorithm::apply(const VisionImage &input,
                                     VisionImage &out,
                                     int d,
                                     double sigmaColor,
                                     double sigmaSpace,
                                     QString *error)
{
    if (!requireInput(input, error))
        return false;

    const int diameter = qMax(1, d);
    cv::Mat dst;
    if (!Selt::runOpenCv([&]() {
            cv::bilateralFilter(input.mat(), dst, diameter, sigmaColor, sigmaSpace);
        }, error)) {
        return false;
    }
    out = VisionImage(dst, input.sourcePath());
    return true;
}

bool OtsuThresholdAlgorithm::apply(const VisionImage &input,
                                   VisionImage &out,
                                   double maxValue,
                                   bool invert,
                                   QString *error)
{
    if (!requireInput(input, error))
        return false;

    cv::Mat binary;
    if (!Selt::runOpenCv([&]() {
            const cv::Mat gray = toGrayMat(input.mat());
            const int type = invert ? (cv::THRESH_BINARY_INV | cv::THRESH_OTSU)
                                   : (cv::THRESH_BINARY | cv::THRESH_OTSU);
            cv::threshold(gray, binary, 0.0, maxValue, type);
        }, error)) {
        return false;
    }
    out = VisionImage(binary, input.sourcePath());
    return true;
}

bool AdaptiveThresholdAlgorithm::apply(const VisionImage &input,
                                       VisionImage &out,
                                       double maxValue,
                                       Method method,
                                       int blockSize,
                                       double C,
                                       bool invert,
                                       QString *error)
{
    if (!requireInput(input, error))
        return false;

    const int block = ensureOddKernel(blockSize);
    if (block < 3) {
        if (error)
            *error = QStringLiteral("自适应阈值 blockSize 必须为 >=3 的奇数");
        return false;
    }

    cv::Mat binary;
    if (!Selt::runOpenCv([&]() {
            const cv::Mat gray = toGrayMat(input.mat());
            const int adaptiveMethod = (method == Method::AdaptiveGaussian)
                                           ? cv::ADAPTIVE_THRESH_GAUSSIAN_C
                                           : cv::ADAPTIVE_THRESH_MEAN_C;
            const int threshType = invert ? cv::THRESH_BINARY_INV : cv::THRESH_BINARY;
            cv::adaptiveThreshold(gray, binary, maxValue, adaptiveMethod, threshType, block, C);
        }, error)) {
        return false;
    }
    out = VisionImage(binary, input.sourcePath());
    return true;
}

bool SobelAlgorithm::apply(const VisionImage &input,
                           VisionImage &out,
                           int dx,
                           int dy,
                           int ksize,
                           double scale,
                           QString *error)
{
    if (!requireInput(input, error))
        return false;
    if (dx == 0 && dy == 0) {
        if (error)
            *error = QStringLiteral("Sobel 的 dx 与 dy 不能同时为 0");
        return false;
    }

    const int k = ensureOddKernel(ksize);
    cv::Mat abs8u;
    if (!Selt::runOpenCv([&]() {
            const cv::Mat gray = toGrayMat(input.mat());
            cv::Mat grad;
            cv::Sobel(gray, grad, CV_16S, dx, dy, k, scale, 0.0, cv::BORDER_DEFAULT);
            cv::Mat absGrad;
            cv::convertScaleAbs(grad, absGrad);
            abs8u = absGrad;
        }, error)) {
        return false;
    }
    out = VisionImage(abs8u, input.sourcePath());
    return true;
}

bool ColorConvertAlgorithm::apply(const VisionImage &input,
                                  VisionImage &out,
                                  const QString &mode,
                                  QString *error)
{
    if (!requireInput(input, error))
        return false;

    const QString m = mode.trimmed().toLower();
    if (m != QLatin1String("gray") && m != QLatin1String("hsv") && m != QLatin1String("lab")
        && m != QLatin1String("bgr")) {
        if (error)
            *error = QStringLiteral("不支持的颜色模式: %1（可用 gray/hsv/lab/bgr）").arg(mode);
        return false;
    }

    cv::Mat dst;
    if (!Selt::runOpenCv([&]() {
            const cv::Mat &src = input.mat();
            if (m == QLatin1String("gray")) {
                dst = toGrayMat(src).clone();
            } else if (m == QLatin1String("hsv")) {
                const cv::Mat bgr = toBgrMat(src);
                cv::cvtColor(bgr, dst, cv::COLOR_BGR2HSV);
            } else if (m == QLatin1String("lab")) {
                const cv::Mat bgr = toBgrMat(src);
                cv::cvtColor(bgr, dst, cv::COLOR_BGR2Lab);
            } else {
                dst = toBgrMat(src).clone();
            }
        }, error)) {
        return false;
    }
    out = VisionImage(dst, input.sourcePath());
    return true;
}

bool GeometricTransformAlgorithm::apply(const VisionImage &input,
                                        VisionImage &out,
                                        const QString &op,
                                        QString *error)
{
    if (!requireInput(input, error))
        return false;

    const QString o = op.trimmed().toLower();
    int rotateCode = -1;
    int flipCode = 99; // sentinel: no flip
    if (o == QLatin1String("rotate90"))
        rotateCode = cv::ROTATE_90_CLOCKWISE;
    else if (o == QLatin1String("rotate180"))
        rotateCode = cv::ROTATE_180;
    else if (o == QLatin1String("rotate270"))
        rotateCode = cv::ROTATE_90_COUNTERCLOCKWISE;
    else if (o == QLatin1String("flipx"))
        flipCode = 1; // horizontal
    else if (o == QLatin1String("flipy"))
        flipCode = 0; // vertical
    else if (o == QLatin1String("flipxy"))
        flipCode = -1;
    else {
        if (error)
            *error = QStringLiteral("不支持的几何变换: %1").arg(op);
        return false;
    }

    cv::Mat dst;
    if (!Selt::runOpenCv([&]() {
            const cv::Mat &src = input.mat();
            if (rotateCode >= 0)
                cv::rotate(src, dst, rotateCode);
            else
                cv::flip(src, dst, flipCode);
        }, error)) {
        return false;
    }
    out = VisionImage(dst, input.sourcePath());
    return true;
}
