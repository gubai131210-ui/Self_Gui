#include "visionimage.h"

#include <opencv2/imgproc.hpp>

VisionImage::VisionImage(const cv::Mat &mat, const QString &sourcePath)
    : m_mat(mat)
    , m_sourcePath(sourcePath)
{
}

bool VisionImage::isEmpty() const
{
    return m_mat.empty();
}

int VisionImage::width() const
{
    return m_mat.empty() ? 0 : m_mat.cols;
}

int VisionImage::height() const
{
    return m_mat.empty() ? 0 : m_mat.rows;
}

int VisionImage::channels() const
{
    return m_mat.empty() ? 0 : m_mat.channels();
}

VisionImage VisionImage::clone() const
{
    VisionImage copy;
    copy.m_mat = m_mat.clone();
    copy.m_sourcePath = m_sourcePath;
    return copy;
}

QImage VisionImage::toQImage() const
{
    if (m_mat.empty())
        return {};

    cv::Mat display;
    if (m_mat.channels() == 1) {
        cv::cvtColor(m_mat, display, cv::COLOR_GRAY2RGB);
    } else if (m_mat.channels() == 3) {
        cv::cvtColor(m_mat, display, cv::COLOR_BGR2RGB);
    } else if (m_mat.channels() == 4) {
        cv::cvtColor(m_mat, display, cv::COLOR_BGRA2RGBA);
    } else {
        return {};
    }

    return QImage(display.data, display.cols, display.rows,
                  static_cast<int>(display.step), QImage::Format_RGB888)
        .copy();
}

VisionImage VisionImage::fromQImage(const QImage &image, const QString &sourcePath)
{
    if (image.isNull())
        return {};

    QImage converted = image.convertToFormat(QImage::Format_RGB888);
    cv::Mat mat(converted.height(), converted.width(), CV_8UC3,
                const_cast<uchar *>(converted.bits()), converted.bytesPerLine());
    cv::Mat bgr;
    cv::cvtColor(mat, bgr, cv::COLOR_RGB2BGR);
    return VisionImage(bgr.clone(), sourcePath);
}
