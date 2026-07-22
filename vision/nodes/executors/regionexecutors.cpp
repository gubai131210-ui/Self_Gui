#include "vision/algorithms/regionalgorithms.h"
#include "vision/model/contourdata.h"
#include "vision/nodes/imageexecutorhelpers.h"
#include "vision/registry/visionnodeids.h"
#include "vision/runtime/nodeexecutorregistry.h"

#include <QElapsedTimer>
#include <memory>

namespace Selt {
namespace {

class MaskCombineExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::maskCombine(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        QElapsedTimer t;
        t.start();
        VisionImage a = request.inputs.value(QStringLiteral("imageA")).toImage();
        if (a.isEmpty())
            a = request.inputs.value(QStringLiteral("inA")).toImage();
        if (a.isEmpty())
            a = request.inputs.value(QStringLiteral("image")).toImage();
        VisionImage b = request.inputs.value(QStringLiteral("imageB")).toImage();
        if (b.isEmpty())
            b = request.inputs.value(QStringLiteral("inB")).toImage();
        VisionImage out;
        QString err;
        if (!MaskCombineAlgorithm::apply(a, b, out,
                                         resolveStringParam(request, QStringLiteral("op"),
                                                            QStringLiteral("and")),
                                         &err))
            return failResult(err, DiagnosticCodes::imageTypeMismatch(), t.elapsed());
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("image"), DataValue(out));
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class FindContoursExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::findContours(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        QElapsedTimer t;
        t.start();
        VisionImage input = applyRoiFromRequest(request.inputs.value(QStringLiteral("image")).toImage(),
                                                request.parameters);
        QVector<QVector<QPointF>> raw;
        VisionImage overlay;
        QString err;
        if (!FindContoursAlgorithm::apply(input, raw, overlay, &err))
            return failResult(err, DiagnosticCodes::regionEmpty(), t.elapsed());
        ContourList list;
        for (const QVector<QPointF> &c : raw) {
            ContourData d;
            d.points = c;
            list.contours.append(d);
        }
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("image"), DataValue(overlay));
        r.outputs.insert(QStringLiteral("contours"), DataValue(list));
        r.outputs.insert(QStringLiteral("count"), DataValue(int(list.contours.size())));
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class ConnectedComponentsExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::connectedComponents(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        QElapsedTimer t;
        t.start();
        VisionImage input = applyRoiFromRequest(request.inputs.value(QStringLiteral("image")).toImage(),
                                                request.parameters);
        VisionImage labels;
        int count = 0;
        QVector<QPointF> centroids;
        QVector<double> areas;
        QString err;
        if (!ConnectedComponentsAlgorithm::apply(input, labels, count, centroids, areas, &err))
            return failResult(err, DiagnosticCodes::regionEmpty(), t.elapsed());
        RegionData region;
        region.labelCount = count;
        TableData table;
        table.columns = {QStringLiteral("label"), QStringLiteral("cx"), QStringLiteral("cy"),
                         QStringLiteral("area")};
        for (int i = 0; i < centroids.size(); ++i) {
            RegionStats s;
            s.label = i + 1;
            s.centroid = centroids.at(i);
            s.area = i < areas.size() ? areas.at(i) : 0.0;
            region.regions.append(s);
            table.rows.append({s.label, s.centroid.x(), s.centroid.y(), s.area});
        }
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("image"), DataValue(labels));
        r.outputs.insert(QStringLiteral("region"), DataValue(region));
        r.outputs.insert(QStringLiteral("table"), DataValue(table));
        r.outputs.insert(QStringLiteral("count"), DataValue(count));
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class RegionFillExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::regionFill(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        QElapsedTimer t;
        t.start();
        VisionImage input = applyRoiFromRequest(request.inputs.value(QStringLiteral("image")).toImage(),
                                                request.parameters);
        VisionImage out;
        VisionImage overlay;
        QString err;
        if (!RegionFillAlgorithm::apply(input, out, overlay, &err))
            return failResult(err, DiagnosticCodes::regionEmpty(), t.elapsed());
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("image"), DataValue(out.isEmpty() ? overlay : out));
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class BlobAnalyzeExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::blobAnalyze(); }
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
        opts.minCircularity = resolveRealInput(request, QStringLiteral("minCircularity"), 0.0);
        opts.minAspectRatio = resolveRealInput(request, QStringLiteral("minAspectRatio"), 0.0);
        opts.maxAspectRatio = resolveRealInput(request, QStringLiteral("maxAspectRatio"), 1e9);
        opts.minExtent = resolveRealInput(request, QStringLiteral("minExtent"), 0.0);
        opts.minSolidity = resolveRealInput(request, QStringLiteral("minSolidity"), 0.0);
        opts.maxCount = resolveIntParam(request, QStringLiteral("maxCount"), 100);
        opts.sortBy = resolveStringParam(request, QStringLiteral("sortBy"), QStringLiteral("area"));
        opts.selectIndex = resolveIntParam(request, QStringLiteral("selectIndex"), 0);
        const bool requireTarget = resolveBoolParam(request, QStringLiteral("requireTarget"), false);
        if (!BlobAnalyzeAlgorithm::apply(input, opts, region, scores, overlay, &selectedIndex, &err))
            return failResult(err, DiagnosticCodes::noTarget(), t.elapsed());
        if (requireTarget && selectedIndex < 0) {
            return failResult(region.regions.isEmpty()
                                  ? QStringLiteral("未找到符合条件的 Blob 目标")
                                  : QStringLiteral("选中索引超出候选范围"),
                              DiagnosticCodes::noTarget(), t.elapsed());
        }
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("image"), DataValue(overlay));
        r.outputs.insert(QStringLiteral("region"), DataValue(region));
        r.outputs.insert(QStringLiteral("count"), DataValue(int(region.regions.size())));
        r.outputs.insert(QStringLiteral("candidateCount"), DataValue(int(region.regions.size())));
        r.outputs.insert(QStringLiteral("selectedIndex"), DataValue(selectedIndex));
        if (selectedIndex >= 0 && selectedIndex < region.regions.size()) {
            const RegionStats &picked = region.regions.at(selectedIndex);
            r.outputs.insert(QStringLiteral("center"), DataValue(picked.centroid));
            r.outputs.insert(QStringLiteral("area"), DataValue(picked.area));
            r.outputs.insert(QStringLiteral("circularity"), DataValue(picked.circularity));
            r.outputs.insert(QStringLiteral("confidence"),
                             DataValue(selectedIndex < scores.size() ? scores.at(selectedIndex) : 1.0));
        } else {
            // Soft success for count-style flows: emit stable zeros when no primary target.
            r.outputs.insert(QStringLiteral("center"), DataValue(QPointF(0.0, 0.0)));
            r.outputs.insert(QStringLiteral("area"), DataValue(0.0));
            r.outputs.insert(QStringLiteral("circularity"), DataValue(0.0));
            r.outputs.insert(QStringLiteral("confidence"), DataValue(0.0));
        }
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

} // namespace

void registerRegionExecutors(NodeExecutorRegistry &reg)
{
    reg.registerFactory(VisionNodeIds::maskCombine(), [] { return std::make_shared<MaskCombineExecutor>(); });
    reg.registerFactory(VisionNodeIds::findContours(), [] { return std::make_shared<FindContoursExecutor>(); });
    reg.registerFactory(VisionNodeIds::connectedComponents(), [] { return std::make_shared<ConnectedComponentsExecutor>(); });
    reg.registerFactory(VisionNodeIds::regionFill(), [] { return std::make_shared<RegionFillExecutor>(); });
    reg.registerFactory(VisionNodeIds::blobAnalyze(), [] { return std::make_shared<BlobAnalyzeExecutor>(); });
}

} // namespace Selt
