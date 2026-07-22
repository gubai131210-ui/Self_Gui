#include "vision/algorithms/imagefilteralgorithms.h"

#include <QtGlobal>
#include <opencv2/imgproc.hpp>

bool BlurAlgorithm::apply(const VisionImage &input, VisionImage &output, int ksize, QString *error)
{
    if (input.isEmpty()) {
        if (error) *error = QStringLiteral("输入图像为空");
        return false;
    }
    int k = qMax(1, ksize);
    if (k % 2 == 0) ++k;
    cv::Mat dst;
    cv::blur(input.mat(), dst, cv::Size(k, k));
    output = VisionImage(dst);
    return true;
}

bool CannyAlgorithm::apply(const VisionImage &input, VisionImage &output,
                           double threshold1, double threshold2, QString *error,
                           int apertureSize)
{
    if (input.isEmpty()) {
        if (error) *error = QStringLiteral("输入图像为空");
        return false;
    }
    int aperture = apertureSize;
    if (aperture != 3 && aperture != 5 && aperture != 7)
        aperture = 3;
    cv::Mat gray;
    if (input.channels() == 1)
        gray = input.mat();
    else
        cv::cvtColor(input.mat(), gray, cv::COLOR_BGR2GRAY);
    cv::Mat edges;
    cv::Canny(gray, edges, threshold1, threshold2, aperture);
    output = VisionImage(edges);
    return true;
}

bool MorphologyAlgorithm::apply(const VisionImage &input, VisionImage &output, Op op, int ksize,
                                QString *error, int iterations)
{
    if (input.isEmpty()) {
        if (error) *error = QStringLiteral("输入图像为空");
        return false;
    }
    int k = qMax(1, ksize);
    if (k % 2 == 0) ++k;
    const int iters = qMax(1, iterations);
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(k, k));
    cv::Mat dst;
    switch (op) {
    case Op::Erode: cv::erode(input.mat(), dst, kernel, cv::Point(-1, -1), iters); break;
    case Op::Dilate: cv::dilate(input.mat(), dst, kernel, cv::Point(-1, -1), iters); break;
    case Op::Open: cv::morphologyEx(input.mat(), dst, cv::MORPH_OPEN, kernel, cv::Point(-1, -1), iters); break;
    case Op::Close: cv::morphologyEx(input.mat(), dst, cv::MORPH_CLOSE, kernel, cv::Point(-1, -1), iters); break;
    }
    output = VisionImage(dst);
    return true;
}

bool TemplateMatchAlgorithm::match(const VisionImage &image, const VisionImage &templ,
                                   QPointF *peak, double *score, QString *error)
{
    if (image.isEmpty() || templ.isEmpty()) {
        if (error) *error = QStringLiteral("图像或模板为空");
        return false;
    }
    if (templ.width() > image.width() || templ.height() > image.height()) {
        if (error) *error = QStringLiteral("模板尺寸大于图像");
        return false;
    }
    cv::Mat search = image.mat();
    cv::Mat pattern = templ.mat();
    try {
        if (search.channels() == 3)
            cv::cvtColor(search, search, cv::COLOR_BGR2GRAY);
        else if (search.channels() == 4)
            cv::cvtColor(search, search, cv::COLOR_BGRA2GRAY);
        if (pattern.channels() == 3)
            cv::cvtColor(pattern, pattern, cv::COLOR_BGR2GRAY);
        else if (pattern.channels() == 4)
            cv::cvtColor(pattern, pattern, cv::COLOR_BGRA2GRAY);
        if (search.depth() != CV_8U && search.depth() != CV_32F)
            search.convertTo(search, CV_8U);
        if (pattern.depth() != CV_8U && pattern.depth() != CV_32F)
            pattern.convertTo(pattern, CV_8U);
        if (search.type() != pattern.type())
            pattern.convertTo(pattern, search.type());

        cv::Mat result;
        // Use CCORR_NORMED for stability with constant-color templates.
        cv::matchTemplate(search, pattern, result, cv::TM_CCORR_NORMED);
        double minVal = 0, maxVal = 0;
        cv::Point minLoc, maxLoc;
        cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);
        if (peak)
            *peak = QPointF(maxLoc.x, maxLoc.y);
        if (score)
            *score = maxVal;
        return true;
    } catch (const cv::Exception &exception) {
        if (error)
            *error = QStringLiteral("模板匹配输入格式无效: %1")
                         .arg(QString::fromLocal8Bit(exception.what()));
        return false;
    } catch (const std::exception &exception) {
        if (error)
            *error = QStringLiteral("模板匹配异常: %1")
                         .arg(QString::fromLocal8Bit(exception.what()));
        return false;
    }
}

bool ResizeAlgorithm::apply(const VisionImage &input, VisionImage &output,
                            int width, int height, QString *error)
{
    if (input.isEmpty()) {
        if (error) *error = QStringLiteral("输入图像为空");
        return false;
    }
    if (width < 1 || height < 1) {
        if (error) *error = QStringLiteral("目标尺寸无效");
        return false;
    }
    cv::Mat dst;
    cv::resize(input.mat(), dst, cv::Size(width, height), 0, 0, cv::INTER_LINEAR);
    output = VisionImage(dst);
    return true;
}
