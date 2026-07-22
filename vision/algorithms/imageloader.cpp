#include "imageloader.h"

#include <QFile>
#include <QFileInfo>
#include <exception>
#include <opencv2/imgcodecs.hpp>

bool ImageLoader::load(const QString &filePath, VisionImage &outImage, QString *errorMessage)
{
    if (filePath.trimmed().isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("图片路径为空");
        return false;
    }
    if (!QFileInfo::exists(filePath)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("图片文件不存在: %1").arg(filePath);
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("无法打开图片: %1").arg(file.errorString());
        return false;
    }
    const QByteArray bytes = file.readAll();
    const cv::Mat encoded(1, bytes.size(), CV_8U,
                          const_cast<char *>(bytes.constData()));
    cv::Mat mat;
    try {
        mat = cv::imdecode(encoded, cv::IMREAD_UNCHANGED);
    } catch (const cv::Exception &exception) {
        if (errorMessage)
            *errorMessage = QStringLiteral("图像解码异常: %1")
                                .arg(QString::fromLocal8Bit(exception.what()));
        return false;
    } catch (const std::exception &exception) {
        if (errorMessage)
            *errorMessage = QStringLiteral("图像解码异常: %1")
                                .arg(QString::fromLocal8Bit(exception.what()));
        return false;
    }
    if (mat.empty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("无法读取图片: %1").arg(filePath);
        return false;
    }

    outImage = VisionImage(mat, filePath);
    return true;
}
