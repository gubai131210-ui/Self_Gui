#ifndef VISIONIMAGE_H
#define VISIONIMAGE_H

#include <opencv2/core.hpp>

#include <QImage>
#include <QString>

class VisionImage
{
public:
    VisionImage() = default;
    explicit VisionImage(const cv::Mat &mat, const QString &sourcePath = {});

    bool isEmpty() const;
    int width() const;
    int height() const;
    int channels() const;
    QString sourcePath() const { return m_sourcePath; }

    const cv::Mat &mat() const { return m_mat; }
    cv::Mat &mat() { return m_mat; }

    VisionImage clone() const;
    QImage toQImage() const;
    static VisionImage fromQImage(const QImage &image, const QString &sourcePath = {});

private:
    cv::Mat m_mat;
    QString m_sourcePath;
};

#endif // VISIONIMAGE_H
