#include "vision/algorithms/locatealgorithms.h"
#include "vision/algorithms/regionalgorithms.h"
#include "vision/algorithms/subpixelalgorithms.h"
#include "vision/model/operatoroutcome.h"
#include "vision/model/overlayitems.h"
#include "vision/nodes/imageexecutorhelpers.h"
#include "vision/registry/visionnodeids.h"
#include "vision/runtime/nodeexecutorregistry.h"
#include "vision/storage/projectresourcestore.h"
#include "vision/algorithms/imageloader.h"

#include <QElapsedTimer>
#include <QLineF>
#include <cmath>
#include <memory>

namespace Selt {
namespace {

VisionImage loadTemplateImage(const ExecutionRequest &request, ExecutionContext &context, QString *error)
{
    if (request.inputs.contains(QStringLiteral("template"))) {
        VisionImage templ = request.inputs.value(QStringLiteral("template")).toImage();
        if (!templ.isEmpty())
            return templ;
    }
    if (context.resourceStore()) {
        const QString rid = resolveStringParam(request, QStringLiteral("templateResourceId"));
        if (!rid.isEmpty()) {
            const ProjectResource resource = context.resourceStore()->resource(rid);
            if (resource.exists) {
                VisionImage templ;
                if (ImageLoader::load(resource.absolutePath, templ, error))
                    return templ;
            }
        }
    }
    const QString path = resolveStringParam(request, QStringLiteral("templatePath"));
    VisionImage templ;
    if (!path.isEmpty() && ImageLoader::load(path, templ, error))
        return templ;
    if (error && error->isEmpty())
        *error = QStringLiteral("缺少模板图像");
    return {};
}

class HoughLinesExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::houghLines(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        QElapsedTimer t;
        t.start();
        VisionImage input = applyRoiFromRequest(request.inputs.value(QStringLiteral("image")).toImage(),
                                                request.parameters);
        QVector<LocateLine2D> lines;
        VisionImage overlay;
        QString err;
        HoughLinesOptions opts;
        const double diag = std::hypot(double(input.width()), double(input.height()));
        opts.cannyLow = resolveAutoReal(request, QStringLiteral("cannyLow"), 50.0, 50.0);
        opts.cannyHigh = resolveAutoReal(request, QStringLiteral("cannyHigh"), 150.0, 150.0);
        opts.accumulatorThreshold =
            resolveAutoInt(request, QStringLiteral("accumulatorThreshold"), 50,
                           qMax(20, int(std::lround(diag * 0.04))));
        opts.minLineLength =
            resolveAutoReal(request, QStringLiteral("minLineLength"), 30.0, qMax(20.0, diag * 0.08));
        opts.maxLineGap = resolveAutoReal(request, QStringLiteral("maxLineGap"), 10.0, 10.0);
        opts.maxCount = resolveAutoInt(request, QStringLiteral("maxCount"), 50, 50);
        opts.sortBy = resolveStringParam(request, QStringLiteral("sortBy"), QStringLiteral("length"));
        if (!HoughLinesAlgorithm::apply(input, lines, opts, overlay, &err)) {
            ExecutionResult r = failResult(err, DiagnosticCodes::noCandidate(), t.elapsed());
            OperatorResultMeta meta;
            meta.outcome = OperatorOutcome::NoCandidate;
            meta.failureStage = QStringLiteral("hough");
            meta.message = err;
            applyOperatorMeta(r, meta);
            return r;
        }
        int selected = 0;
        if (request.parameters.contains(QStringLiteral("pickX"))
            && request.parameters.contains(QStringLiteral("pickY")) && !lines.isEmpty()) {
            const QPointF pick(request.parameters.value(QStringLiteral("pickX")).toDouble(),
                               request.parameters.value(QStringLiteral("pickY")).toDouble());
            double best = 1e100;
            for (int i = 0; i < lines.size(); ++i) {
                const QPointF mid = (lines.at(i).start + lines.at(i).end) * 0.5;
                const double d = QLineF(pick, mid).length();
                if (d < best) {
                    best = d;
                    selected = i;
                }
            }
        }
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("image"), DataValue(overlay));
        r.outputs.insert(QStringLiteral("count"), DataValue(int(lines.size())));
        r.outputs.insert(QStringLiteral("candidateCount"), DataValue(int(lines.size())));
        r.outputs.insert(QStringLiteral("selectedIndex"), DataValue(lines.isEmpty() ? -1 : selected));
        OverlayList overlays;
        for (int i = 0; i < lines.size(); ++i) {
            OverlayItem item;
            item.type = OverlayItemType::Line;
            item.points = {lines.at(i).start, lines.at(i).end};
            item.color = (i == selected) ? QColor(0, 220, 255) : QColor(255, 180, 40);
            item.penWidth = (i == selected) ? 2.5 : 1.5;
            overlays.append(item);
        }
        r.overlays = overlays;
        r.outputs.insert(QStringLiteral("overlay"), DataValue(overlays));
        if (!lines.isEmpty()) {
            Line2D line;
            line.start = lines.at(selected).start;
            line.end = lines.at(selected).end;
            r.outputs.insert(QStringLiteral("line"), DataValue(line));
            r.outputs.insert(QStringLiteral("confidence"), DataValue(lines.at(selected).confidence));
        }
        OperatorResultMeta meta;
        meta.outcome = lines.isEmpty() ? OperatorOutcome::NoCandidate : OperatorOutcome::Success;
        meta.qualityScore = lines.isEmpty() ? 0.0 : lines.at(selected).confidence;
        QJsonObject snap;
        snap.insert(QStringLiteral("accumulatorThreshold"), opts.accumulatorThreshold);
        snap.insert(QStringLiteral("minLineLength"), opts.minLineLength);
        snap.insert(QStringLiteral("candidateCount"), lines.size());
        meta.autoSnapshot = makeAutoSnapshot(snap);
        r.elapsedMs = t.elapsed();
        applyOperatorMeta(r, meta);
        return r;
    }
};

class HoughCirclesExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::houghCircles(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        QElapsedTimer t;
        t.start();
        VisionImage input = applyRoiFromRequest(request.inputs.value(QStringLiteral("image")).toImage(),
                                                request.parameters);
        QVector<LocateCircle2D> circles;
        VisionImage overlay;
        QString err;
        HoughCirclesOptions opts;
        const double shortSide = qMin(double(input.width()), double(input.height()));
        opts.minRadius = resolveAutoReal(request, QStringLiteral("minRadius"), 5.0, qMax(3.0, shortSide * 0.02));
        opts.maxRadius = resolveAutoReal(request, QStringLiteral("maxRadius"), 0.0, shortSide * 0.5);
        opts.dp = resolveAutoReal(request, QStringLiteral("dp"), 1.2, 1.2);
        opts.minDist = resolveAutoReal(request, QStringLiteral("minDist"), 0.0, qMax(10.0, shortSide * 0.08));
        opts.param1 = resolveAutoReal(request, QStringLiteral("param1"), 100.0, 100.0);
        opts.param2 = resolveAutoReal(request, QStringLiteral("param2"), 30.0, 28.0);
        opts.maxCount = resolveAutoInt(request, QStringLiteral("maxCount"), 50, 50);
        if (!HoughCirclesAlgorithm::apply(input, circles, opts, overlay, &err)) {
            ExecutionResult r = failResult(err, DiagnosticCodes::noCandidate(), t.elapsed());
            OperatorResultMeta meta;
            meta.outcome = OperatorOutcome::NoCandidate;
            meta.failureStage = QStringLiteral("hough");
            meta.message = err;
            applyOperatorMeta(r, meta);
            return r;
        }
        int selected = 0;
        if (request.parameters.contains(QStringLiteral("pickX"))
            && request.parameters.contains(QStringLiteral("pickY")) && !circles.isEmpty()) {
            const QPointF pick(request.parameters.value(QStringLiteral("pickX")).toDouble(),
                               request.parameters.value(QStringLiteral("pickY")).toDouble());
            double best = 1e100;
            for (int i = 0; i < circles.size(); ++i) {
                const double d = QLineF(pick, circles.at(i).center).length();
                if (d < best) {
                    best = d;
                    selected = i;
                }
            }
        }
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("image"), DataValue(overlay));
        r.outputs.insert(QStringLiteral("count"), DataValue(int(circles.size())));
        r.outputs.insert(QStringLiteral("candidateCount"), DataValue(int(circles.size())));
        r.outputs.insert(QStringLiteral("selectedIndex"),
                         DataValue(circles.isEmpty() ? -1 : selected));
        OverlayList overlays;
        for (int i = 0; i < circles.size(); ++i) {
            OverlayItem item;
            item.type = OverlayItemType::Contour;
            QVector<QPointF> arc;
            const auto &cc = circles.at(i);
            for (int k = 0; k < 48; ++k) {
                const double a = (2.0 * 3.141592653589793 * k) / 48.0;
                arc.append(QPointF(cc.center.x() + cc.radius * std::cos(a),
                                   cc.center.y() + cc.radius * std::sin(a)));
            }
            item.points = arc;
            item.color = (i == selected) ? QColor(0, 220, 255) : QColor(255, 180, 40);
            item.penWidth = (i == selected) ? 2.5 : 1.5;
            overlays.append(item);
        }
        r.overlays = overlays;
        r.outputs.insert(QStringLiteral("overlay"), DataValue(overlays));
        if (!circles.isEmpty()) {
            Circle2D c;
            c.center = circles.at(selected).center;
            c.radius = circles.at(selected).radius;
            r.outputs.insert(QStringLiteral("circle"), DataValue(c));
            r.outputs.insert(QStringLiteral("center"), DataValue(c.center));
            r.outputs.insert(QStringLiteral("radius"), DataValue(c.radius));
            r.outputs.insert(QStringLiteral("confidence"),
                             DataValue(circles.at(selected).confidence));
        }
        OperatorResultMeta meta;
        meta.outcome = circles.isEmpty() ? OperatorOutcome::NoCandidate : OperatorOutcome::Success;
        meta.qualityScore = circles.isEmpty() ? 0.0 : circles.at(selected).confidence;
        QJsonObject snap;
        snap.insert(QStringLiteral("minRadius"), opts.minRadius);
        snap.insert(QStringLiteral("maxRadius"), opts.maxRadius);
        snap.insert(QStringLiteral("param2"), opts.param2);
        snap.insert(QStringLiteral("candidateCount"), circles.size());
        meta.autoSnapshot = makeAutoSnapshot(snap);
        r.elapsedMs = t.elapsed();
        applyOperatorMeta(r, meta);
        return r;
    }
};

class BlobLocateExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::blobLocate(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        QElapsedTimer t;
        t.start();
        QString err;
        QString code;
        QPointF origin;
        VisionImage input = requireImageWithRoi(request, &origin, &err, &code);
        if (input.isEmpty())
            return failResult(err, code.isEmpty() ? DiagnosticCodes::imageEmpty() : code, t.elapsed());
        RegionData region;
        QVector<double> scores;
        VisionImage overlay;
        int selectedIndex = -1;
        int unfiltered = 0;
        BlobAnalyzeAlgorithm::Options opts;
        opts.minArea = resolveRealInput(request, QStringLiteral("minArea"), 0.0);
        opts.maxArea = resolveRealInput(request, QStringLiteral("maxArea"), 0.0);
        opts.minCircularity = resolveRealInput(request, QStringLiteral("minCircularity"), 0.0);
        opts.maxCircularity = resolveRealInput(request, QStringLiteral("maxCircularity"), 1.0);
        opts.minAspectRatio = resolveRealInput(request, QStringLiteral("minAspectRatio"), 0.0);
        opts.maxAspectRatio = resolveRealInput(request, QStringLiteral("maxAspectRatio"), 0.0);
        opts.minExtent = resolveRealInput(request, QStringLiteral("minExtent"), 0.0);
        opts.minSolidity = resolveRealInput(request, QStringLiteral("minSolidity"), 0.0);
        opts.minPerimeter = resolveRealInput(request, QStringLiteral("minPerimeter"), 0.0);
        opts.maxPerimeter = resolveRealInput(request, QStringLiteral("maxPerimeter"), 0.0);
        opts.minElongation = resolveRealInput(request, QStringLiteral("minElongation"), 1.0);
        opts.maxElongation = resolveRealInput(request, QStringLiteral("maxElongation"), 0.0);
        opts.minCx = resolveRealInput(request, QStringLiteral("minCx"), -1e9);
        opts.maxCx = resolveRealInput(request, QStringLiteral("maxCx"), 1e9);
        opts.minCy = resolveRealInput(request, QStringLiteral("minCy"), -1e9);
        opts.maxCy = resolveRealInput(request, QStringLiteral("maxCy"), 1e9);
        opts.minWidth = resolveRealInput(request, QStringLiteral("minWidth"), 0.0);
        opts.maxWidth = resolveRealInput(request, QStringLiteral("maxWidth"), 0.0);
        opts.minHeight = resolveRealInput(request, QStringLiteral("minHeight"), 0.0);
        opts.maxHeight = resolveRealInput(request, QStringLiteral("maxHeight"), 0.0);
        opts.maxCount = resolveIntParam(request, QStringLiteral("maxCount"), 100);
        opts.sortBy = resolveStringParam(request, QStringLiteral("sortBy"), QStringLiteral("area"));
        opts.selectIndex = resolveIntParam(request, QStringLiteral("selectIndex"), 0);
        opts.thresholdMode =
            resolveStringParam(request, QStringLiteral("thresholdMode"), QStringLiteral("auto"));
        opts.polarity = resolveStringParam(request, QStringLiteral("polarity"), QStringLiteral("any"));
        opts.manualThreshold = resolveIntParam(request, QStringLiteral("threshold"), 128);
        opts.originOffset = origin;
        if (!BlobAnalyzeAlgorithm::apply(input, opts, region, scores, overlay, &selectedIndex,
                                         &unfiltered, &err))
            return failResult(err, DiagnosticCodes::matchFailed(), t.elapsed());
        if (selectedIndex < 0) {
            return failResult(region.regions.isEmpty()
                                  ? (unfiltered > 0
                                         ? QStringLiteral("Blob定位：存在候选但被过滤（未过滤=%1）")
                                               .arg(unfiltered)
                                         : QStringLiteral("Blob定位未找到目标"))
                                  : QStringLiteral("Blob定位选中索引超出候选范围"),
                              DiagnosticCodes::noTarget(), t.elapsed());
        }
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("image"), DataValue(overlay));
        r.outputs.insert(QStringLiteral("region"), DataValue(region));
        r.outputs.insert(QStringLiteral("count"), DataValue(int(region.regions.size())));
        r.outputs.insert(QStringLiteral("candidateCount"), DataValue(int(region.regions.size())));
        r.outputs.insert(QStringLiteral("unfilteredCount"), DataValue(unfiltered));
        r.outputs.insert(QStringLiteral("selectedIndex"), DataValue(selectedIndex));
        const RegionStats &picked = region.regions.at(selectedIndex);
        r.outputs.insert(QStringLiteral("center"), DataValue(picked.centroid));
        r.outputs.insert(QStringLiteral("area"), DataValue(picked.area));
        r.outputs.insert(QStringLiteral("confidence"),
                         DataValue(selectedIndex < scores.size() ? scores.at(selectedIndex) : 1.0));
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class FeatureMatchExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::featureMatch(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &context) override
    {
        QElapsedTimer t;
        t.start();
        VisionImage image = request.inputs.value(QStringLiteral("image")).toImage();
        QString err;
        VisionImage templ = loadTemplateImage(request, context, &err);
        if (templ.isEmpty())
            return failResult(err, DiagnosticCodes::templateMissing(), t.elapsed());
        QPointF peak;
        double score = 0.0;
        int inliers = 0;
        VisionImage overlay;
        FeatureMatchOptions opts;
        opts.loweRatio = resolveRealInput(request, QStringLiteral("loweRatio"), 0.75);
        opts.minMatches = resolveIntParam(request, QStringLiteral("minMatches"), 8);
        opts.ransacReprojThreshold = resolveRealInput(request, QStringLiteral("ransacReprojThreshold"), 3.0);
        opts.minInlierRatio = resolveRealInput(request, QStringLiteral("minInlierRatio"), 0.3);
        if (!FeatureMatchAlgorithm::apply(image, templ, opts, peak, score, inliers, overlay, &err))
            return failResult(err, DiagnosticCodes::matchFailed(), t.elapsed());
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("image"), DataValue(overlay));
        r.outputs.insert(QStringLiteral("center"), DataValue(peak));
        r.outputs.insert(QStringLiteral("score"), DataValue(score));
        r.outputs.insert(QStringLiteral("confidence"), DataValue(score));
        r.outputs.insert(QStringLiteral("inlierCount"), DataValue(inliers));
        r.outputs.insert(QStringLiteral("candidateCount"), DataValue(1));
        r.outputs.insert(QStringLiteral("selectedIndex"), DataValue(0));
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class TemplateMatchMultiExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::templateMatchMulti(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &context) override
    {
        QElapsedTimer t;
        t.start();
        VisionImage image = request.inputs.value(QStringLiteral("image")).toImage();
        QString err;
        VisionImage templ = loadTemplateImage(request, context, &err);
        if (templ.isEmpty())
            return failResult(err, DiagnosticCodes::templateMissing(), t.elapsed());
        QVector<QPointF> peaks;
        QVector<double> scores;
        VisionImage overlay;
        TemplateMatchMultiOptions opts;
        opts.minScore = resolveAutoReal(request, QStringLiteral("minScore"), 0.6, 0.55);
        opts.maxCount = resolveAutoInt(request, QStringLiteral("maxCount"), 5, 8);
        opts.nmsIoU = resolveAutoReal(request, QStringLiteral("nmsIoU"), 0.3, 0.3);
        opts.nmsCenterDistance =
            resolveAutoReal(request, QStringLiteral("nmsCenterDistance"), 0.0,
                            qMax(8.0, 0.5 * qMin(templ.width(), templ.height())));
        // Auto: mild scale/rotation search when user left identity defaults.
        const double scaleMinUser = resolveRealInput(request, QStringLiteral("scaleMin"), 1.0);
        const double scaleMaxUser = resolveRealInput(request, QStringLiteral("scaleMax"), 1.0);
        const bool autoScale =
            paramBindingMode(request.parameters, QStringLiteral("scaleMin"))
                != ParamBindingMode::Strict
            && qFuzzyCompare(scaleMinUser, 1.0) && qFuzzyCompare(scaleMaxUser, 1.0);
        opts.scaleMin = autoScale ? 0.9 : scaleMinUser;
        opts.scaleMax = autoScale ? 1.1 : scaleMaxUser;
        opts.scaleStep = resolveAutoReal(request, QStringLiteral("scaleStep"), 0.1, 0.05);
        const double angleMinUser = resolveRealInput(request, QStringLiteral("angleMin"), 0.0);
        const double angleMaxUser = resolveRealInput(request, QStringLiteral("angleMax"), 0.0);
        const bool autoAngle =
            paramBindingMode(request.parameters, QStringLiteral("angleMin"))
                != ParamBindingMode::Strict
            && qFuzzyIsNull(angleMinUser) && qFuzzyIsNull(angleMaxUser);
        opts.angleMin = autoAngle ? -10.0 : angleMinUser;
        opts.angleMax = autoAngle ? 10.0 : angleMaxUser;
        opts.angleStep = resolveAutoReal(request, QStringLiteral("angleStep"), 15.0, 5.0);
        QVector<double> scales;
        QVector<double> angles;
        if (!TemplateMatchMultiAlgorithm::apply(image, templ, opts, peaks, scores, overlay, &err,
                                                &scales, &angles))
            return failResult(err, DiagnosticCodes::matchFailed(), t.elapsed());

        int selected = 0;
        if (request.parameters.contains(QStringLiteral("pickX"))
            && request.parameters.contains(QStringLiteral("pickY")) && !peaks.isEmpty()) {
            const QPointF pick(request.parameters.value(QStringLiteral("pickX")).toDouble(),
                               request.parameters.value(QStringLiteral("pickY")).toDouble());
            double best = 1e100;
            for (int i = 0; i < peaks.size(); ++i) {
                const double d = QLineF(pick, peaks.at(i)).length();
                if (d < best) {
                    best = d;
                    selected = i;
                }
            }
        }

        OverlayList overlays;
        for (int i = 0; i < peaks.size(); ++i) {
            OverlayItem cross;
            cross.type = OverlayItemType::Contour;
            const QPointF p = peaks.at(i);
            cross.points = {p + QPointF(-6, 0), p + QPointF(6, 0), p,
                            p + QPointF(0, -6), p + QPointF(0, 6)};
            cross.color = (i == selected) ? QColor(0, 220, 255) : QColor(255, 180, 40);
            cross.penWidth = (i == selected) ? 2.5 : 1.5;
            overlays.append(cross);
        }

        ExecutionResult r;
        r.outputs.insert(QStringLiteral("image"), DataValue(overlay.isEmpty() ? image : overlay));
        r.outputs.insert(QStringLiteral("count"), DataValue(int(peaks.size())));
        r.outputs.insert(QStringLiteral("candidateCount"), DataValue(int(peaks.size())));
        r.outputs.insert(QStringLiteral("selectedIndex"), DataValue(peaks.isEmpty() ? -1 : selected));
        r.outputs.insert(QStringLiteral("found"), DataValue(!peaks.isEmpty()));
        r.overlays = overlays;
        r.outputs.insert(QStringLiteral("overlay"), DataValue(overlays));
        OperatorResultMeta meta;
        QJsonObject snap;
        snap.insert(QStringLiteral("minScore"), opts.minScore);
        snap.insert(QStringLiteral("scaleMin"), opts.scaleMin);
        snap.insert(QStringLiteral("scaleMax"), opts.scaleMax);
        snap.insert(QStringLiteral("angleMin"), opts.angleMin);
        snap.insert(QStringLiteral("angleMax"), opts.angleMax);
        snap.insert(QStringLiteral("candidateCount"), peaks.size());
        meta.autoSnapshot = makeAutoSnapshot(snap);
        if (!peaks.isEmpty()) {
            const double score = selected < scores.size() ? scores.at(selected) : 0.0;
            r.outputs.insert(QStringLiteral("center"), DataValue(peaks.at(selected)));
            r.outputs.insert(QStringLiteral("score"), DataValue(score));
            r.outputs.insert(QStringLiteral("confidence"), DataValue(score));
            r.outputs.insert(QStringLiteral("scale"),
                             DataValue(selected < scales.size() ? scales.at(selected) : 1.0));
            r.outputs.insert(QStringLiteral("angle"),
                             DataValue(selected < angles.size() ? angles.at(selected) : 0.0));
            meta.qualityScore = score;
            meta.outcome = score < opts.minScore + 0.05
                ? OperatorOutcome::SuccessWithWarning
                : OperatorOutcome::Success;
            if (meta.outcome == OperatorOutcome::SuccessWithWarning) {
                meta.failureStage = QStringLiteral("low_score");
                meta.message = QStringLiteral("模板匹配成功但分数接近阈值");
            }
        } else {
            meta.outcome = OperatorOutcome::SuccessWithWarning;
            meta.failureStage = QStringLiteral("match");
            meta.message = QStringLiteral("未找到满足阈值的模板匹配");
            meta.qualityScore = 0.0;
            r.diagnosticCode = DiagnosticCodes::noCandidate();
        }
        r.elapsedMs = t.elapsed();
        applyOperatorMeta(r, meta);
        return r;
    }
};

class SubpixelRefineExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::subpixelRefine(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        QElapsedTimer t;
        t.start();
        const QPointF in = request.inputs.contains(QStringLiteral("point"))
            ? request.inputs.value(QStringLiteral("point")).toPoint2D()
            : QPointF(resolveRealInput(request, QStringLiteral("x"), 0.0),
                      resolveRealInput(request, QStringLiteral("y"), 0.0));
        VisionImage image = request.inputs.value(QStringLiteral("image")).toImage();
        const bool compatPassthrough = resolveBoolParam(request, QStringLiteral("passthrough"), false);

        ExecutionResult r;
        if (image.isEmpty() || compatPassthrough) {
            // Compatibility mode: no image provided → passthrough with capability mark.
            r.outputs.insert(QStringLiteral("point"), DataValue(in));
            r.outputs.insert(QStringLiteral("x"), DataValue(in.x()));
            r.outputs.insert(QStringLiteral("y"), DataValue(in.y()));
            r.outputs.insert(QStringLiteral("mode"), DataValue(QStringLiteral("passthrough")));
            r.outputs.insert(QStringLiteral("confidence"), DataValue(0.0));
            r.diagnosticCode = DiagnosticCodes::capabilityLimited();
            r.status = ModuleStatus::Success;
            r.elapsedMs = t.elapsed();
            return r;
        }

        SubpixelOptions opts;
        opts.mode = resolveStringParam(request, QStringLiteral("mode"), QStringLiteral("corner"));
        opts.windowSize = resolveIntParam(request, QStringLiteral("windowSize"), 5);
        opts.terminateEps = resolveRealInput(request, QStringLiteral("terminateEps"), 0.01);
        opts.maxIterations = resolveIntParam(request, QStringLiteral("maxIterations"), 40);
        QPointF refined;
        double confidence = 0.0;
        QString err;
        if (!SubpixelRefineAlgorithm::refine(image, in, refined, confidence, opts, &err))
            return failResult(err, DiagnosticCodes::measureFailed(), t.elapsed());

        r.outputs.insert(QStringLiteral("point"), DataValue(refined));
        r.outputs.insert(QStringLiteral("x"), DataValue(refined.x()));
        r.outputs.insert(QStringLiteral("y"), DataValue(refined.y()));
        r.outputs.insert(QStringLiteral("mode"), DataValue(opts.mode));
        r.outputs.insert(QStringLiteral("confidence"), DataValue(confidence));
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

} // namespace

void registerLocateExecutors(NodeExecutorRegistry &reg)
{
    reg.registerFactory(VisionNodeIds::houghLines(), [] { return std::make_shared<HoughLinesExecutor>(); });
    reg.registerFactory(VisionNodeIds::houghCircles(), [] { return std::make_shared<HoughCirclesExecutor>(); });
    reg.registerFactory(VisionNodeIds::blobLocate(), [] { return std::make_shared<BlobLocateExecutor>(); });
    reg.registerFactory(VisionNodeIds::featureMatch(), [] { return std::make_shared<FeatureMatchExecutor>(); });
    reg.registerFactory(VisionNodeIds::templateMatchMulti(), [] { return std::make_shared<TemplateMatchMultiExecutor>(); });
    reg.registerFactory(VisionNodeIds::subpixelRefine(), [] { return std::make_shared<SubpixelRefineExecutor>(); });
}

} // namespace Selt
