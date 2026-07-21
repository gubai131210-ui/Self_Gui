#ifndef VISIONNODEIDS_H
#define VISIONNODEIDS_H

#include <QString>

namespace VisionNodeIds {
inline QString imageLoader() { return QStringLiteral("vision.imageLoader"); }
inline QString grayscale() { return QStringLiteral("vision.grayscale"); }
inline QString threshold() { return QStringLiteral("vision.threshold"); }
inline QString rectangleMeasure() { return QStringLiteral("vision.rectangleMeasure"); }
inline QString resultPreview() { return QStringLiteral("vision.resultPreview"); }

inline bool isVisionType(const QString &type)
{
    return type.startsWith(QStringLiteral("vision."));
}
} // namespace VisionNodeIds

#endif // VISIONNODEIDS_H
