#include "vision/workflow/workflowtemplates.h"

#include "core/model/document.h"
#include "vision/registry/communicationnodeids.h"
#include "vision/registry/visionnodeids.h"
#include "vision/registry/visionnoderegistry.h"

namespace Selt {
namespace {

void link(QVector<ConnectionModel> &conns,
          const QString &src, const QString &srcPort,
          const QString &dst, const QString &dstPort)
{
    ConnectionModel c;
    c.id = Document::createId(QStringLiteral("conn"));
    c.sourceNodeId = src;
    c.sourcePortId = srcPort;
    c.targetNodeId = dst;
    c.targetPortId = dstPort;
    c.lineStyle = ConnectionLineStyle::Orthogonal;
    c.showArrow = true;
    conns.append(c);
}

NodeModel makeNode(const QString &type, const QPointF &pos)
{
    return VisionNodeRegistry::create(type, pos);
}

} // namespace

QStringList WorkflowTemplates::templateIds()
{
    return {
        QStringLiteral("measure_basic"),
        QStringLiteral("locate_measure"),
        QStringLiteral("caliper_decision"),
        QStringLiteral("full_chain"),
        QStringLiteral("blob_area_inspect"),
        QStringLiteral("blob_count_inspect"),
        QStringLiteral("geometry_circle_inspect"),
        QStringLiteral("vision_to_plc"),
        QStringLiteral("qr_preview"),
        QStringLiteral("ocr_preview"),
    };
}

QStringList WorkflowTemplates::categoryStageOrder()
{
    return {
        QStringLiteral("输入"),
        QStringLiteral("预处理"),
        QStringLiteral("区域"),
        QStringLiteral("定位"),
        QStringLiteral("识别"),
        QStringLiteral("测量"),
        QStringLiteral("判定"),
        QStringLiteral("逻辑"),
        QStringLiteral("通讯"),
        QStringLiteral("数据"),
        QStringLiteral("输出"),
    };
}

int WorkflowTemplates::categoryStageRank(const QString &category)
{
    const QStringList order = categoryStageOrder();
    const int idx = order.indexOf(category);
    return idx >= 0 ? idx : 100;
}

WorkflowTemplateSpec WorkflowTemplates::build(const QString &id, const QPointF &origin)
{
    WorkflowTemplateSpec spec;
    spec.id = id;
    const qreal dx = 220.0;

    if (id == QLatin1String("locate_measure")) {
        spec.displayName = QStringLiteral("定位→判定流程");
        spec.description = QStringLiteral("采集 → 灰度 → 二值化 → Blob定位 → 容差判定 → 结果预览");
        NodeModel loader = makeNode(VisionNodeIds::imageLoader(), origin);
        NodeModel gray = makeNode(VisionNodeIds::grayscale(), origin + QPointF(dx, 0));
        NodeModel thr = makeNode(VisionNodeIds::threshold(), origin + QPointF(dx * 2, 0));
        NodeModel blob = makeNode(VisionNodeIds::blobLocate(), origin + QPointF(dx * 3, 0));
        NodeModel decide = makeNode(VisionNodeIds::toleranceDecision(), origin + QPointF(dx * 4, 0));
        NodeModel preview = makeNode(VisionNodeIds::resultPreview(), origin + QPointF(dx * 5, 0));
        blob.parameters.insert(QStringLiteral("minArea"), 50.0);
        decide.parameters.insert(QStringLiteral("lower"), 20.0);
        decide.parameters.insert(QStringLiteral("upper"), 1e9);
        decide.parameters.insert(QStringLiteral("nominal"), 100.0);
        // Preview uses Blob image output; ResultWriter needs Measurement and is covered by full_chain.
        spec.nodes = {loader, gray, thr, blob, decide, preview};
        link(spec.connections, loader.id, QStringLiteral("out"), gray.id, QStringLiteral("in"));
        link(spec.connections, gray.id, QStringLiteral("out"), thr.id, QStringLiteral("in"));
        link(spec.connections, thr.id, QStringLiteral("out"), blob.id, QStringLiteral("in"));
        link(spec.connections, blob.id, QStringLiteral("area"), decide.id, QStringLiteral("value"));
        link(spec.connections, blob.id, QStringLiteral("out"), preview.id, QStringLiteral("in"));
        return spec;
    }

    if (id == QLatin1String("caliper_decision")) {
        spec.displayName = QStringLiteral("卡尺测距判定流程");
        spec.description = QStringLiteral("采集 → 灰度 → 卡尺测量 → 容差判定 → 结果预览");
        NodeModel loader = makeNode(VisionNodeIds::imageLoader(), origin);
        NodeModel gray = makeNode(VisionNodeIds::grayscale(), origin + QPointF(dx, 0));
        NodeModel caliper = makeNode(VisionNodeIds::caliperMeasure(), origin + QPointF(dx * 2, 0));
        NodeModel decide = makeNode(VisionNodeIds::toleranceDecision(), origin + QPointF(dx * 3, 0));
        NodeModel preview = makeNode(VisionNodeIds::resultPreview(), origin + QPointF(dx * 4, 0));
        caliper.parameters.insert(QStringLiteral("cx"), 100.0);
        caliper.parameters.insert(QStringLiteral("cy"), 100.0);
        caliper.parameters.insert(QStringLiteral("length"), 120.0);
        caliper.parameters.insert(QStringLiteral("width"), 8.0);
        decide.parameters.insert(QStringLiteral("lower"), 10.0);
        decide.parameters.insert(QStringLiteral("upper"), 200.0);
        decide.parameters.insert(QStringLiteral("nominal"), 70.0);
        spec.nodes = {loader, gray, caliper, decide, preview};
        link(spec.connections, loader.id, QStringLiteral("out"), gray.id, QStringLiteral("in"));
        link(spec.connections, gray.id, QStringLiteral("out"), caliper.id, QStringLiteral("in"));
        link(spec.connections, caliper.id, QStringLiteral("distance"), decide.id, QStringLiteral("value"));
        link(spec.connections, caliper.id, QStringLiteral("out"), preview.id, QStringLiteral("in"));
        return spec;
    }

    if (id == QLatin1String("full_chain")) {
        spec.displayName = QStringLiteral("完整测量链路");
        spec.description =
            QStringLiteral("采集 → 灰度 → 二值化 → 轮廓 → 矩形测量 → 容差判定 → 结果写出");
        NodeModel loader = makeNode(VisionNodeIds::imageLoader(), origin);
        NodeModel gray = makeNode(VisionNodeIds::grayscale(), origin + QPointF(dx, 0));
        NodeModel thr = makeNode(VisionNodeIds::threshold(), origin + QPointF(dx * 2, 0));
        NodeModel contours = makeNode(VisionNodeIds::findContours(), origin + QPointF(dx * 3, 0));
        NodeModel measure = makeNode(VisionNodeIds::rectangleMeasure(), origin + QPointF(dx * 4, 0));
        NodeModel decide = makeNode(VisionNodeIds::toleranceDecision(), origin + QPointF(dx * 5, 0));
        NodeModel writer = makeNode(VisionNodeIds::resultWriter(), origin + QPointF(dx * 6, 0));
        decide.parameters.insert(QStringLiteral("lower"), 50.0);
        decide.parameters.insert(QStringLiteral("upper"), 5000.0);
        decide.parameters.insert(QStringLiteral("nominal"), 500.0);
        spec.nodes = {loader, gray, thr, contours, measure, decide, writer};
        link(spec.connections, loader.id, QStringLiteral("out"), gray.id, QStringLiteral("in"));
        link(spec.connections, gray.id, QStringLiteral("out"), thr.id, QStringLiteral("in"));
        link(spec.connections, thr.id, QStringLiteral("out"), contours.id, QStringLiteral("in"));
        // Contours stays on the image path so the stage is not a dead branch.
        link(spec.connections, contours.id, QStringLiteral("out"), measure.id, QStringLiteral("in"));
        link(spec.connections, measure.id, QStringLiteral("width"), decide.id, QStringLiteral("value"));
        link(spec.connections, measure.id, QStringLiteral("measurement"), writer.id,
             QStringLiteral("measurement"));
        link(spec.connections, decide.id, QStringLiteral("state"), writer.id, QStringLiteral("state"));
        return spec;
    }

    if (id == QLatin1String("blob_area_inspect")) {
        spec.displayName = QStringLiteral("单Blob面积检测");
        spec.description = QStringLiteral("采集 → 灰度 → 二值化 → Blob分析 → 容差判定 → 结果打包");
        NodeModel loader = makeNode(VisionNodeIds::imageLoader(), origin);
        NodeModel gray = makeNode(VisionNodeIds::grayscale(), origin + QPointF(dx, 0));
        NodeModel thr = makeNode(VisionNodeIds::threshold(), origin + QPointF(dx * 2, 0));
        NodeModel blob = makeNode(VisionNodeIds::blobAnalyze(), origin + QPointF(dx * 3, 0));
        NodeModel decide = makeNode(VisionNodeIds::toleranceDecision(), origin + QPointF(dx * 4, 0));
        NodeModel pack = makeNode(VisionNodeIds::inspectionPack(), origin + QPointF(dx * 5, 0));
        blob.parameters.insert(QStringLiteral("minArea"), 50.0);
        blob.parameters.insert(QStringLiteral("selectIndex"), 0);
        blob.parameters.insert(QStringLiteral("sortBy"), QStringLiteral("area"));
        blob.parameters.insert(QStringLiteral("requireTarget"), true);
        decide.parameters.insert(QStringLiteral("lower"), 100.0);
        decide.parameters.insert(QStringLiteral("upper"), 50000.0);
        decide.parameters.insert(QStringLiteral("nominal"), 1000.0);
        spec.nodes = {loader, gray, thr, blob, decide, pack};
        link(spec.connections, loader.id, QStringLiteral("out"), gray.id, QStringLiteral("in"));
        link(spec.connections, gray.id, QStringLiteral("out"), thr.id, QStringLiteral("in"));
        link(spec.connections, thr.id, QStringLiteral("out"), blob.id, QStringLiteral("in"));
        link(spec.connections, blob.id, QStringLiteral("area"), decide.id, QStringLiteral("value"));
        link(spec.connections, decide.id, QStringLiteral("ok"), pack.id, QStringLiteral("ok"));
        link(spec.connections, blob.id, QStringLiteral("area"), pack.id, QStringLiteral("value"));
        return spec;
    }

    if (id == QLatin1String("blob_count_inspect")) {
        spec.displayName = QStringLiteral("多Blob数量检测");
        spec.description = QStringLiteral("采集 → 灰度 → 二值化 → Blob分析 → 数量判定 → 结果打包");
        NodeModel loader = makeNode(VisionNodeIds::imageLoader(), origin);
        NodeModel gray = makeNode(VisionNodeIds::grayscale(), origin + QPointF(dx, 0));
        NodeModel thr = makeNode(VisionNodeIds::threshold(), origin + QPointF(dx * 2, 0));
        NodeModel blob = makeNode(VisionNodeIds::blobAnalyze(), origin + QPointF(dx * 3, 0));
        NodeModel decide = makeNode(VisionNodeIds::rangeDecision(), origin + QPointF(dx * 4, 0));
        NodeModel pack = makeNode(VisionNodeIds::inspectionPack(), origin + QPointF(dx * 5, 0));
        blob.parameters.insert(QStringLiteral("minArea"), 30.0);
        blob.parameters.insert(QStringLiteral("maxCount"), 50);
        decide.parameters.insert(QStringLiteral("lower"), 1.0);
        decide.parameters.insert(QStringLiteral("upper"), 20.0);
        decide.parameters.insert(QStringLiteral("nominal"), 5.0);
        spec.nodes = {loader, gray, thr, blob, decide, pack};
        link(spec.connections, loader.id, QStringLiteral("out"), gray.id, QStringLiteral("in"));
        link(spec.connections, gray.id, QStringLiteral("out"), thr.id, QStringLiteral("in"));
        link(spec.connections, thr.id, QStringLiteral("out"), blob.id, QStringLiteral("in"));
        link(spec.connections, blob.id, QStringLiteral("count"), decide.id, QStringLiteral("value"));
        link(spec.connections, decide.id, QStringLiteral("ok"), pack.id, QStringLiteral("ok"));
        link(spec.connections, blob.id, QStringLiteral("count"), pack.id, QStringLiteral("value"));
        return spec;
    }

    if (id == QLatin1String("geometry_circle_inspect")) {
        spec.displayName = QStringLiteral("圆几何尺寸检测");
        spec.description = QStringLiteral("采集 → 灰度 → Hough圆 → 位置偏差 → 容差判定");
        NodeModel loader = makeNode(VisionNodeIds::imageLoader(), origin);
        NodeModel gray = makeNode(VisionNodeIds::grayscale(), origin + QPointF(dx, 0));
        NodeModel circle = makeNode(VisionNodeIds::houghCircles(), origin + QPointF(dx * 2, 0));
        NodeModel ref = makeNode(VisionNodeIds::referencePoint(), origin + QPointF(dx * 3, -80));
        NodeModel deviate = makeNode(VisionNodeIds::positionDeviation(), origin + QPointF(dx * 3, 80));
        NodeModel decide = makeNode(VisionNodeIds::toleranceDecision(), origin + QPointF(dx * 4, 0));
        ref.parameters.insert(QStringLiteral("x"), 320.0);
        ref.parameters.insert(QStringLiteral("y"), 240.0);
        decide.parameters.insert(QStringLiteral("lower"), 0.0);
        decide.parameters.insert(QStringLiteral("upper"), 15.0);
        decide.parameters.insert(QStringLiteral("nominal"), 0.0);
        spec.nodes = {loader, gray, circle, ref, deviate, decide};
        link(spec.connections, loader.id, QStringLiteral("out"), gray.id, QStringLiteral("in"));
        link(spec.connections, gray.id, QStringLiteral("out"), circle.id, QStringLiteral("in"));
        link(spec.connections, circle.id, QStringLiteral("center"), deviate.id, QStringLiteral("point"));
        link(spec.connections, ref.id, QStringLiteral("point"), deviate.id, QStringLiteral("reference"));
        link(spec.connections, deviate.id, QStringLiteral("distance"), decide.id, QStringLiteral("value"));
        return spec;
    }

    if (id == QLatin1String("vision_to_plc")) {
        spec.displayName = QStringLiteral("视觉判定→PLC写出");
        spec.description =
            QStringLiteral("采集 → 灰度 → 二值化 → Blob定位 → 容差判定 → Modbus写出（默认 Mock）");
        NodeModel loader = makeNode(VisionNodeIds::imageLoader(), origin);
        NodeModel gray = makeNode(VisionNodeIds::grayscale(), origin + QPointF(dx, 0));
        NodeModel thr = makeNode(VisionNodeIds::threshold(), origin + QPointF(dx * 2, 0));
        NodeModel blob = makeNode(VisionNodeIds::blobLocate(), origin + QPointF(dx * 3, 0));
        NodeModel decide = makeNode(VisionNodeIds::toleranceDecision(), origin + QPointF(dx * 4, 0));
        NodeModel write = makeNode(CommunicationNodeIds::modbusWrite(), origin + QPointF(dx * 5, 0));
        blob.parameters.insert(QStringLiteral("minArea"), 50.0);
        decide.parameters.insert(QStringLiteral("lower"), 20.0);
        decide.parameters.insert(QStringLiteral("upper"), 1e9);
        decide.parameters.insert(QStringLiteral("nominal"), 100.0);
        write.parameters.insert(QStringLiteral("useMock"), true);
        write.parameters.insert(QStringLiteral("allowWrite"), false);
        write.parameters.insert(QStringLiteral("address"), 0);
        write.parameters.insert(QStringLiteral("valueKind"), QStringLiteral("coil"));
        write.parameters.insert(QStringLiteral("table"), QStringLiteral("coils"));
        spec.nodes = {loader, gray, thr, blob, decide, write};
        link(spec.connections, loader.id, QStringLiteral("out"), gray.id, QStringLiteral("in"));
        link(spec.connections, gray.id, QStringLiteral("out"), thr.id, QStringLiteral("in"));
        link(spec.connections, thr.id, QStringLiteral("out"), blob.id, QStringLiteral("in"));
        link(spec.connections, blob.id, QStringLiteral("area"), decide.id, QStringLiteral("value"));
        link(spec.connections, decide.id, QStringLiteral("ok"), write.id, QStringLiteral("okIn"));
        return spec;
    }

    if (id == QLatin1String("qr_preview")) {
        spec.displayName = QStringLiteral("二维码识别预览");
        spec.description = QStringLiteral("采集 → 条码/二维码识别 → 结果预览 / 变量写回");
        NodeModel loader = makeNode(VisionNodeIds::imageLoader(), origin);
        NodeModel decode = makeNode(VisionNodeIds::barcodeDecode(), origin + QPointF(dx, 0));
        NodeModel preview = makeNode(VisionNodeIds::resultPreview(), origin + QPointF(dx * 2, 0));
        NodeModel write = makeNode(VisionNodeIds::variableWrite(), origin + QPointF(dx * 2, 120));
        decode.parameters.insert(QStringLiteral("symbology"), QStringLiteral("qr"));
        write.parameters.insert(QStringLiteral("key"), QStringLiteral("qr_text"));
        spec.nodes = {loader, decode, preview, write};
        link(spec.connections, loader.id, QStringLiteral("out"), decode.id, QStringLiteral("in"));
        link(spec.connections, decode.id, QStringLiteral("out"), preview.id, QStringLiteral("in"));
        link(spec.connections, decode.id, QStringLiteral("text"), write.id, QStringLiteral("value"));
        return spec;
    }

    if (id == QLatin1String("ocr_preview")) {
        spec.displayName = QStringLiteral("OCR识别预览");
        spec.description = QStringLiteral("采集 → OCR识别 → 结果预览 / 变量写回（需 Tesseract）");
        NodeModel loader = makeNode(VisionNodeIds::imageLoader(), origin);
        NodeModel ocr = makeNode(VisionNodeIds::ocr(), origin + QPointF(dx, 0));
        NodeModel preview = makeNode(VisionNodeIds::resultPreview(), origin + QPointF(dx * 2, 0));
        NodeModel write = makeNode(VisionNodeIds::variableWrite(), origin + QPointF(dx * 2, 120));
        ocr.parameters.insert(QStringLiteral("language"), QStringLiteral("eng"));
        write.parameters.insert(QStringLiteral("key"), QStringLiteral("ocr_text"));
        spec.nodes = {loader, ocr, preview, write};
        link(spec.connections, loader.id, QStringLiteral("out"), ocr.id, QStringLiteral("in"));
        link(spec.connections, ocr.id, QStringLiteral("out"), preview.id, QStringLiteral("in"));
        link(spec.connections, ocr.id, QStringLiteral("text"), write.id, QStringLiteral("value"));
        return spec;
    }

    // Default: measure_basic
    spec.id = QStringLiteral("measure_basic");
    spec.displayName = QStringLiteral("基础测量流程");
    spec.description = QStringLiteral("采集 → 灰度 → 二值化 → 矩形测量 → 容差判定");
    NodeModel loader = makeNode(VisionNodeIds::imageLoader(), origin);
    NodeModel gray = makeNode(VisionNodeIds::grayscale(), origin + QPointF(dx, 0));
    NodeModel thr = makeNode(VisionNodeIds::threshold(), origin + QPointF(dx * 2, 0));
    NodeModel measure = makeNode(VisionNodeIds::rectangleMeasure(), origin + QPointF(dx * 3, 0));
    NodeModel decide = makeNode(VisionNodeIds::toleranceDecision(), origin + QPointF(dx * 4, 0));
    decide.parameters.insert(QStringLiteral("lower"), 50.0);
    decide.parameters.insert(QStringLiteral("upper"), 5000.0);
    decide.parameters.insert(QStringLiteral("nominal"), 500.0);
    spec.nodes = {loader, gray, thr, measure, decide};
    link(spec.connections, loader.id, QStringLiteral("out"), gray.id, QStringLiteral("in"));
    link(spec.connections, gray.id, QStringLiteral("out"), thr.id, QStringLiteral("in"));
    link(spec.connections, thr.id, QStringLiteral("out"), measure.id, QStringLiteral("in"));
    link(spec.connections, measure.id, QStringLiteral("width"), decide.id, QStringLiteral("value"));
    return spec;
}

} // namespace Selt
