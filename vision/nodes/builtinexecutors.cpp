#include "vision/nodes/builtinexecutors.h"

#include "vision/algorithms/circlemeasurementalgorithm.h"
#include "vision/algorithms/grayscalealgorithm.h"
#include "vision/algorithms/imagefilteralgorithms.h"
#include "vision/algorithms/imageloader.h"
#include "vision/algorithms/linemeasurementalgorithm.h"
#include "vision/algorithms/rectanglemeasurementalgorithm.h"
#include "vision/algorithms/thresholdalgorithm.h"
#include "vision/model/measurementdefinition.h"
#include "vision/nodes/imageexecutorhelpers.h"
#include "vision/registry/visionnodeids.h"
#include "vision/runtime/diagnosticcodes.h"
#include "vision/runtime/nodeexecutorregistry.h"
#include "vision/storage/projectresourcestore.h"

#include <QElapsedTimer>
#include <QtMath>
#include <cmath>
#include <opencv2/imgproc.hpp>

namespace Selt {

// Extended operator registration (O1–O5) — defined in vision/nodes/executors/*.cpp
void registerPreprocessExecutors(NodeExecutorRegistry &reg);
void registerRegionExecutors(NodeExecutorRegistry &reg);
void registerLocateExecutors(NodeExecutorRegistry &reg);
void registerAdvancedMeasureExecutors(NodeExecutorRegistry &reg);
void registerLogicOutputExecutors(NodeExecutorRegistry &reg);
void registerCommunicationExecutors(NodeExecutorRegistry &reg);
void registerRecognitionExecutors(NodeExecutorRegistry &reg);

namespace {

class ImageLoaderExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::imageLoader(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &context) override
    {
        ExecutionResult r;
        QElapsedTimer t;
        t.start();
        VisionImage image;
        QString error;
        if (context.hasLiveFrame()) {
            image = context.liveFrame();
            r.outputs.insert(QStringLiteral("image"), DataValue(image));
            r.status = ModuleStatus::Success;
            r.elapsedMs = t.elapsed();
            return r;
        }
        const QString path = request.parameters.value(QStringLiteral("path")).toString();
        if (!ImageLoader::load(path, image, &error)) {
            r.status = ModuleStatus::Failed;
            r.errorMessage = error;
            r.diagnosticCode = QStringLiteral("image_load_failed");
            r.elapsedMs = t.elapsed();
            return r;
        }
        r.outputs.insert(QStringLiteral("image"), DataValue(image));
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class GrayscaleExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::grayscale(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        ExecutionResult r;
        QElapsedTimer t;
        t.start();
        VisionImage input = request.inputs.value(QStringLiteral("image")).toImage();
        input = applyRoiFromRequest(input, request.parameters);
        VisionImage gray;
        QString error;
        if (!GrayscaleAlgorithm::convert(input, gray, &error)) {
            r.status = ModuleStatus::Failed;
            r.errorMessage = error;
            r.elapsedMs = t.elapsed();
            return r;
        }
        r.outputs.insert(QStringLiteral("image"), DataValue(gray));
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class ThresholdExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::threshold(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        ExecutionResult r;
        QElapsedTimer t;
        t.start();
        VisionImage input = applyRoiFromRequest(request.inputs.value(QStringLiteral("image")).toImage(),
                                     request.parameters);
        VisionImage binary;
        QString error;
        const int threshold = request.parameters.value(QStringLiteral("threshold")).toInt(128);
        const int maxValue = request.parameters.value(QStringLiteral("maxValue")).toInt(255);
        const ThresholdMode mode =
            request.parameters.value(QStringLiteral("mode")).toString() == QLatin1String("binaryInv")
                ? ThresholdMode::BinaryInv
                : ThresholdMode::Binary;
        if (!ThresholdAlgorithm::apply(input, binary, threshold, maxValue, mode, &error)) {
            r.status = ModuleStatus::Failed;
            r.errorMessage = error;
            r.elapsedMs = t.elapsed();
            return r;
        }
        r.outputs.insert(QStringLiteral("image"), DataValue(binary));
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class BlurExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::blur(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        ExecutionResult r;
        QElapsedTimer t;
        t.start();
        VisionImage input = applyRoiFromRequest(request.inputs.value(QStringLiteral("image")).toImage(),
                                     request.parameters);
        VisionImage out;
        QString error;
        const int ksize = request.parameters.value(QStringLiteral("ksize")).toInt(5);
        if (!BlurAlgorithm::apply(input, out, ksize, &error)) {
            r.status = ModuleStatus::Failed;
            r.errorMessage = error;
            r.elapsedMs = t.elapsed();
            return r;
        }
        r.outputs.insert(QStringLiteral("image"), DataValue(out));
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class CannyExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::canny(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        ExecutionResult r;
        QElapsedTimer t;
        t.start();
        VisionImage input = applyRoiFromRequest(request.inputs.value(QStringLiteral("image")).toImage(),
                                     request.parameters);
        VisionImage out;
        QString error;
        const double t1 = request.parameters.value(QStringLiteral("threshold1")).toDouble(50.0);
        const double t2 = request.parameters.value(QStringLiteral("threshold2")).toDouble(150.0);
        int aperture = request.parameters.value(QStringLiteral("apertureSize")).toInt(3);
        if (aperture != 3 && aperture != 5 && aperture != 7)
            aperture = 3;
        if (!CannyAlgorithm::apply(input, out, t1, t2, &error, aperture)) {
            r.status = ModuleStatus::Failed;
            r.errorMessage = error;
            r.elapsedMs = t.elapsed();
            return r;
        }
        r.outputs.insert(QStringLiteral("image"), DataValue(out));
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class MorphologyExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::morphology(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        ExecutionResult r;
        QElapsedTimer t;
        t.start();
        VisionImage input = applyRoiFromRequest(request.inputs.value(QStringLiteral("image")).toImage(),
                                     request.parameters);
        VisionImage out;
        QString error;
        const QString opText = request.parameters.value(QStringLiteral("op")).toString(QStringLiteral("erode"));
        MorphologyAlgorithm::Op op = MorphologyAlgorithm::Op::Erode;
        if (opText == QLatin1String("dilate"))
            op = MorphologyAlgorithm::Op::Dilate;
        else if (opText == QLatin1String("open"))
            op = MorphologyAlgorithm::Op::Open;
        else if (opText == QLatin1String("close"))
            op = MorphologyAlgorithm::Op::Close;
        const int ksize = request.parameters.value(QStringLiteral("ksize")).toInt(3);
        const int iterations = qMax(1, request.parameters.value(QStringLiteral("iterations")).toInt(1));
        if (!MorphologyAlgorithm::apply(input, out, op, ksize, &error, iterations)) {
            r.status = ModuleStatus::Failed;
            r.errorMessage = error;
            r.elapsedMs = t.elapsed();
            return r;
        }
        r.outputs.insert(QStringLiteral("image"), DataValue(out));
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class ResizeExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::resize(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        ExecutionResult r;
        QElapsedTimer t;
        t.start();
        VisionImage input = request.inputs.value(QStringLiteral("image")).toImage();
        VisionImage out;
        QString error;
        const int width = request.parameters.value(QStringLiteral("width")).toInt(640);
        const int height = request.parameters.value(QStringLiteral("height")).toInt(480);
        if (!ResizeAlgorithm::apply(input, out, width, height, &error)) {
            r.status = ModuleStatus::Failed;
            r.errorMessage = error;
            r.elapsedMs = t.elapsed();
            return r;
        }
        r.outputs.insert(QStringLiteral("image"), DataValue(out));
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class TemplateMatchExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::templateMatch(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &context) override
    {
        ExecutionResult r;
        QElapsedTimer t;
        t.start();
        VisionImage image = request.inputs.value(QStringLiteral("image")).toImage();
        VisionImage templ = request.inputs.value(QStringLiteral("template")).toImage();
        QString templateSource = QStringLiteral("upstream");

        if (templ.isEmpty()) {
            const QString resourceId =
                request.parameters.value(QStringLiteral("templateResourceId")).toString();
            if (!resourceId.isEmpty()) {
                const ProjectResourceStore *store = context.resourceStore();
                if (!store || !store->contains(resourceId)) {
                    r.status = ModuleStatus::Failed;
                    r.errorMessage = QStringLiteral("模板资源不存在: %1").arg(resourceId);
                    r.diagnosticCode = QStringLiteral("template_missing");
                    r.elapsedMs = t.elapsed();
                    return r;
                }
                const ProjectResource resource = store->resource(resourceId);
                if (!resource.exists) {
                    r.status = ModuleStatus::Failed;
                    r.errorMessage = QStringLiteral("模板资源文件缺失: %1").arg(resource.relativePath);
                    r.diagnosticCode = QStringLiteral("template_missing");
                    r.elapsedMs = t.elapsed();
                    return r;
                }
                QString loadError;
                if (!ImageLoader::load(resource.absolutePath, templ, &loadError)) {
                    r.status = ModuleStatus::Failed;
                    r.errorMessage = loadError;
                    r.diagnosticCode = QStringLiteral("image_load_failed");
                    r.elapsedMs = t.elapsed();
                    return r;
                }
                templateSource = QStringLiteral("resource");
            }
        }

        if (templ.isEmpty()) {
            const QString path = request.parameters.value(QStringLiteral("templatePath")).toString();
            QString loadError;
            if (!path.isEmpty() && !ImageLoader::load(path, templ, &loadError)) {
                r.status = ModuleStatus::Failed;
                r.errorMessage = loadError;
                r.diagnosticCode = QStringLiteral("image_load_failed");
                r.elapsedMs = t.elapsed();
                return r;
            }
            if (!templ.isEmpty())
                templateSource = QStringLiteral("file");
        }

        if (templ.isEmpty()) {
            r.status = ModuleStatus::Failed;
            r.errorMessage = QStringLiteral("模板为空：请连接模板端口、设置资源ID或模板路径");
            r.diagnosticCode = QStringLiteral("template_missing");
            r.elapsedMs = t.elapsed();
            return r;
        }
        if (image.isEmpty()) {
            r.status = ModuleStatus::Failed;
            r.errorMessage = QStringLiteral("搜索图像为空");
            r.diagnosticCode = QStringLiteral("image_load_failed");
            r.elapsedMs = t.elapsed();
            return r;
        }
        if (templ.width() > image.width() || templ.height() > image.height()) {
            r.status = ModuleStatus::Failed;
            r.errorMessage = QStringLiteral("模板尺寸大于搜索图像");
            r.diagnosticCode = QStringLiteral("image_type_mismatch");
            r.elapsedMs = t.elapsed();
            return r;
        }

        QPointF peak;
        double score = 0.0;
        QString error;
        if (!TemplateMatchAlgorithm::match(image, templ, &peak, &score, &error)) {
            r.status = ModuleStatus::Failed;
            r.errorMessage = error;
            r.diagnosticCode = error.contains(QStringLiteral("ROI"))
                ? QStringLiteral("invalid_roi")
                : (error.contains(QStringLiteral("格式")) || error.contains(QStringLiteral("type"))
                       ? QStringLiteral("image_type_mismatch")
                       : QStringLiteral("opencv_exception"));
            r.elapsedMs = t.elapsed();
            return r;
        }
        const double minScore = request.parameters.value(QStringLiteral("minScore")).toDouble(0.5);
        if (score < minScore) {
            r.status = ModuleStatus::Failed;
            r.errorMessage = QStringLiteral("匹配得分过低: %1").arg(score, 0, 'f', 3);
            r.elapsedMs = t.elapsed();
            return r;
        }

        VisionImage annotated = image.clone();
        OverlayList overlays;
        const bool draw = request.parameters.value(QStringLiteral("drawOverlay")).toBool(true);
        if (draw && !annotated.isEmpty() && !templ.isEmpty()) {
            const QRectF box(peak.x(), peak.y(), templ.width(), templ.height());
            OverlayItem item;
            item.type = OverlayItemType::Rectangle;
            item.rect = box;
            item.color = QColor(0, 200, 255);
            item.text = QStringLiteral("score=%1").arg(score, 0, 'f', 3);
            overlays.append(item);

            OverlayItem label;
            label.type = OverlayItemType::Text;
            label.rect = QRectF(box.topLeft(), QSizeF(1, 1));
            label.color = QColor(0, 200, 255);
            label.text = item.text;
            overlays.append(label);
        }

        r.outputs.insert(QStringLiteral("image"), DataValue(annotated));
        r.outputs.insert(QStringLiteral("score"), DataValue(score));
        const QPointF center(peak.x() + templ.width() * 0.5, peak.y() + templ.height() * 0.5);
        r.outputs.insert(QStringLiteral("center"), DataValue(center));
        RegionData region;
        RegionStats stats;
        stats.centroid = center;
        stats.boundingRect = QRectF(peak.x(), peak.y(), templ.width(), templ.height());
        stats.area = double(templ.width()) * double(templ.height());
        stats.width = templ.width();
        stats.height = templ.height();
        region.regions.append(stats);
        region.labelCount = 1;
        r.outputs.insert(QStringLiteral("region"), DataValue(region));
        r.outputs.insert(QStringLiteral("overlay"), DataValue(overlays));
        r.overlays = overlays;
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        Q_UNUSED(templateSource);
        return r;
    }
};

class TemplateSourceExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::templateSource(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &context) override
    {
        ExecutionResult r;
        QElapsedTimer t;
        t.start();
        VisionImage templ;
        QString error;

        const QString resourceId =
            request.parameters.value(QStringLiteral("templateResourceId")).toString();
        if (!resourceId.isEmpty()) {
            const ProjectResourceStore *store = context.resourceStore();
            if (!store || !store->contains(resourceId)) {
                r.status = ModuleStatus::Failed;
                r.errorMessage = QStringLiteral("模板资源不存在: %1").arg(resourceId);
                r.diagnosticCode = QStringLiteral("template_missing");
                r.elapsedMs = t.elapsed();
                return r;
            }
            const ProjectResource resource = store->resource(resourceId);
            if (!resource.exists) {
                r.status = ModuleStatus::Failed;
                r.errorMessage = QStringLiteral("模板资源文件缺失: %1").arg(resource.relativePath);
                r.diagnosticCode = QStringLiteral("template_missing");
                r.elapsedMs = t.elapsed();
                return r;
            }
            if (!ImageLoader::load(resource.absolutePath, templ, &error)) {
                r.status = ModuleStatus::Failed;
                r.errorMessage = error;
                r.diagnosticCode = QStringLiteral("image_load_failed");
                r.elapsedMs = t.elapsed();
                return r;
            }
        } else {
            const QString path = request.parameters.value(QStringLiteral("templatePath")).toString();
            if (!ImageLoader::load(path, templ, &error)) {
                r.status = ModuleStatus::Failed;
                r.errorMessage = error.isEmpty() ? QStringLiteral("请设置模板资源或路径") : error;
                r.diagnosticCode = error.isEmpty()
                    ? QStringLiteral("template_missing")
                    : QStringLiteral("image_load_failed");
                r.elapsedMs = t.elapsed();
                return r;
            }
        }

        r.outputs.insert(QStringLiteral("image"), DataValue(templ));
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class RectangleMeasureExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::rectangleMeasure(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &context) override
    {
        QElapsedTimer t;
        t.start();
        QString error;
        QString code;
        VisionImage input = requireImageWithRoi(request, &error, &code);
        if (input.isEmpty())
            return failResult(error, code.isEmpty() ? DiagnosticCodes::imageEmpty() : code, t.elapsed());
        MeasurementResult measure;
        VisionImage overlay;
        RectangleMeasureOptions options;
        options.minArea = resolveRealInput(request, QStringLiteral("minArea"), 100.0);
        options.drawOverlay = resolveBoolParam(request, QStringLiteral("drawOverlay"), true);
        options.threshold.strategy = ThresholdAlgorithm::parseStrategy(
            resolveStringParam(request, QStringLiteral("thresholdStrategy"), QStringLiteral("manual")));
        options.threshold.threshold = resolveIntParam(request, QStringLiteral("threshold"), 128);
        options.threshold.adaptiveBlockSize = resolveIntParam(request, QStringLiteral("adaptiveBlockSize"), 31);
        options.threshold.adaptiveC = resolveRealInput(request, QStringLiteral("adaptiveC"), 5.0);
        if (!RectangleMeasurementAlgorithm::measure(input, measure, overlay, options, &error))
            return failResult(error, DiagnosticCodes::measureFailed(), t.elapsed());
        measure = finalizeLengthMeasurement(measure, request, context);
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("image"), DataValue(overlay));
        r.outputs.insert(QStringLiteral("measurement"), DataValue(measure));
        if (measure.valid) {
            r.outputs.insert(QStringLiteral("width"), DataValue(measure.width));
            r.outputs.insert(QStringLiteral("height"), DataValue(measure.height));
            OverlayItem box;
            box.type = OverlayItemType::Rectangle;
            box.rect = measure.boundingRect;
            box.color = QColor(0, 220, 80);
            r.overlays.append(box);
            r.outputs.insert(QStringLiteral("overlay"), DataValue(r.overlays));
        }
        r.measurement = measure;
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class CircleMeasureExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::circleMeasure(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &context) override
    {
        QElapsedTimer t;
        t.start();
        QString error;
        QString code;
        VisionImage input = requireImageWithRoi(request, &error, &code);
        if (input.isEmpty())
            return failResult(error, code.isEmpty() ? DiagnosticCodes::imageEmpty() : code, t.elapsed());
        MeasurementResult measure;
        VisionImage overlay;
        CircleMeasureOptions options;
        options.minRadius = resolveRealInput(request, QStringLiteral("minRadius"), 5.0);
        options.drawOverlay = resolveBoolParam(request, QStringLiteral("drawOverlay"), true);
        options.threshold.strategy = ThresholdAlgorithm::parseStrategy(
            resolveStringParam(request, QStringLiteral("thresholdStrategy"), QStringLiteral("manual")));
        options.threshold.threshold = resolveIntParam(request, QStringLiteral("threshold"), 128);
        options.threshold.adaptiveBlockSize = resolveIntParam(request, QStringLiteral("adaptiveBlockSize"), 31);
        options.threshold.adaptiveC = resolveRealInput(request, QStringLiteral("adaptiveC"), 5.0);
        if (!CircleMeasurementAlgorithm::measure(input, measure, overlay, options, &error))
            return failResult(error, DiagnosticCodes::measureFailed(), t.elapsed());
        measure = finalizeLengthMeasurement(measure, request, context);
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("image"), DataValue(overlay));
        r.outputs.insert(QStringLiteral("measurement"), DataValue(measure));
        if (measure.valid) {
            r.outputs.insert(QStringLiteral("diameter"), DataValue(measure.width));
            r.outputs.insert(QStringLiteral("radius"), DataValue(measure.width * 0.5));
            OverlayItem circle;
            circle.type = OverlayItemType::Rectangle;
            circle.rect = measure.boundingRect;
            circle.color = QColor(0, 200, 255);
            r.overlays.append(circle);
            r.outputs.insert(QStringLiteral("overlay"), DataValue(r.overlays));
        }
        r.measurement = measure;
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class LineMeasureExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::lineMeasure(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &context) override
    {
        QElapsedTimer t;
        t.start();
        QString error;
        QString code;
        VisionImage input = requireImageWithRoi(request, &error, &code);
        if (input.isEmpty())
            return failResult(error, code.isEmpty() ? DiagnosticCodes::imageEmpty() : code, t.elapsed());
        MeasurementResult measure;
        VisionImage overlay;
        const int cannyLow = resolveIntParam(request, QStringLiteral("cannyLow"), 50);
        const int cannyHigh = resolveIntParam(request, QStringLiteral("cannyHigh"), 150);
        const bool drawOverlay = resolveBoolParam(request, QStringLiteral("drawOverlay"), true);
        if (!LineMeasurementAlgorithm::measure(input, measure, overlay, cannyLow, cannyHigh,
                                               drawOverlay, &error))
            return failResult(error, DiagnosticCodes::measureFailed(), t.elapsed());
        measure = finalizeLengthMeasurement(measure, request, context);
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("image"), DataValue(overlay));
        r.outputs.insert(QStringLiteral("measurement"), DataValue(measure));
        if (measure.valid) {
            r.outputs.insert(QStringLiteral("length"), DataValue(measure.width));
            r.outputs.insert(QStringLiteral("angle"), DataValue(measure.angle));
            OverlayItem line;
            line.type = OverlayItemType::Line;
            line.points = {measure.boundingRect.topLeft(), measure.boundingRect.bottomRight()};
            line.color = QColor(255, 220, 0);
            r.overlays.append(line);
            r.outputs.insert(QStringLiteral("overlay"), DataValue(r.overlays));
        }
        r.measurement = measure;
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class DistanceMeasureExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::distanceMeasure(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &context) override
    {
        QElapsedTimer t;
        t.start();
        const double x1 = resolveRealInput(request, QStringLiteral("x1"), 0.0);
        const double y1 = resolveRealInput(request, QStringLiteral("y1"), 0.0);
        const double x2 = resolveRealInput(request, QStringLiteral("x2"), 100.0);
        const double y2 = resolveRealInput(request, QStringLiteral("y2"), 0.0);
        if (!std::isfinite(x1) || !std::isfinite(y1) || !std::isfinite(x2) || !std::isfinite(y2))
            return failResult(QStringLiteral("距离测量坐标无效"),
                              DiagnosticCodes::invalidParameter(), t.elapsed());
        const double distance = qHypot(x2 - x1, y2 - y1);

        MeasurementResult measure;
        measure.valid = true;
        measure.unit = QStringLiteral("px");
        measure.confidence = 1.0;
        measure.measurementType = QStringLiteral("distance");
        measure.decisionState = QStringLiteral("unknown");
        measure.width = distance;
        measure.height = 0.0;
        measure.area = 0.0;
        measure.perimeter = distance;
        measure.center = QPointF((x1 + x2) * 0.5, (y1 + y2) * 0.5);
        measure.boundingRect = QRectF(QPointF(x1, y1), QPointF(x2, y2)).normalized();
        measure.message = QStringLiteral("距离测量成功");
        measure = finalizeLengthMeasurement(measure, request, context);

        ExecutionResult r;
        r.outputs.insert(QStringLiteral("distance"), DataValue(measure.width));
        r.outputs.insert(QStringLiteral("measurement"), DataValue(measure));
        r.measurement = measure;
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class RangeDecisionExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::rangeDecision(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        ExecutionResult r;
        QElapsedTimer t;
        t.start();
        const double value = resolveRealInput(request, QStringLiteral("value"), 0.0);
        MeasurementDefinition def;
        def.hasTolerance = true;
        def.lower = request.parameters.value(QStringLiteral("lower")).toDouble(0.0);
        def.upper = request.parameters.value(QStringLiteral("upper")).toDouble(100.0);
        def.nominal = request.parameters.value(QStringLiteral("nominal")).toDouble(50.0);
        const QString state = def.evaluateDecision(value);
        const bool ok = state == QLatin1String("ok");
        r.outputs.insert(QStringLiteral("ok"), DataValue(ok));
        r.outputs.insert(QStringLiteral("state"), DataValue(state));
        MeasurementResult m;
        m.valid = true;
        m.measurementType = QStringLiteral("rangeDecision");
        m.width = value;
        m.decisionState = state;
        m.message = ok ? QStringLiteral("合格") : QStringLiteral("不合格");
        r.outputs.insert(QStringLiteral("measurement"), DataValue(m));
        r.measurement = m;
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class ResultPreviewExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::resultPreview(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        ExecutionResult r;
        r.outputs = request.inputs;
        if (request.inputs.contains(QStringLiteral("image")))
            r.outputs.insert(QStringLiteral("image"), request.inputs.value(QStringLiteral("image")));
        r.status = ModuleStatus::Success;
        return r;
    }
};

} // namespace

void registerBuiltInOpenCvExecutors()
{
    auto &reg = NodeExecutorRegistry::instance();
    reg.registerFactory(VisionNodeIds::imageLoader(), [] { return std::make_shared<ImageLoaderExecutor>(); });
    reg.registerFactory(VisionNodeIds::grayscale(), [] { return std::make_shared<GrayscaleExecutor>(); });
    reg.registerFactory(VisionNodeIds::threshold(), [] { return std::make_shared<ThresholdExecutor>(); });
    reg.registerFactory(VisionNodeIds::blur(), [] { return std::make_shared<BlurExecutor>(); });
    reg.registerFactory(VisionNodeIds::canny(), [] { return std::make_shared<CannyExecutor>(); });
    reg.registerFactory(VisionNodeIds::morphology(), [] { return std::make_shared<MorphologyExecutor>(); });
    reg.registerFactory(VisionNodeIds::resize(), [] { return std::make_shared<ResizeExecutor>(); });
    reg.registerFactory(VisionNodeIds::templateMatch(), [] { return std::make_shared<TemplateMatchExecutor>(); });
    reg.registerFactory(VisionNodeIds::templateSource(), [] { return std::make_shared<TemplateSourceExecutor>(); });
    reg.registerFactory(VisionNodeIds::rectangleMeasure(), [] { return std::make_shared<RectangleMeasureExecutor>(); });
    reg.registerFactory(VisionNodeIds::circleMeasure(), [] { return std::make_shared<CircleMeasureExecutor>(); });
    reg.registerFactory(VisionNodeIds::lineMeasure(), [] { return std::make_shared<LineMeasureExecutor>(); });
    reg.registerFactory(VisionNodeIds::distanceMeasure(), [] { return std::make_shared<DistanceMeasureExecutor>(); });
    reg.registerFactory(VisionNodeIds::rangeDecision(), [] { return std::make_shared<RangeDecisionExecutor>(); });
    reg.registerFactory(VisionNodeIds::resultPreview(), [] { return std::make_shared<ResultPreviewExecutor>(); });

    registerPreprocessExecutors(reg);
    registerRegionExecutors(reg);
    registerLocateExecutors(reg);
    registerAdvancedMeasureExecutors(reg);
    registerLogicOutputExecutors(reg);
    registerCommunicationExecutors(reg);
    registerRecognitionExecutors(reg);
}

} // namespace Selt
