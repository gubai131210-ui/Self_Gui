#ifndef IMAGELOADER_H
#define IMAGELOADER_H

#include "vision/model/visionimage.h"

#include <QString>

class ImageLoader
{
public:
    static bool load(const QString &filePath, VisionImage &outImage, QString *errorMessage = nullptr);
};

#endif // IMAGELOADER_H
