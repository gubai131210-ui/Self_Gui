#ifndef TEMPLATETEACHSERVICE_H
#define TEMPLATETEACHSERVICE_H

#include "vision/model/extendedroi.h"
#include "vision/model/visionimage.h"
#include "vision/storage/projectresourcestore.h"

#include <QImage>
#include <QString>

namespace Selt {

struct TemplateTeachResult
{
    bool ok{false};
    QString error;
    QString resourceId;
    QString relativePath;
    QString absolutePath;
    VisionImage templateImage;
    QImage preview;
};

class TemplateTeachService
{
public:
    static constexpr int kMinTemplateSize = 8;

    /// Crop template from image using ROI (axis-aligned bounding box of any shape).
    static bool cropTemplate(const QImage &source,
                             const ExtendedRoi &roi,
                             VisionImage *outImage,
                             QString *error = nullptr);

    /// Crop, save under projectRoot/assets/templates/, and register in store.
    static TemplateTeachResult teachFromRoi(const QImage &source,
                                            const ExtendedRoi &roi,
                                            ProjectResourceStore &store,
                                            const QString &displayName = QString());
};

} // namespace Selt

#endif // TEMPLATETEACHSERVICE_H
