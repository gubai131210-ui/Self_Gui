#include "vision/algorithms/locatealgorithms.h"
#include "vision/algorithms/regionalgorithms.h"
#include "vision/algorithms/subpixelalgorithms.h"
#include "vision/nodes/imageexecutorhelpers.h"
#include "vision/registry/visionnodeids.h"
#include "vision/runtime/nodeexecutorregistry.h"
#include "vision/storage/projectresourcestore.h"
#include "vision/algorithms/imageloader.h"

#include <QElapsedTimer>
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
        opts.cannyLow = resolveRealInput(request, QStringLiteral("cannyLow"), 50.0);
        opts.cannyHigh = resolveRealInput(request, QStringLiteral("cannyHigh"), 150.0);
        opts.accumulatorThreshold = resolveIntParam(request, QStringLiteral("accumulatorThreshold"), 50);
        opts.minLineLength = resolveRealInput(request, QStringLiteral("minLineLength"), 30.0);
        opts.maxLineGap = resolveRealInput(request, QStringLiteral("maxLineGap"), 10.0);
        opts.maxCount = resolveIntParam(request, QStringLiteral("maxCount"), 50);
        opts.sortBy = resolveStringParam(request, QStringLiteral("sortBy"), QStringLiteral("length"));
        if (!HoughLinesAlgorithm::apply(input, lines, opts, overlay, &err))
            return failResult(err, DiagnosticCodes::noTarget(), t.elapsed());
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("image"), DataValue(overlay));
        r.outputs.insert(QStringLiteral("count"), DataValue(int(lines.size())));
        r.outputs.insert(QStringLiteral("candidateCount"), DataValue(int(lines.size())));
        r.outputs.insert(QStringLiteral("selectedIndex"), DataValue(lines.isEmpty() ? -1 : 0));
        if (!lines.isEmpty()) {
            Line2D line;
            line.start = lines.first().start;
            line.end = lines.first().end;
            r.outputs.insert(QStringLiteral("line"), DataValue(line));
            r.outputs.insert(QStringLiteral("confidence"), DataValue(lines.first().confidence));
        }
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
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
        opts.minRadius = resolveRealInput(request, QStringLiteral("minRadius"), 5.0);
        opts.maxRadius = resolveRealInput(request, QStringLiteral("maxRadius"), 0.0);
        opts.dp = resolveRealInput(request, QStringLiteral("dp"), 1.2);
        opts.minDist = resolveRealInput(request, QStringLiteral("minDist"), 0.0);
        opts.param1 = resolveRealInput(request, QStringLiteral("param1"), 100.0);
        opts.param2 = resolveRealInput(request, QStringLiteral("param2"), 30.0);
        opts.maxCount = resolveIntParam(request, QStringLiteral("maxCount"), 50);
        if (!HoughCirclesAlgorithm::apply(input, circles, opts, overlay, &err))
            return failResult(err, DiagnosticCodes::noTarget(), t.elapsed());
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("image"), DataValue(overlay));
        r.outputs.insert(QStringLiteral("count"), DataValue(int(circles.size())));
        r.outputs.insert(QStringLiteral("candidateCount"), DataValue(int(circles.size())));
        r.outputs.insert(QStringLiteral("selectedIndex"), DataValue(circles.isEmpty() ? -1 : 0));
        if (!circles.isEmpty()) {
            Circle2D c;
            c.center = circles.first().center;
            c.radius = circles.first().radius;
            r.outputs.insert(QStringLiteral("circle"), DataValue(c));
            r.outputs.insert(QStringLiteral("center"), DataValue(c.center));
            r.outputs.insert(QStringLiteral("radius"), DataValue(c.radius));
            r.outputs.insert(QStringLiteral("confidence"), DataValue(circles.first().confidence));
        }
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
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
        VisionImage input = applyRoiFromRequest(request.inputs.value(QStringLiteral("image")).toImage(),
                                                request.parameters);
        RegionData region;
        QVector<double> scores;
        VisionImage overlay;
        QString err;
        int selectedIndex = -1;
        BlobAnalyzeAlgorithm::Options opts;
        opts.minArea = resolveRealInput(request, QStringLiteral("minArea"), 50.0);
        opts.maxArea = resolveRealInput(request, QStringLiteral("maxArea"), 1e9);
        opts.minCircularity = resolveRealInput(request, QStringLiteral("minCircularity"), 0.2);
        opts.minAspectRatio = resolveRealInput(request, QStringLiteral("minAspectRatio"), 0.0);
        opts.maxAspectRatio = resolveRealInput(request, QStringLiteral("maxAspectRatio"), 1e9);
        opts.minExtent = resolveRealInput(request, QStringLiteral("minExtent"), 0.0);
        opts.minSolidity = resolveRealInput(request, QStringLiteral("minSolidity"), 0.0);
        opts.maxCount = resolveIntParam(request, QStringLiteral("maxCount"), 100);
        opts.sortBy = resolveStringParam(request, QStringLiteral("sortBy"), QStringLiteral("area"));
        opts.selectIndex = resolveIntParam(request, QStringLiteral("selectIndex"), 0);
        if (!BlobAnalyzeAlgorithm::apply(input, opts, region, scores, overlay, &selectedIndex, &err))
            return failResult(err, DiagnosticCodes::matchFailed(), t.elapsed());
        // Locate always requires a primary target; empty / OOR selectIndex is a hard failure.
        if (selectedIndex < 0) {
            return failResult(region.regions.isEmpty()
                                  ? QStringLiteral("Blob定位未找到目标")
                                  : QStringLiteral("Blob定位选中索引超出候选范围"),
                              DiagnosticCodes::noTarget(), t.elapsed());
        }
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("image"), DataValue(overlay));
        r.outputs.insert(QStringLiteral("region"), DataValue(region));
        r.outputs.insert(QStringLiteral("count"), DataValue(int(region.regions.size())));
        r.outputs.insert(QStringLiteral("candidateCount"), DataValue(int(region.regions.size())));
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
        opts.minScore = resolveRealInput(request, QStringLiteral("minScore"), 0.6);
        opts.maxCount = resolveIntParam(request, QStringLiteral("maxCount"), 5);
        opts.nmsIoU = resolveRealInput(request, QStringLiteral("nmsIoU"), 0.3);
        opts.nmsCenterDistance = resolveRealInput(request, QStringLiteral("nmsCenterDistance"), 0.0);
        if (!TemplateMatchMultiAlgorithm::apply(image, templ, opts, peaks, scores, overlay, &err))
            return failResult(err, DiagnosticCodes::matchFailed(), t.elapsed());
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("image"), DataValue(overlay));
        r.outputs.insert(QStringLiteral("count"), DataValue(int(peaks.size())));
        r.outputs.insert(QStringLiteral("candidateCount"), DataValue(int(peaks.size())));
        r.outputs.insert(QStringLiteral("selectedIndex"), DataValue(peaks.isEmpty() ? -1 : 0));
        if (!peaks.isEmpty()) {
            r.outputs.insert(QStringLiteral("center"), DataValue(peaks.first()));
            r.outputs.insert(QStringLiteral("score"),
                             DataValue(scores.isEmpty() ? 0.0 : scores.first()));
            r.outputs.insert(QStringLiteral("confidence"),
                             DataValue(scores.isEmpty() ? 0.0 : scores.first()));
        }
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
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
