#include "vision/algorithms/caliperalgorithm.h"
#include "vision/model/measurementdefinition.h"
#include "vision/nodes/imageexecutorhelpers.h"
#include "vision/registry/visionnodeids.h"
#include "vision/runtime/nodeexecutorregistry.h"
#include "vision/runtime/resultsink.h"

#include <QElapsedTimer>
#include <QLineF>
#include <QtMath>
#include <cmath>
#include <memory>

namespace Selt {
namespace {

class AngleMeasureExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::angleMeasure(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        QElapsedTimer t;
        t.start();
        Line2D l1 = request.inputs.value(QStringLiteral("line1")).toLine();
        Line2D l2 = request.inputs.value(QStringLiteral("line2")).toLine();
        if (l1.start == l1.end) {
            l1.start = QPointF(resolveRealInput(request, QStringLiteral("x1"), 0.0),
                               resolveRealInput(request, QStringLiteral("y1"), 0.0));
            l1.end = QPointF(resolveRealInput(request, QStringLiteral("x2"), 100.0),
                             resolveRealInput(request, QStringLiteral("y2"), 0.0));
        }
        if (l2.start == l2.end) {
            l2.start = QPointF(resolveRealInput(request, QStringLiteral("x3"), 0.0),
                               resolveRealInput(request, QStringLiteral("y3"), 0.0));
            l2.end = QPointF(resolveRealInput(request, QStringLiteral("x4"), 0.0),
                             resolveRealInput(request, QStringLiteral("y4"), 100.0));
        }
        double angle = 0.0;
        QString err;
        if (!AngleMeasureAlgorithm::fromLines(l1.start, l1.end, l2.start, l2.end, angle, &err))
            return failResult(err, DiagnosticCodes::degenerateGeometry(), t.elapsed());
        MeasurementResult m;
        m.valid = true;
        m.measurementType = QStringLiteral("angle");
        m.angle = angle;
        m.unit = QStringLiteral("deg");
        m.confidence = 1.0;
        m.message = QStringLiteral("角度测量成功");
        m = finalizeAngularMeasurement(m, request);
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("angle"), DataValue(angle));
        r.outputs.insert(QStringLiteral("measurement"), DataValue(m));
        r.measurement = m;
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class ParallelDistanceExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::parallelDistance(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &context) override
    {
        QElapsedTimer t;
        t.start();
        Line2D l1 = request.inputs.value(QStringLiteral("line1")).toLine();
        Line2D l2 = request.inputs.value(QStringLiteral("line2")).toLine();
        double distance = 0.0;
        QString err;
        if (!ParallelDistanceAlgorithm::fromLines(l1.start, l1.end, l2.start, l2.end, distance, &err))
            return failResult(err, DiagnosticCodes::degenerateGeometry(), t.elapsed());
        MeasurementResult m;
        m.valid = true;
        m.measurementType = QStringLiteral("parallelDistance");
        m.width = distance;
        m.unit = QStringLiteral("px");
        m.confidence = 1.0;
        m.message = QStringLiteral("平行距测量成功");
        m = finalizeLengthMeasurement(m, request, context);
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("distance"), DataValue(m.width));
        r.outputs.insert(QStringLiteral("measurement"), DataValue(m));
        r.measurement = m;
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class CaliperMeasureExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::caliperMeasure(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &context) override
    {
        QElapsedTimer t;
        t.start();
        QString err;
        QString code;
        VisionImage input = requireImageWithRoi(request, &err, &code);
        if (input.isEmpty())
            return failResult(err, code.isEmpty() ? DiagnosticCodes::imageEmpty() : code, t.elapsed());

        QPointF center = request.inputs.contains(QStringLiteral("center"))
            ? request.inputs.value(QStringLiteral("center")).toPoint2D()
            : QPointF(resolveRealInput(request, QStringLiteral("cx"), 0.0),
                      resolveRealInput(request, QStringLiteral("cy"), 0.0));
        const double length = resolveRealInput(request, QStringLiteral("length"), 80.0);
        const double width = resolveRealInput(request, QStringLiteral("width"), 8.0);
        if (length < 2.0 || width < 1.0)
            return failResult(QStringLiteral("卡尺长度/宽度无效"),
                              DiagnosticCodes::invalidParameter(), t.elapsed());

        QVector<QPointF> edges;
        double distance = 0.0;
        double confidence = 0.0;
        VisionImage overlay;
        CaliperOptions opts;
        opts.sampleStep = resolveRealInput(request, QStringLiteral("sampleStep"), 1.0);
        opts.smoothSigma = resolveRealInput(request, QStringLiteral("smoothSigma"), 0.0);
        opts.polarity = resolveStringParam(request, QStringLiteral("polarity"), QStringLiteral("any"));
        opts.gradientThreshold = resolveRealInput(request, QStringLiteral("gradientThreshold"), 1.0);
        opts.minEdgeGap = resolveRealInput(request, QStringLiteral("minEdgeGap"), 4.0);
        opts.secondPeakRatio = resolveRealInput(request, QStringLiteral("secondPeakRatio"), 0.35);
        if (!CaliperAlgorithm::sample(input, center,
                                      resolveRealInput(request, QStringLiteral("angle"), 0.0),
                                      length, width, opts, edges, distance, confidence, overlay, &err)) {
            const QString diag = err.contains(QStringLiteral("空"))
                ? DiagnosticCodes::imageEmpty()
                : (err.contains(QStringLiteral("无效"))
                       ? DiagnosticCodes::invalidParameter()
                       : DiagnosticCodes::measureFailed());
            return failResult(err, diag, t.elapsed());
        }
        MeasurementResult m;
        m.valid = !edges.isEmpty();
        m.measurementType = QStringLiteral("caliper");
        m.width = distance;
        m.center = edges.isEmpty() ? center : edges.first();
        m.unit = QStringLiteral("px");
        m.confidence = confidence > 0.0 ? confidence : (edges.size() >= 2 ? 1.0 : 0.7);
        m.message = edges.size() >= 2
            ? QStringLiteral("卡尺双边测距成功")
            : QStringLiteral("卡尺仅找到单边边缘");
        m = finalizeLengthMeasurement(m, request, context);
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("image"), DataValue(overlay));
        r.outputs.insert(QStringLiteral("distance"), DataValue(m.width));
        r.outputs.insert(QStringLiteral("measurement"), DataValue(m));
        r.outputs.insert(QStringLiteral("candidateCount"), DataValue(int(edges.size())));
        r.outputs.insert(QStringLiteral("selectedIndex"), DataValue(0));
        r.outputs.insert(QStringLiteral("confidence"), DataValue(m.confidence));
        if (!edges.isEmpty())
            r.outputs.insert(QStringLiteral("point"), DataValue(edges.first()));
        r.measurement = m;
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class FitCircleExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::fitCircle(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &context) override
    {
        QElapsedTimer t;
        t.start();
        ContourList contours = request.inputs.value(QStringLiteral("contours")).toContours();
        QVector<QPointF> points;
        const QString selectMode = resolveStringParam(request, QStringLiteral("contourSelect"),
                                                      QStringLiteral("maxPoints"));
        const int contourIndex = resolveIntParam(request, QStringLiteral("contourIndex"), 0);
        if (!contours.contours.isEmpty()) {
            int best = 0;
            if (selectMode == QLatin1String("index")) {
                best = qBound(0, contourIndex, contours.contours.size() - 1);
            } else {
                for (int i = 1; i < contours.contours.size(); ++i) {
                    if (contours.contours.at(i).points.size() > contours.contours.at(best).points.size())
                        best = i;
                }
            }
            points = contours.contours.at(best).points;
        }
        if (points.isEmpty() && request.inputs.contains(QStringLiteral("point")))
            points.append(request.inputs.value(QStringLiteral("point")).toPoint2D());
        FitOptions fitOpts;
        fitOpts.mode = resolveStringParam(request, QStringLiteral("fitMode"), QStringLiteral("algebraic"));
        fitOpts.residualThreshold = resolveRealInput(request, QStringLiteral("residualThreshold"), 2.0);
        fitOpts.maxIterations = resolveIntParam(request, QStringLiteral("maxIterations"), 100);
        fitOpts.minInlierRatio = resolveRealInput(request, QStringLiteral("minInlierRatio"), 0.6);
        fitOpts.minInliers = resolveIntParam(request, QStringLiteral("minInliers"), 3);
        FitCircleResult fitResult;
        QString err;
        if (!FitCircleAlgorithm::fit(points, fitOpts, fitResult, &err)) {
            const QString diag = err.contains(QStringLiteral("共线")) || err.contains(QStringLiteral("至少"))
                || err.contains(QStringLiteral("内点"))
                ? DiagnosticCodes::degenerateGeometry()
                : DiagnosticCodes::measureFailed();
            return failResult(err, diag, t.elapsed());
        }
        MeasurementResult m;
        m.valid = true;
        m.measurementType = QStringLiteral("fitCircle");
        m.center = fitResult.center;
        m.width = fitResult.radius * 2.0;
        m.height = fitResult.radius * 2.0;
        m.unit = QStringLiteral("px");
        m.confidence = fitResult.confidence;
        m.message = QStringLiteral("拟合圆成功");
        m = finalizeLengthMeasurement(m, request, context);
        Circle2D c;
        c.center = fitResult.center;
        c.radius = m.width * 0.5;
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("circle"), DataValue(c));
        r.outputs.insert(QStringLiteral("center"), DataValue(m.center));
        r.outputs.insert(QStringLiteral("radius"), DataValue(c.radius));
        r.outputs.insert(QStringLiteral("diameter"), DataValue(m.width));
        r.outputs.insert(QStringLiteral("measurement"), DataValue(m));
        r.outputs.insert(QStringLiteral("inlierCount"), DataValue(fitResult.inlierCount));
        r.outputs.insert(QStringLiteral("residualRms"), DataValue(fitResult.residualRms));
        r.outputs.insert(QStringLiteral("confidence"), DataValue(fitResult.confidence));
        r.measurement = m;
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class FitLineExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::fitLine(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &context) override
    {
        QElapsedTimer t;
        t.start();
        ContourList contours = request.inputs.value(QStringLiteral("contours")).toContours();
        QVector<QPointF> points;
        const QString selectMode = resolveStringParam(request, QStringLiteral("contourSelect"),
                                                      QStringLiteral("maxPoints"));
        const int contourIndex = resolveIntParam(request, QStringLiteral("contourIndex"), 0);
        if (!contours.contours.isEmpty()) {
            int best = 0;
            if (selectMode == QLatin1String("index")) {
                best = qBound(0, contourIndex, contours.contours.size() - 1);
            } else {
                for (int i = 1; i < contours.contours.size(); ++i) {
                    if (contours.contours.at(i).points.size() > contours.contours.at(best).points.size())
                        best = i;
                }
            }
            points = contours.contours.at(best).points;
        }
        FitOptions fitOpts;
        fitOpts.mode = resolveStringParam(request, QStringLiteral("fitMode"), QStringLiteral("algebraic"));
        fitOpts.residualThreshold = resolveRealInput(request, QStringLiteral("residualThreshold"), 2.0);
        fitOpts.maxIterations = resolveIntParam(request, QStringLiteral("maxIterations"), 100);
        fitOpts.minInlierRatio = resolveRealInput(request, QStringLiteral("minInlierRatio"), 0.6);
        fitOpts.minInliers = resolveIntParam(request, QStringLiteral("minInliers"), 2);
        FitLineResult fitResult;
        QString err;
        if (!FitLineAlgorithm::fit(points, fitOpts, fitResult, &err)) {
            const QString diag = err.contains(QStringLiteral("退化")) || err.contains(QStringLiteral("至少"))
                || err.contains(QStringLiteral("内点"))
                ? DiagnosticCodes::degenerateGeometry()
                : DiagnosticCodes::measureFailed();
            return failResult(err, diag, t.elapsed());
        }
        Line2D line;
        line.start = fitResult.start;
        line.end = fitResult.end;
        MeasurementResult m;
        m.valid = true;
        m.measurementType = QStringLiteral("fitLine");
        m.angle = fitResult.angleDeg;
        m.width = QLineF(fitResult.start, fitResult.end).length();
        m.center = (fitResult.start + fitResult.end) * 0.5;
        m.unit = QStringLiteral("px");
        m.confidence = fitResult.confidence;
        m.message = QStringLiteral("拟合直线成功");
        m = finalizeLengthMeasurement(m, request, context);
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("line"), DataValue(line));
        r.outputs.insert(QStringLiteral("angle"), DataValue(fitResult.angleDeg));
        r.outputs.insert(QStringLiteral("length"), DataValue(m.width));
        r.outputs.insert(QStringLiteral("measurement"), DataValue(m));
        r.outputs.insert(QStringLiteral("inlierCount"), DataValue(fitResult.inlierCount));
        r.outputs.insert(QStringLiteral("residualRms"), DataValue(fitResult.residualRms));
        r.outputs.insert(QStringLiteral("confidence"), DataValue(fitResult.confidence));
        r.measurement = m;
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class MeasureStatisticsExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::measureStatistics(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        QElapsedTimer t;
        t.start();
        QVector<double> values;
        if (request.inputs.contains(QStringLiteral("value")))
            values.append(request.inputs.value(QStringLiteral("value")).toReal());
        RegionData region = request.inputs.value(QStringLiteral("region")).toRegion();
        for (const RegionStats &s : region.regions)
            values.append(s.area);
        if (values.isEmpty())
            return failResult(QStringLiteral("统计输入为空"), DiagnosticCodes::invalidParameter(), t.elapsed());
        double sum = 0.0;
        double minV = values.first();
        double maxV = values.first();
        for (double v : values) {
            sum += v;
            minV = qMin(minV, v);
            maxV = qMax(maxV, v);
        }
        const double mean = sum / values.size();
        double var = 0.0;
        for (double v : values)
            var += (v - mean) * (v - mean);
        const double stddev = std::sqrt(var / values.size());
        TableData table;
        table.columns = {QStringLiteral("count"), QStringLiteral("mean"), QStringLiteral("min"),
                         QStringLiteral("max"), QStringLiteral("stddev")};
        table.rows.append({values.size(), mean, minV, maxV, stddev});
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("mean"), DataValue(mean));
        r.outputs.insert(QStringLiteral("min"), DataValue(minV));
        r.outputs.insert(QStringLiteral("max"), DataValue(maxV));
        r.outputs.insert(QStringLiteral("stddev"), DataValue(stddev));
        r.outputs.insert(QStringLiteral("table"), DataValue(table));
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class ToleranceDecisionExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::toleranceDecision(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        QElapsedTimer t;
        t.start();
        const double value = resolveRealInput(request, QStringLiteral("value"), 0.0);
        MeasurementDefinition def;
        def.hasTolerance = true;
        def.lower = resolveRealInput(request, QStringLiteral("lower"), 0.0);
        def.upper = resolveRealInput(request, QStringLiteral("upper"), 100.0);
        def.nominal = resolveRealInput(request, QStringLiteral("nominal"), 50.0);
        if (def.lower > def.upper)
            return failResult(QStringLiteral("容差上下限无效"), DiagnosticCodes::invalidParameter(), t.elapsed());
        const QString state = def.evaluateDecision(value);
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("ok"), DataValue(state == QLatin1String("ok")));
        r.outputs.insert(QStringLiteral("state"), DataValue(state));
        MeasurementResult m;
        m.valid = true;
        m.measurementType = QStringLiteral("toleranceDecision");
        m.width = value;
        m.decisionState = state;
        m.message = state == QLatin1String("ok") ? QStringLiteral("合格") : QStringLiteral("不合格");
        r.outputs.insert(QStringLiteral("measurement"), DataValue(m));
        r.measurement = m;
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class PointLineDistanceExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::pointLineDistance(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        QElapsedTimer t;
        t.start();
        const QPointF p = request.inputs.value(QStringLiteral("point")).toPoint2D();
        const Line2D line = request.inputs.value(QStringLiteral("line")).toLine();
        const QLineF qline(line.start, line.end);
        if (qline.length() < 1e-9)
            return failResult(QStringLiteral("直线长度为零"), DiagnosticCodes::degenerateGeometry(), t.elapsed());
        const double dist = qAbs((qline.dy() * (p.x() - qline.x1()) - qline.dx() * (p.y() - qline.y1()))
                                 / qline.length());
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("distance"), DataValue(dist));
        r.outputs.insert(QStringLiteral("ok"), DataValue(true));
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class CircularityMeasureExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::circularityMeasure(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        QElapsedTimer t;
        t.start();
        ContourList contours = request.inputs.value(QStringLiteral("contour")).toContours();
        const int minArea = resolveIntParam(request, QStringLiteral("minArea"), 20);
        double best = 0.0;
        bool found = false;
        for (const ContourData &c : contours.contours) {
            if (c.points.size() < 3)
                continue;
            double area = 0.0;
            double peri = 0.0;
            for (int i = 0; i < c.points.size(); ++i) {
                const QPointF &a = c.points.at(i);
                const QPointF &b = c.points.at((i + 1) % c.points.size());
                area += a.x() * b.y() - b.x() * a.y();
                peri += QLineF(a, b).length();
            }
            area = std::abs(area) * 0.5;
            if (area < minArea || peri < 1e-9)
                continue;
            const double circ = (4.0 * 3.14159265358979323846 * area) / (peri * peri);
            if (!found || circ > best) {
                best = circ;
                found = true;
            }
        }
        if (!found && request.inputs.contains(QStringLiteral("image"))) {
            // no contour: fail softly
            return failResult(QStringLiteral("无有效轮廓"), DiagnosticCodes::noTarget(), t.elapsed());
        }
        if (!found)
            return failResult(QStringLiteral("无有效轮廓"), DiagnosticCodes::noTarget(), t.elapsed());
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("circularity"), DataValue(best));
        r.outputs.insert(QStringLiteral("ok"), DataValue(best > 0.0));
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class PointPointDistanceExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::pointPointDistance(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &context) override
    {
        QElapsedTimer t;
        t.start();
        QPointF p1 = request.inputs.value(QStringLiteral("point1")).toPoint2D(QPointF(qQNaN(), qQNaN()));
        QPointF p2 = request.inputs.value(QStringLiteral("point2")).toPoint2D(QPointF(qQNaN(), qQNaN()));
        if (!std::isfinite(p1.x()) || !std::isfinite(p1.y())
            || !std::isfinite(p2.x()) || !std::isfinite(p2.y())) {
            p1 = QPointF(resolveRealInput(request, QStringLiteral("x1"), 0.0),
                         resolveRealInput(request, QStringLiteral("y1"), 0.0));
            p2 = QPointF(resolveRealInput(request, QStringLiteral("x2"), 100.0),
                         resolveRealInput(request, QStringLiteral("y2"), 0.0));
        }
        const double distance = QLineF(p1, p2).length();
        if (!std::isfinite(distance))
            return failResult(QStringLiteral("点距无效"), DiagnosticCodes::degenerateGeometry(), t.elapsed());
        MeasurementResult m;
        m.valid = true;
        m.measurementType = QStringLiteral("pointPointDistance");
        m.width = distance;
        m.perimeter = distance;
        m.center = (p1 + p2) * 0.5;
        m.unit = QStringLiteral("px");
        m.confidence = 1.0;
        m.message = QStringLiteral("点到点距离测量成功");
        m = finalizeLengthMeasurement(m, request, context);
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("distance"), DataValue(m.width));
        r.outputs.insert(QStringLiteral("measurement"), DataValue(m));
        r.measurement = m;
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class PerpendicularityMeasureExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::perpendicularityMeasure(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        QElapsedTimer t;
        t.start();
        Line2D l1 = request.inputs.value(QStringLiteral("line1")).toLine();
        Line2D l2 = request.inputs.value(QStringLiteral("line2")).toLine();
        double angle = 0.0;
        double deviation = 0.0;
        QString err;
        if (!PerpendicularityAlgorithm::fromLines(l1.start, l1.end, l2.start, l2.end, angle, deviation, &err))
            return failResult(err, DiagnosticCodes::degenerateGeometry(), t.elapsed());
        MeasurementResult m;
        m.valid = true;
        m.measurementType = QStringLiteral("perpendicularity");
        m.angle = deviation;
        m.unit = QStringLiteral("deg");
        m.confidence = 1.0;
        m.message = QStringLiteral("垂直度偏差测量成功");
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("angle"), DataValue(angle));
        r.outputs.insert(QStringLiteral("deviation"), DataValue(deviation));
        r.outputs.insert(QStringLiteral("measurement"), DataValue(m));
        r.measurement = m;
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class PositionDeviationExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::positionDeviation(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &context) override
    {
        QElapsedTimer t;
        t.start();
        QPointF actual = request.inputs.value(QStringLiteral("point")).toPoint2D();
        QPointF reference = request.inputs.value(QStringLiteral("reference"))
                                .toPoint2D(QPointF(qQNaN(), qQNaN()));
        if (!std::isfinite(reference.x()) || !std::isfinite(reference.y())) {
            reference = QPointF(resolveRealInput(request, QStringLiteral("refX"), 0.0),
                                resolveRealInput(request, QStringLiteral("refY"), 0.0));
        }
        double dx = 0.0;
        double dy = 0.0;
        double distance = 0.0;
        QString err;
        if (!PositionDeviationAlgorithm::fromPoints(actual, reference, dx, dy, distance, &err))
            return failResult(err.isEmpty() ? QStringLiteral("位置偏差无效") : err,
                              DiagnosticCodes::degenerateGeometry(), t.elapsed());
        MeasurementResult m;
        m.valid = true;
        m.measurementType = QStringLiteral("positionDeviation");
        m.width = distance;
        m.center = actual;
        m.unit = QStringLiteral("px");
        m.confidence = 1.0;
        m.message = QStringLiteral("位置偏差测量成功");
        m = finalizeLengthMeasurement(m, request, context);
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("dx"), DataValue(dx));
        r.outputs.insert(QStringLiteral("dy"), DataValue(dy));
        r.outputs.insert(QStringLiteral("distance"), DataValue(m.width));
        r.outputs.insert(QStringLiteral("measurement"), DataValue(m));
        r.measurement = m;
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class ReferencePointExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::referencePoint(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        QElapsedTimer t;
        t.start();
        QPointF point = request.inputs.value(QStringLiteral("point")).toPoint2D(QPointF(qQNaN(), qQNaN()));
        if (!request.inputs.contains(QStringLiteral("point")))
            point = request.inputs.value(QStringLiteral("in")).toPoint2D(QPointF(qQNaN(), qQNaN()));
        if (!std::isfinite(point.x()) || !std::isfinite(point.y())) {
            point = QPointF(resolveRealInput(request, QStringLiteral("x"), 0.0),
                            resolveRealInput(request, QStringLiteral("y"), 0.0));
        }
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("point"), DataValue(point));
        r.outputs.insert(QStringLiteral("x"), DataValue(point.x()));
        r.outputs.insert(QStringLiteral("y"), DataValue(point.y()));
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class VariableWriteExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::variableWrite(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &context) override
    {
        QElapsedTimer t;
        t.start();
        const QString key = resolveStringParam(request, QStringLiteral("key"), QStringLiteral("result"));
        if (key.trimmed().isEmpty())
            return failResult(QStringLiteral("变量键为空"), DiagnosticCodes::invalidParameter(), t.elapsed());
        DataValue value = request.inputs.value(QStringLiteral("value"));
        if (value.isNull()) {
            if (request.typedParameters.contains(QStringLiteral("value")))
                value = request.typedParameters.value(QStringLiteral("value"));
            else if (request.parameters.contains(QStringLiteral("value")))
                value = DataValue(request.parameters.value(QStringLiteral("value")).toDouble());
        }
        context.setVariable(key, value);
        if (context.variableScopeSnapshot())
            context.variableScopeSnapshot()->runtimeWrite(key, value);
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("out"), value);
        r.outputs.insert(QStringLiteral("value"), value);
        r.outputs.insert(QStringLiteral("key"), DataValue(key));
        r.outputs.insert(QStringLiteral("ok"), DataValue(true));
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class InspectionPackExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::inspectionPack(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        QElapsedTimer t;
        t.start();
        const bool hasOk = request.inputs.contains(QStringLiteral("ok"));
        const bool ok = hasOk
            ? request.inputs.value(QStringLiteral("ok")).toBool(false)
            : (resolveStringParam(request, QStringLiteral("state"), QStringLiteral("unknown"))
               == QLatin1String("ok"));
        const double value = resolveRealInput(request, QStringLiteral("value"), 0.0);
        const QString state = ok ? QStringLiteral("ok") : QStringLiteral("ng");
        const QString message = resolveStringParam(request, QStringLiteral("message"),
                                                   ok ? QStringLiteral("合格") : QStringLiteral("不合格"));
        const InspectionResult inspection = InspectionResult::fromDecision(ok, value, message);
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("pass"), DataValue(inspection.ok));
        r.outputs.insert(QStringLiteral("ok"), DataValue(inspection.ok));
        r.outputs.insert(QStringLiteral("resultValue"), DataValue(inspection.value));
        r.outputs.insert(QStringLiteral("value"), DataValue(inspection.value));
        r.outputs.insert(QStringLiteral("state"), DataValue(inspection.state));
        r.outputs.insert(QStringLiteral("message"), DataValue(inspection.message));
        r.outputs.insert(QStringLiteral("measurement"), DataValue(inspection.measurement));
        r.measurement = inspection.measurement;
        r.measurement.decisionState = state;
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

} // namespace

void registerAdvancedMeasureExecutors(NodeExecutorRegistry &reg)
{
    reg.registerFactory(VisionNodeIds::angleMeasure(), [] { return std::make_shared<AngleMeasureExecutor>(); });
    reg.registerFactory(VisionNodeIds::parallelDistance(), [] { return std::make_shared<ParallelDistanceExecutor>(); });
    reg.registerFactory(VisionNodeIds::caliperMeasure(), [] { return std::make_shared<CaliperMeasureExecutor>(); });
    reg.registerFactory(VisionNodeIds::fitCircle(), [] { return std::make_shared<FitCircleExecutor>(); });
    reg.registerFactory(VisionNodeIds::fitLine(), [] { return std::make_shared<FitLineExecutor>(); });
    reg.registerFactory(VisionNodeIds::measureStatistics(), [] { return std::make_shared<MeasureStatisticsExecutor>(); });
    reg.registerFactory(VisionNodeIds::toleranceDecision(), [] { return std::make_shared<ToleranceDecisionExecutor>(); });
    reg.registerFactory(VisionNodeIds::pointLineDistance(), [] { return std::make_shared<PointLineDistanceExecutor>(); });
    reg.registerFactory(VisionNodeIds::circularityMeasure(), [] { return std::make_shared<CircularityMeasureExecutor>(); });
    reg.registerFactory(VisionNodeIds::pointPointDistance(), [] { return std::make_shared<PointPointDistanceExecutor>(); });
    reg.registerFactory(VisionNodeIds::perpendicularityMeasure(), [] { return std::make_shared<PerpendicularityMeasureExecutor>(); });
    reg.registerFactory(VisionNodeIds::positionDeviation(), [] { return std::make_shared<PositionDeviationExecutor>(); });
    reg.registerFactory(VisionNodeIds::referencePoint(), [] { return std::make_shared<ReferencePointExecutor>(); });
    reg.registerFactory(VisionNodeIds::variableWrite(), [] { return std::make_shared<VariableWriteExecutor>(); });
    reg.registerFactory(VisionNodeIds::inspectionPack(), [] { return std::make_shared<InspectionPackExecutor>(); });
}

} // namespace Selt
