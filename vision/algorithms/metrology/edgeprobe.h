#ifndef EDGEPROBE_H
#define EDGEPROBE_H

#include "vision/algorithms/caliperalgorithm.h"
#include "vision/algorithms/common/geometryquality.h"

namespace Selt {

/// Facade over CaliperAlgorithm for industrial edge probing.
class EdgeProbe
{
public:
    struct ProbeResult
    {
        QVector<QPointF> edges;
        double distance{0.0};
        double confidence{0.0};
        VisionImage overlay;
        QString error;
        bool ok{false};
    };

    static ProbeResult sampleStrip(const VisionImage &input,
                                   const QPointF &center,
                                   double angleDeg,
                                   double length,
                                   double width,
                                   const CaliperOptions &options = {})
    {
        ProbeResult r;
        r.ok = CaliperAlgorithm::sample(input, center, angleDeg, length, width, options, r.edges,
                                        r.distance, r.confidence, r.overlay, &r.error);
        return r;
    }

    static bool findLine(const VisionImage &input,
                         QPointF p0,
                         QPointF p1,
                         const FindLineByCalipersOptions &opts,
                         FitLineResult &result,
                         QVector<QPointF> &edges,
                         VisionImage &overlay,
                         QString *error = nullptr)
    {
        return FindLineByCalipersAlgorithm::apply(input, p0, p1, opts, result, edges, overlay,
                                                  error);
    }

    static bool findCircle(const VisionImage &input,
                           QPointF center,
                           double radius,
                           const FindCircleByCalipersOptions &opts,
                           FitCircleResult &result,
                           QVector<QPointF> &edges,
                           VisionImage &overlay,
                           FindCircleDiagnostics *diagnostics = nullptr,
                           QString *error = nullptr)
    {
        return FindCircleByCalipersAlgorithm::apply(input, center, radius, opts, result, edges,
                                                    overlay, diagnostics, error);
    }

    static double scoreLine(const FitLineResult &fit,
                            const QVector<QPointF> &edges,
                            const QPointF &expectedP0,
                            const QPointF &expectedP1)
    {
        const double coverage =
            GeometryQuality::lineCoverageRatio(edges, expectedP0, expectedP1);
        const double inlierRatio = edges.isEmpty()
            ? 0.0
            : double(fit.inlierCount) / double(edges.size());
        return GeometryQuality::scoreFromResidualCoverage(fit.residualRms, coverage, inlierRatio);
    }

    static double scoreCircle(const FitCircleResult &fit, const QVector<QPointF> &edges)
    {
        const double inlierRatio = edges.isEmpty()
            ? 0.0
            : double(fit.inlierCount) / double(edges.size());
        // Angular coverage approximated by inlier ratio for radial calipers.
        return GeometryQuality::scoreFromResidualCoverage(fit.residualRms, inlierRatio, inlierRatio);
    }
};

} // namespace Selt

#endif // EDGEPROBE_H
