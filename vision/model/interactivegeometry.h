#ifndef INTERACTIVEGEOMETRY_H
#define INTERACTIVEGEOMETRY_H

#include <QString>
#include <QStringList>

namespace Selt {

/// Geometry teaching kind shown on the image canvas.
enum class GeometryKind {
    None,
    NotApplicable, ///< Logic/comms/data nodes with no spatial teaching.
    Roi,           ///< Shared ROI editor (rect/rotated/circle/ellipse/polygon).
    Point,         ///< Single point handle (x,y).
    TwoPoint,      ///< Two points (distance / pair).
    LineSegment,   ///< Expected line segment (x0,y0,x1,y1).
    TwoLineSegment,///< Two line segments for angle / parallel / perpendicular.
    Circle,        ///< Expected circle (cx,cy,radius) + optional arc.
    CaliperStrip,  ///< Oriented caliper strip.
    TemplateRoi,   ///< Template teach rectangle / search ROI.
    ContourPick    ///< Click to select contour / blob candidate.
};

struct InteractiveGeometrySpec
{
    GeometryKind kind{GeometryKind::None};
    QString nodeType;
    bool editable{false};
    /// Keys written back on drag release.
    QStringList parameterKeys;
    /// Whether the shared ROI editor is also enabled for this node.
    bool supportsRoi{false};
    /// Human-readable coverage note for inventory / tests.
    QString coverageNote;
};

inline InteractiveGeometrySpec makeRoiOnlySpec(const QString &nodeType, const QString &note = {})
{
    InteractiveGeometrySpec spec;
    spec.nodeType = nodeType;
    spec.kind = GeometryKind::Roi;
    spec.editable = true;
    spec.supportsRoi = true;
    spec.parameterKeys = {QStringLiteral("roi"), QStringLiteral("extendedRoi")};
    spec.coverageNote = note.isEmpty() ? QStringLiteral("ROI teach") : note;
    return spec;
}

inline InteractiveGeometrySpec makeNotApplicableSpec(const QString &nodeType, const QString &note)
{
    InteractiveGeometrySpec spec;
    spec.nodeType = nodeType;
    spec.kind = GeometryKind::NotApplicable;
    spec.editable = false;
    spec.supportsRoi = false;
    spec.coverageNote = note;
    return spec;
}

/// Full coverage matrix: every vision.* node must resolve to a kind (including NotApplicable).
inline InteractiveGeometrySpec interactiveGeometrySpecForNode(const QString &nodeType)
{
    InteractiveGeometrySpec spec;
    spec.nodeType = nodeType;

    // ---- Geometry teaching tools ----
    if (nodeType == QLatin1String("vision.findLine")) {
        spec.kind = GeometryKind::LineSegment;
        spec.editable = true;
        spec.supportsRoi = true;
        spec.parameterKeys = {QStringLiteral("x0"), QStringLiteral("y0"),
                              QStringLiteral("x1"), QStringLiteral("y1")};
        spec.coverageNote = QStringLiteral("Expected line + caliper preview + ROI");
        return spec;
    }
    if (nodeType == QLatin1String("vision.findCircle")) {
        spec.kind = GeometryKind::Circle;
        spec.editable = true;
        spec.supportsRoi = true;
        spec.parameterKeys = {QStringLiteral("cx"), QStringLiteral("cy"),
                              QStringLiteral("radius")};
        spec.coverageNote = QStringLiteral("Expected circle + radial calipers + ROI");
        return spec;
    }
    if (nodeType == QLatin1String("vision.caliperMeasure")) {
        spec.kind = GeometryKind::CaliperStrip;
        spec.editable = true;
        spec.supportsRoi = true;
        spec.parameterKeys = {QStringLiteral("cx"), QStringLiteral("cy"),
                              QStringLiteral("angle"), QStringLiteral("length"),
                              QStringLiteral("width")};
        spec.coverageNote = QStringLiteral("Caliper strip with rotate/length/width handles");
        return spec;
    }
    if (nodeType == QLatin1String("vision.referencePoint")
        || nodeType == QLatin1String("vision.subpixelRefine")) {
        spec.kind = GeometryKind::Point;
        spec.editable = true;
        spec.supportsRoi = (nodeType == QLatin1String("vision.subpixelRefine"));
        spec.parameterKeys = {QStringLiteral("x"), QStringLiteral("y")};
        spec.coverageNote = QStringLiteral("Single point teach");
        return spec;
    }
    if (nodeType == QLatin1String("vision.distanceMeasure")
        || nodeType == QLatin1String("vision.pointPointDistance")) {
        spec.kind = GeometryKind::TwoPoint;
        spec.editable = true;
        spec.parameterKeys = {QStringLiteral("x1"), QStringLiteral("y1"),
                              QStringLiteral("x2"), QStringLiteral("y2")};
        spec.coverageNote = QStringLiteral("Two-point distance teach");
        return spec;
    }
    if (nodeType == QLatin1String("vision.positionDeviation")) {
        spec.kind = GeometryKind::TwoPoint;
        spec.editable = true;
        spec.parameterKeys = {QStringLiteral("refX"), QStringLiteral("refY")};
        spec.coverageNote = QStringLiteral("Reference point teach (actual from port)");
        return spec;
    }
    if (nodeType == QLatin1String("vision.angleMeasure")) {
        spec.kind = GeometryKind::TwoLineSegment;
        spec.editable = true;
        spec.supportsRoi = true;
        spec.parameterKeys = {QStringLiteral("x1"), QStringLiteral("y1"),
                              QStringLiteral("x2"), QStringLiteral("y2"),
                              QStringLiteral("x3"), QStringLiteral("y3"),
                              QStringLiteral("x4"), QStringLiteral("y4")};
        spec.coverageNote = QStringLiteral("Two line segments for angle");
        return spec;
    }
    if (nodeType == QLatin1String("vision.parallelDistance")
        || nodeType == QLatin1String("vision.perpendicularityMeasure")
        || nodeType == QLatin1String("vision.pointLineDistance")) {
        spec.kind = GeometryKind::TwoLineSegment;
        spec.editable = true;
        spec.supportsRoi = true;
        spec.parameterKeys = {QStringLiteral("x1"), QStringLiteral("y1"),
                              QStringLiteral("x2"), QStringLiteral("y2")};
        spec.coverageNote = QStringLiteral("Line teach / port geometry preview");
        return spec;
    }
    if (nodeType == QLatin1String("vision.fitLine")
        || nodeType == QLatin1String("vision.fitCircle")
        || nodeType == QLatin1String("vision.circularityMeasure")) {
        spec.kind = GeometryKind::ContourPick;
        spec.editable = true;
        spec.supportsRoi = true;
        spec.parameterKeys = {QStringLiteral("contourIndex")};
        spec.coverageNote = QStringLiteral("Contour/blob pick + ROI");
        return spec;
    }
    if (nodeType == QLatin1String("vision.templateMatch")
        || nodeType == QLatin1String("vision.templateSource")
        || nodeType == QLatin1String("vision.templateMatchMulti")) {
        spec.kind = GeometryKind::TemplateRoi;
        spec.editable = true;
        spec.supportsRoi = true;
        spec.parameterKeys = {QStringLiteral("roi"), QStringLiteral("extendedRoi")};
        spec.coverageNote = QStringLiteral("Template teach ROI / search ROI");
        return spec;
    }
    if (nodeType == QLatin1String("vision.houghCircles")
        || nodeType == QLatin1String("vision.circleMeasure")) {
        spec.kind = GeometryKind::Circle;
        spec.editable = true;
        spec.supportsRoi = true;
        spec.parameterKeys = {QStringLiteral("cx"), QStringLiteral("cy"),
                              QStringLiteral("radius")};
        spec.coverageNote = QStringLiteral("Optional expected circle + ROI (radius Auto when unset)");
        return spec;
    }
    if (nodeType == QLatin1String("vision.houghLines")
        || nodeType == QLatin1String("vision.lineMeasure")) {
        spec.kind = GeometryKind::LineSegment;
        spec.editable = true;
        spec.supportsRoi = true;
        spec.parameterKeys = {QStringLiteral("x0"), QStringLiteral("y0"),
                              QStringLiteral("x1"), QStringLiteral("y1")};
        spec.coverageNote = QStringLiteral("Optional expected line + ROI");
        return spec;
    }
    if (nodeType == QLatin1String("vision.rectangleMeasure")) {
        return makeRoiOnlySpec(nodeType, QStringLiteral("Measurement ROI teach"));
    }
    if (nodeType == QLatin1String("vision.blobAnalyze")
        || nodeType == QLatin1String("vision.blobLocate")) {
        return makeRoiOnlySpec(nodeType, QStringLiteral("Blob ROI + candidate pick overlay"));
    }
    if (nodeType == QLatin1String("vision.findContours")
        || nodeType == QLatin1String("vision.connectedComponents")
        || nodeType == QLatin1String("vision.regionFill")) {
        spec.kind = GeometryKind::ContourPick;
        spec.editable = true;
        spec.supportsRoi = true;
        spec.parameterKeys = {QStringLiteral("roi"), QStringLiteral("extendedRoi")};
        spec.coverageNote = QStringLiteral("ROI + contour/component pick");
        return spec;
    }

    // ---- ROI-only preprocess / recognition / locate ----
    static const QStringList kRoiOnly = {
        QStringLiteral("vision.grayscale"),
        QStringLiteral("vision.threshold"),
        QStringLiteral("vision.blur"),
        QStringLiteral("vision.canny"),
        QStringLiteral("vision.morphology"),
        QStringLiteral("vision.barcodeDecode"),
        QStringLiteral("vision.ocr"),
        QStringLiteral("vision.gaussianBlur"),
        QStringLiteral("vision.medianBlur"),
        QStringLiteral("vision.bilateralFilter"),
        QStringLiteral("vision.otsuThreshold"),
        QStringLiteral("vision.adaptiveThreshold"),
        QStringLiteral("vision.sobel"),
        QStringLiteral("vision.colorConvert"),
        QStringLiteral("vision.gammaCorrect"),
        QStringLiteral("vision.contrastBrightness"),
        QStringLiteral("vision.claheEnhance"),
        QStringLiteral("vision.sharpen"),
        QStringLiteral("vision.morphGradient"),
        QStringLiteral("vision.featureMatch"),
    };
    if (kRoiOnly.contains(nodeType))
        return makeRoiOnlySpec(nodeType);

    // ---- Not applicable (logic / IO / non-spatial) ----
    if (nodeType == QLatin1String("vision.imageLoader"))
        return makeNotApplicableSpec(nodeType, QStringLiteral("Image input — no spatial teach"));
    if (nodeType == QLatin1String("vision.resize")
        || nodeType == QLatin1String("vision.geometricTransform")
        || nodeType == QLatin1String("vision.maskCombine"))
        return makeNotApplicableSpec(nodeType, QStringLiteral("Transform / mask — no teach geometry"));
    if (nodeType.startsWith(QLatin1String("vision.bool"))
        || nodeType == QLatin1String("vision.numericCompare")
        || nodeType == QLatin1String("vision.switch")
        || nodeType == QLatin1String("vision.counter")
        || nodeType == QLatin1String("vision.aggregate")
        || nodeType == QLatin1String("vision.variableWrite")
        || nodeType == QLatin1String("vision.rangeDecision")
        || nodeType == QLatin1String("vision.toleranceDecision")
        || nodeType == QLatin1String("vision.measureStatistics")
        || nodeType == QLatin1String("vision.resultPreview")
        || nodeType == QLatin1String("vision.resultWriter")
        || nodeType == QLatin1String("vision.inspectionPack")
        || nodeType == QLatin1String("vision.subflow")
        || nodeType.startsWith(QLatin1String("comm."))
        || nodeType.startsWith(QLatin1String("communication.")))
        return makeNotApplicableSpec(nodeType, QStringLiteral("Logic/IO/comms — NotApplicable"));

    // Unknown vision node: treat as ROI-capable if name suggests image pipeline.
    if (nodeType.startsWith(QLatin1String("vision.")))
        return makeRoiOnlySpec(nodeType, QStringLiteral("Default ROI coverage for vision node"));

    return makeNotApplicableSpec(nodeType, QStringLiteral("Non-vision node"));
}

inline bool isSpatialTeachable(const InteractiveGeometrySpec &spec)
{
    return spec.editable
        && spec.kind != GeometryKind::None
        && spec.kind != GeometryKind::NotApplicable;
}

} // namespace Selt

#endif // INTERACTIVEGEOMETRY_H
