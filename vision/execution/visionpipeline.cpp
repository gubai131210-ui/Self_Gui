#include "visionpipeline.h"

#include "vision/algorithms/grayscalealgorithm.h"
#include "vision/algorithms/imageloader.h"
#include "vision/algorithms/rectanglemeasurementalgorithm.h"
#include "vision/algorithms/thresholdalgorithm.h"
#include "vision/model/roi.h"
#include "vision/registry/visionnodeids.h"
#include "vision/registry/visionnoderegistry.h"

#include <QElapsedTimer>
#include <QHash>
#include <QSet>
#include <opencv2/imgproc.hpp>

static ThresholdMode thresholdModeFromString(const QString &mode)
{
    return mode == QLatin1String("binaryInv") ? ThresholdMode::BinaryInv : ThresholdMode::Binary;
}

static VisionImage applyRoiIfEnabled(const VisionImage &input, const QJsonObject &params)
{
    const RoiRect roi = RoiRect::fromJson(params.value(QStringLiteral("roi")).toObject());
    if (!roi.isValid() || input.isEmpty())
        return input;

    const cv::Mat &src = input.mat();
    QRectF clamped = roi.rect.intersected(QRectF(0, 0, src.cols, src.rows));
    if (clamped.width() < 1 || clamped.height() < 1)
        return input;

    cv::Rect cvRoi(static_cast<int>(clamped.x()),
                   static_cast<int>(clamped.y()),
                   static_cast<int>(clamped.width()),
                   static_cast<int>(clamped.height()));
    cv::Mat masked = cv::Mat::zeros(src.size(), src.type());
    src(cvRoi).copyTo(masked(cvRoi));
    return VisionImage(masked);
}

static OverlayList makeMeasurementOverlays(const MeasurementResult &result)
{
    OverlayList overlays;
    if (!result.valid)
        return overlays;

    OverlayItem box;
    box.type = OverlayItemType::Rectangle;
    box.rect = result.boundingRect;
    box.color = QColor(0, 220, 80);
    box.penWidth = 2.0;
    overlays.append(box);

    OverlayItem label;
    label.type = OverlayItemType::Text;
    label.rect = QRectF(result.boundingRect.topLeft(), QSizeF(160, 24));
    label.text = QStringLiteral("W:%1 H:%2 A:%3")
                     .arg(result.width, 0, 'f', 1)
                     .arg(result.height, 0, 'f', 1)
                     .arg(result.area, 0, 'f', 0);
    label.color = QColor(255, 220, 40);
    overlays.append(label);
    return overlays;
}

QStringList VisionPipeline::buildExecutionOrder(const Document &document)
{
    QHash<QString, QStringList> outgoing;
    QHash<QString, int> inDegree;
    QSet<QString> visionNodeIds;

    for (const NodeModel &node : document.nodes()) {
        if (!VisionNodeIds::isVisionType(node.type))
            continue;
        visionNodeIds.insert(node.id);
        inDegree.insert(node.id, 0);
    }

    for (const ConnectionModel &conn : document.connections()) {
        if (!visionNodeIds.contains(conn.sourceNodeId) || !visionNodeIds.contains(conn.targetNodeId))
            continue;
        outgoing[conn.sourceNodeId].append(conn.targetNodeId);
        inDegree[conn.targetNodeId] = inDegree.value(conn.targetNodeId) + 1;
    }

    QStringList queue;
    for (const QString &id : visionNodeIds) {
        if (inDegree.value(id) == 0)
            queue.append(id);
    }

    QStringList order;
    while (!queue.isEmpty()) {
        const QString current = queue.takeFirst();
        order.append(current);
        for (const QString &next : outgoing.value(current)) {
            inDegree[next] = inDegree.value(next) - 1;
            if (inDegree.value(next) == 0)
                queue.append(next);
        }
    }

    if (order.size() != visionNodeIds.size()) {
        for (const QString &id : visionNodeIds) {
            if (!order.contains(id))
                order.append(id);
        }
    }
    return order;
}

bool VisionPipeline::execute(const Document &document, VisionContext &context, ExecutionMode mode)
{
    context = VisionContext{};
    context.mode = mode;
    QElapsedTimer timer;
    timer.start();

    const QStringList order = buildExecutionOrder(document);
    context.executionOrder = order;
    if (order.isEmpty()) {
        context.setError(QStringLiteral("流程中没有视觉节点"));
        return false;
    }

    VisionImage currentImage;
    for (const QString &nodeId : order) {
        const NodeModel node = document.node(nodeId);
        ModuleRunResult result;
        result.nodeId = nodeId;
        result.type = node.type;
        result.displayName = VisionNodeRegistry::displayName(node.type);
        result.status = ModuleStatus::Running;

        QString paramError;
        if (!VisionNodeRegistry::validateParameters(node.type, node.parameters, &paramError)) {
            result.status = ModuleStatus::Failed;
            result.errorMessage = paramError;
            context.moduleResults.insert(nodeId, result);
            context.setError(paramError, nodeId);
            context.elapsedMs = timer.elapsed();
            return false;
        }

        QString error;
        QElapsedTimer nodeTimer;
        nodeTimer.start();

        if (node.type == VisionNodeIds::imageLoader()) {
            const QString path = node.parameters.value(QStringLiteral("path")).toString(node.imagePath);
            if (!ImageLoader::load(path, currentImage, &error)) {
                result.status = ModuleStatus::Failed;
                result.errorMessage = error;
                context.moduleResults.insert(nodeId, result);
                context.setError(error, nodeId);
                context.elapsedMs = timer.elapsed();
                return false;
            }
            context.originalImage = currentImage.clone();
            context.currentImage = currentImage.clone();
            result.outputImage = currentImage.clone();
            context.appendLog(QStringLiteral("[%1] 图片输入: %2").arg(result.displayName, path));
        } else if (node.type == VisionNodeIds::grayscale()) {
            VisionImage input = applyRoiIfEnabled(currentImage, node.parameters);
            VisionImage gray;
            if (!GrayscaleAlgorithm::convert(input, gray, &error)) {
                result.status = ModuleStatus::Failed;
                result.errorMessage = error;
                context.moduleResults.insert(nodeId, result);
                context.setError(error, nodeId);
                context.elapsedMs = timer.elapsed();
                return false;
            }
            currentImage = gray;
            context.currentImage = currentImage.clone();
            result.outputImage = currentImage.clone();
            context.appendLog(QStringLiteral("[%1] 灰度化完成").arg(result.displayName));
        } else if (node.type == VisionNodeIds::threshold()) {
            VisionImage input = applyRoiIfEnabled(currentImage, node.parameters);
            const int threshold = node.parameters.value(QStringLiteral("threshold")).toInt(128);
            const int maxValue = node.parameters.value(QStringLiteral("maxValue")).toInt(255);
            const ThresholdMode modeValue = thresholdModeFromString(
                node.parameters.value(QStringLiteral("mode")).toString(QStringLiteral("binary")));
            VisionImage binary;
            if (!ThresholdAlgorithm::apply(input, binary, threshold, maxValue, modeValue, &error)) {
                result.status = ModuleStatus::Failed;
                result.errorMessage = error;
                context.moduleResults.insert(nodeId, result);
                context.setError(error, nodeId);
                context.elapsedMs = timer.elapsed();
                return false;
            }
            currentImage = binary;
            context.binaryImage = currentImage.clone();
            context.currentImage = currentImage.clone();
            result.outputImage = currentImage.clone();
            context.appendLog(QStringLiteral("[%1] 二值化完成, threshold=%2")
                                  .arg(result.displayName)
                                  .arg(threshold));
        } else if (node.type == VisionNodeIds::rectangleMeasure()) {
            VisionImage input = applyRoiIfEnabled(currentImage, node.parameters);
            const double minArea = node.parameters.value(QStringLiteral("minArea")).toDouble(100.0);
            const bool drawOverlay = node.parameters.value(QStringLiteral("drawOverlay")).toBool(true);
            MeasurementResult measure;
            VisionImage overlay;
            if (!RectangleMeasurementAlgorithm::measure(input, measure, overlay, minArea, drawOverlay, &error)) {
                result.status = ModuleStatus::Failed;
                result.errorMessage = error;
                context.moduleResults.insert(nodeId, result);
                context.setError(error, nodeId);
                context.elapsedMs = timer.elapsed();
                return false;
            }
            context.measurement = measure;
            context.resultImage = overlay.clone();
            context.currentImage = overlay.clone();
            result.outputImage = overlay.clone();
            result.measurement = measure;
            result.overlays = makeMeasurementOverlays(measure);
            context.appendLog(QStringLiteral("[%1] 矩形测量: 宽=%2 高=%3 面积=%4")
                                  .arg(result.displayName)
                                  .arg(measure.width)
                                  .arg(measure.height)
                                  .arg(measure.area));
        } else if (node.type == VisionNodeIds::resultPreview()) {
            result.outputImage = currentImage.clone();
            context.appendLog(QStringLiteral("[%1] 结果预览").arg(result.displayName));
        } else {
            result.status = ModuleStatus::Failed;
            result.errorMessage = QStringLiteral("未知视觉模块: %1").arg(node.type);
            context.moduleResults.insert(nodeId, result);
            context.setError(result.errorMessage, nodeId);
            context.elapsedMs = timer.elapsed();
            return false;
        }

        result.status = ModuleStatus::Success;
        result.elapsedMs = nodeTimer.elapsed();
        context.moduleResults.insert(nodeId, result);
    }

    context.elapsedMs = timer.elapsed();
    context.appendLog(QStringLiteral("单次执行完成, 总耗时 %1 ms").arg(context.elapsedMs));
    return true;
}
