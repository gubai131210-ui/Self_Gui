#include "imageloader.h"

#include <QFileInfo>
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

    const cv::Mat mat = cv::imread(filePath.toStdString(), cv::IMREAD_UNCHANGED);
    if (mat.empty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("无法读取图片: %1").arg(filePath);
        return false;
    }

    outImage = VisionImage(mat, filePath);
    return true;
}
