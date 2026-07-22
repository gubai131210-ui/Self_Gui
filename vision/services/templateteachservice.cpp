#include "templateteachservice.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QUuid>

namespace Selt {

bool TemplateTeachService::cropTemplate(const QImage &source,
                                        const ExtendedRoi &roi,
                                        VisionImage *outImage,
                                        QString *error)
{
    if (!outImage) {
        if (error)
            *error = QStringLiteral("输出图像指针为空");
        return false;
    }
    if (source.isNull()) {
        if (error)
            *error = QStringLiteral("源图像为空");
        return false;
    }
    if (!roi.enabled || !roi.isValid()) {
        if (error)
            *error = QStringLiteral("ROI 无效，请先框选有效区域");
        return false;
    }

    QRectF box = roi.boundingRect().intersected(QRectF(0, 0, source.width(), source.height()));
    box = box.normalized();
    if (box.width() < kMinTemplateSize || box.height() < kMinTemplateSize) {
        if (error)
            *error = QStringLiteral("模板尺寸过小（至少 %1x%1）").arg(kMinTemplateSize);
        return false;
    }

    const QRect cropRect = box.toRect();
    if (!cropRect.isValid()) {
        if (error)
            *error = QStringLiteral("ROI 越界或无效");
        return false;
    }

    const QImage cropped = source.copy(cropRect);
    if (cropped.isNull()) {
        if (error)
            *error = QStringLiteral("裁切模板失败");
        return false;
    }
    *outImage = VisionImage::fromQImage(cropped);
    return true;
}

TemplateTeachResult TemplateTeachService::teachFromRoi(const QImage &source,
                                                       const ExtendedRoi &roi,
                                                       ProjectResourceStore &store,
                                                       const QString &displayName)
{
    TemplateTeachResult result;
    VisionImage image;
    if (!cropTemplate(source, roi, &image, &result.error))
        return result;

    QString root = store.projectRoot();
    if (root.isEmpty()) {
        root = QDir::temp().filePath(QStringLiteral("selt_templates"));
        store.setProjectRoot(root);
    }

    const QDir assetsDir(QDir(root).filePath(QStringLiteral("assets/templates")));
    if (!assetsDir.exists() && !QDir().mkpath(assetsDir.absolutePath())) {
        result.error = QStringLiteral("无法创建模板资源目录: %1").arg(assetsDir.absolutePath());
        return result;
    }

    const QString fileName = QStringLiteral("tpl_%1.png")
                                 .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss_zzz")));
    const QString absPath = assetsDir.filePath(fileName);
    if (!image.toQImage().save(absPath, "PNG")) {
        result.error = QStringLiteral("保存模板文件失败: %1").arg(absPath);
        return result;
    }

    ProjectResource resource;
    resource.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    resource.kind = QStringLiteral("template");
    resource.displayName = displayName.isEmpty()
        ? QStringLiteral("模板_%1").arg(QFileInfo(fileName).baseName())
        : displayName;
    resource.relativePath = store.makeRelativePath(absPath);
    if (resource.relativePath.isEmpty() || QFileInfo(resource.relativePath).isAbsolute())
        resource.relativePath = QStringLiteral("assets/templates/%1").arg(fileName);
    resource.notes = QStringLiteral("由 ROI 教示生成");

    result.resourceId = store.addOrUpdate(resource);
    result.relativePath = store.resource(result.resourceId).relativePath;
    result.absolutePath = store.absolutePathFor(result.resourceId);
    result.templateImage = image;
    result.preview = image.toQImage();
    result.ok = true;
    return result;
}

} // namespace Selt
