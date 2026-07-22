#ifndef GEOMETRYQUALITY_H
#define GEOMETRYQUALITY_H

#include <QPointF>
#include <QVector>
#include <QtGlobal>
#include <cmath>

namespace Selt {
namespace GeometryQuality {

inline double pointDistance(const QPointF &a, const QPointF &b)
{
    return std::hypot(a.x() - b.x(), a.y() - b.y());
}

/// RMS residual of points to an infinite line through p0→p1.
inline double lineResidualRms(const QVector<QPointF> &points, const QPointF &p0, const QPointF &p1)
{
    if (points.isEmpty())
        return 0.0;
    const QPointF d = p1 - p0;
    const double len = std::hypot(d.x(), d.y());
    if (len < 1e-9)
        return 1e9;
    const double nx = -d.y() / len;
    const double ny = d.x() / len;
    double sumSq = 0.0;
    for (const QPointF &p : points) {
        const double dist = (p.x() - p0.x()) * nx + (p.y() - p0.y()) * ny;
        sumSq += dist * dist;
    }
    return std::sqrt(sumSq / double(points.size()));
}

/// Coverage fraction of an expected segment that has nearby edge samples.
inline double lineCoverageRatio(const QVector<QPointF> &points,
                                const QPointF &p0,
                                const QPointF &p1,
                                double bandPx = 4.0)
{
    if (points.isEmpty())
        return 0.0;
    const QPointF d = p1 - p0;
    const double len = std::hypot(d.x(), d.y());
    if (len < 1e-9)
        return 0.0;
    const int bins = qMax(4, int(std::lround(len / 10.0)));
    QVector<bool> hit(bins, false);
    for (const QPointF &p : points) {
        const QPointF v = p - p0;
        const double t = (v.x() * d.x() + v.y() * d.y()) / (len * len);
        if (t < 0.0 || t > 1.0)
            continue;
        const QPointF proj = p0 + d * t;
        if (pointDistance(p, proj) > bandPx)
            continue;
        const int idx = qBound(0, int(std::floor(t * bins)), bins - 1);
        hit[idx] = true;
    }
    int count = 0;
    for (bool h : hit) {
        if (h)
            ++count;
    }
    return double(count) / double(bins);
}

inline double circleResidualRms(const QVector<QPointF> &points,
                                const QPointF &center,
                                double radius)
{
    if (points.isEmpty() || !(radius > 0.0))
        return 0.0;
    double sumSq = 0.0;
    for (const QPointF &p : points) {
        const double err = pointDistance(p, center) - radius;
        sumSq += err * err;
    }
    return std::sqrt(sumSq / double(points.size()));
}

/// Map residual + coverage + inlier ratio to [0,1] confidence.
inline double scoreFromResidualCoverage(double residualRms,
                                        double coverage,
                                        double inlierRatio,
                                        double residualGood = 1.5,
                                        double residualBad = 6.0)
{
    const double residualScore = residualRms <= residualGood
        ? 1.0
        : (residualRms >= residualBad
               ? 0.0
               : 1.0 - (residualRms - residualGood) / (residualBad - residualGood));
    const double cov = qBound(0.0, coverage, 1.0);
    const double inl = qBound(0.0, inlierRatio, 1.0);
    return qBound(0.0, 0.45 * residualScore + 0.30 * cov + 0.25 * inl, 1.0);
}

inline bool passesSafetyMinPoints(int count, int minPoints)
{
    return count >= qMax(1, minPoints);
}

} // namespace GeometryQuality
} // namespace Selt

#endif // GEOMETRYQUALITY_H
