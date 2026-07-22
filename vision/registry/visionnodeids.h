#ifndef VISIONNODEIDS_H
#define VISIONNODEIDS_H

#include <QString>

namespace VisionNodeIds {
inline QString imageLoader() { return QStringLiteral("vision.imageLoader"); }
inline QString grayscale() { return QStringLiteral("vision.grayscale"); }
inline QString threshold() { return QStringLiteral("vision.threshold"); }
inline QString blur() { return QStringLiteral("vision.blur"); }
inline QString canny() { return QStringLiteral("vision.canny"); }
inline QString morphology() { return QStringLiteral("vision.morphology"); }
inline QString resize() { return QStringLiteral("vision.resize"); }
inline QString templateMatch() { return QStringLiteral("vision.templateMatch"); }
inline QString templateSource() { return QStringLiteral("vision.templateSource"); }
inline QString rectangleMeasure() { return QStringLiteral("vision.rectangleMeasure"); }
inline QString circleMeasure() { return QStringLiteral("vision.circleMeasure"); }
inline QString lineMeasure() { return QStringLiteral("vision.lineMeasure"); }
inline QString distanceMeasure() { return QStringLiteral("vision.distanceMeasure"); }
inline QString rangeDecision() { return QStringLiteral("vision.rangeDecision"); }
inline QString resultPreview() { return QStringLiteral("vision.resultPreview"); }
/// Barcode/QR contract; runtime uses OpenCV QRCodeDetector / optional barcode.
inline QString barcodeDecode() { return QStringLiteral("vision.barcodeDecode"); }
/// OCR via optional Tesseract backend.
inline QString ocr() { return QStringLiteral("vision.ocr"); }

// O1 preprocess
inline QString gaussianBlur() { return QStringLiteral("vision.gaussianBlur"); }
inline QString medianBlur() { return QStringLiteral("vision.medianBlur"); }
inline QString bilateralFilter() { return QStringLiteral("vision.bilateralFilter"); }
inline QString otsuThreshold() { return QStringLiteral("vision.otsuThreshold"); }
inline QString adaptiveThreshold() { return QStringLiteral("vision.adaptiveThreshold"); }
inline QString sobel() { return QStringLiteral("vision.sobel"); }
inline QString colorConvert() { return QStringLiteral("vision.colorConvert"); }
inline QString geometricTransform() { return QStringLiteral("vision.geometricTransform"); }

// O2 region
inline QString maskCombine() { return QStringLiteral("vision.maskCombine"); }
inline QString findContours() { return QStringLiteral("vision.findContours"); }
inline QString connectedComponents() { return QStringLiteral("vision.connectedComponents"); }
inline QString regionFill() { return QStringLiteral("vision.regionFill"); }
inline QString blobAnalyze() { return QStringLiteral("vision.blobAnalyze"); }

// O3 locate
inline QString houghLines() { return QStringLiteral("vision.houghLines"); }
inline QString houghCircles() { return QStringLiteral("vision.houghCircles"); }
inline QString blobLocate() { return QStringLiteral("vision.blobLocate"); }
inline QString featureMatch() { return QStringLiteral("vision.featureMatch"); }
inline QString templateMatchMulti() { return QStringLiteral("vision.templateMatchMulti"); }
inline QString subpixelRefine() { return QStringLiteral("vision.subpixelRefine"); }

// O4 measure
inline QString angleMeasure() { return QStringLiteral("vision.angleMeasure"); }
inline QString parallelDistance() { return QStringLiteral("vision.parallelDistance"); }
inline QString caliperMeasure() { return QStringLiteral("vision.caliperMeasure"); }
inline QString fitCircle() { return QStringLiteral("vision.fitCircle"); }
inline QString fitLine() { return QStringLiteral("vision.fitLine"); }
inline QString measureStatistics() { return QStringLiteral("vision.measureStatistics"); }
inline QString toleranceDecision() { return QStringLiteral("vision.toleranceDecision"); }

// O5 logic/output
inline QString boolAnd() { return QStringLiteral("vision.boolAnd"); }
inline QString boolOr() { return QStringLiteral("vision.boolOr"); }
inline QString boolNot() { return QStringLiteral("vision.boolNot"); }
inline QString numericCompare() { return QStringLiteral("vision.numericCompare"); }
inline QString switchNode() { return QStringLiteral("vision.switch"); }
inline QString counter() { return QStringLiteral("vision.counter"); }
inline QString aggregate() { return QStringLiteral("vision.aggregate"); }
inline QString resultWriter() { return QStringLiteral("vision.resultWriter"); }
inline QString morphGradient() { return QStringLiteral("vision.morphGradient"); }
inline QString pointLineDistance() { return QStringLiteral("vision.pointLineDistance"); }
inline QString circularityMeasure() { return QStringLiteral("vision.circularityMeasure"); }
inline QString pointPointDistance() { return QStringLiteral("vision.pointPointDistance"); }
inline QString perpendicularityMeasure() { return QStringLiteral("vision.perpendicularityMeasure"); }
inline QString positionDeviation() { return QStringLiteral("vision.positionDeviation"); }
inline QString referencePoint() { return QStringLiteral("vision.referencePoint"); }
inline QString variableWrite() { return QStringLiteral("vision.variableWrite"); }
inline QString inspectionPack() { return QStringLiteral("vision.inspectionPack"); }
inline QString subflow() { return QStringLiteral("vision.subflow"); }

inline bool isVisionType(const QString &type)
{
    // Built-in DAG operators: vision + communication share the same runtime path.
    return type.startsWith(QStringLiteral("vision."))
           || type.startsWith(QStringLiteral("communication."));
}
} // namespace VisionNodeIds

#endif // VISIONNODEIDS_H
