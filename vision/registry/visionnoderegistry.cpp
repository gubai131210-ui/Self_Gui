#include "visionnoderegistry.h"

#include "core/model/document.h"
#include "vision/model/modulevisualstyle.h"
#include "vision/model/portexposure.h"
#include "vision/model/subflowsupport.h"
#include "vision/registry/visionnodeids.h"
#include "vision/registry/communicationnodeids.h"
#include "vision/workflow/workflowtemplates.h"

#include <QHash>
#include <QList>
#include <QPair>
#include <QtMath>
#include <algorithm>

static ModuleParamDef makeIntParam(const QString &key, const QString &label, int def,
                                   int minV, int maxV, int order,
                                   const QString &group = {}, const QString &tooltip = {})
{
    ModuleParamDef p;
    p.key = key;
    p.label = label;
    p.type = ModuleParamType::Int;
    p.defaultValue = def;
    p.minimum = minV;
    p.maximum = maxV;
    p.displayOrder = order;
    p.group = group;
    p.tooltip = tooltip;
    return p;
}

namespace {

qreal portSlotY(int index, int count)
{
    if (count <= 1)
        return 0.5;
    return static_cast<qreal>(index + 1) / static_cast<qreal>(count + 1);
}

void layoutPortsFromDescriptor(const ModuleDescriptor &desc, QList<PortModel> &ports)
{
    int inputCount = 0;
    int outputCount = 0;
    for (const ModulePortDef &portDef : desc.ports) {
        if (portDef.isInput)
            ++inputCount;
        else
            ++outputCount;
    }

    int inputIndex = 0;
    int outputIndex = 0;
    for (PortModel &port : ports) {
        const ModulePortDef *portDef = desc.findPort(port.id);
        if (!portDef)
            continue;
        if (portDef->isInput) {
            port.relativeX = 0.0;
            port.relativeY = portSlotY(inputIndex++, inputCount);
        } else {
            port.relativeX = 1.0;
            port.relativeY = portSlotY(outputIndex++, outputCount);
        }
    }
}

bool portsLayoutChanged(const QList<PortModel> &before, const QList<PortModel> &after)
{
    if (before.size() != after.size())
        return true;
    for (int i = 0; i < before.size(); ++i) {
        if (!qFuzzyCompare(before.at(i).relativeX, after.at(i).relativeX)
            || !qFuzzyCompare(before.at(i).relativeY, after.at(i).relativeY)) {
            return true;
        }
    }
    return false;
}

bool inputPortWired(const QVector<ConnectionModel> &connections,
                    const QString &nodeId,
                    const QString &portId)
{
    for (const ConnectionModel &conn : connections) {
        if (conn.targetNodeId != nodeId)
            continue;
        if (conn.targetPortId == portId)
            return true;
        if (portId == QLatin1String("in") && conn.targetPortId == QLatin1String("image"))
            return true;
        if (portId == QLatin1String("image") && conn.targetPortId == QLatin1String("in"))
            return true;
    }
    return false;
}

} // namespace

static ModuleParamDef makeDoubleParam(const QString &key, const QString &label, double def,
                                      double minV, double maxV, int order,
                                      const QString &group = {}, const QString &tooltip = {})
{
    ModuleParamDef p;
    p.key = key;
    p.label = label;
    p.type = ModuleParamType::Double;
    p.defaultValue = def;
    p.minimum = minV;
    p.maximum = maxV;
    p.displayOrder = order;
    p.group = group;
    p.tooltip = tooltip;
    return p;
}

static ModuleParamDef makeBoolParam(const QString &key, const QString &label, bool def, int order,
                                    const QString &group = {}, const QString &tooltip = {})
{
    ModuleParamDef p;
    p.key = key;
    p.label = label;
    p.type = ModuleParamType::Bool;
    p.defaultValue = def;
    p.displayOrder = order;
    p.group = group;
    p.tooltip = tooltip;
    return p;
}

static ModuleParamDef makeFileParam(const QString &key, const QString &label, int order,
                                    const QString &group = {}, const QString &tooltip = {})
{
    ModuleParamDef p;
    p.key = key;
    p.label = label;
    p.type = ModuleParamType::FilePath;
    p.defaultValue = QString();
    p.fileFilter = QStringLiteral("图片 (*.png *.jpg *.jpeg *.bmp *.tif *.tiff);;所有文件 (*.*)");
    p.displayOrder = order;
    p.group = group;
    p.tooltip = tooltip;
    return p;
}

static ModuleParamDef makeEnumParam(const QString &key, const QString &label,
                                    const QStringList &values,
                                    const QStringList &labels,
                                    const QString &def, int order,
                                    const QString &group = {}, const QString &tooltip = {})
{
    ModuleParamDef p;
    p.key = key;
    p.label = label;
    p.type = ModuleParamType::Enum;
    p.enumValues = values;
    p.defaultValue = def;
    p.displayOrder = order;
    p.group = group;
    p.tooltip = tooltip;
    for (int i = 0; i < values.size(); ++i) {
        ModuleParamOption opt;
        opt.value = values.at(i);
        opt.label = (i < labels.size()) ? labels.at(i) : values.at(i);
        p.enumOptions.append(opt);
    }
    return p;
}

static ModuleParamDef makeEnumParam(const QString &key, const QString &label,
                                    const QList<QPair<QString, QString>> &options,
                                    const QString &def, int order,
                                    const QString &group = {}, const QString &tooltip = {})
{
    QStringList values;
    QStringList labels;
    values.reserve(options.size());
    labels.reserve(options.size());
    for (const auto &opt : options) {
        values.append(opt.first);
        labels.append(opt.second);
    }
    return makeEnumParam(key, label, values, labels, def, order, group, tooltip);
}

static ModuleParamDef makeRoiApplyModeParam(int order,
                                            const QString &group = QStringLiteral("ROI"))
{
    return makeEnumParam(QStringLiteral("roiApplyMode"), QStringLiteral("ROI应用模式"),
                         {QStringLiteral("mask"), QStringLiteral("crop")},
                         {QStringLiteral("掩膜外部"), QStringLiteral("裁剪")},
                         QStringLiteral("mask"), order, group,
                         QStringLiteral("mask=保留全图并置黑外部；crop=裁剪到 ROI 包围盒"));
}

static ModuleParamDef makeRoiParam(const QString &key, const QString &label, int order,
                                   const QString &group = QStringLiteral("ROI"))
{
    ModuleParamDef p;
    p.key = key;
    p.label = label;
    p.type = ModuleParamType::RoiRect;
    p.defaultValue = QJsonObject{
        {QStringLiteral("x"), 0},
        {QStringLiteral("y"), 0},
        {QStringLiteral("width"), 0},
        {QStringLiteral("height"), 0},
        {QStringLiteral("enabled"), false},
        {QStringLiteral("locked"), false}};
    p.displayOrder = order;
    p.group = group;
    p.tooltip = QStringLiteral("感兴趣区域；可在结果图 Ctrl+拖拽绘制，缩小搜索范围");
    return p;
}

static ModuleParamDef makeStringParam(const QString &key, const QString &label,
                                      const QString &def, int order, const QString &tooltip = {},
                                      const QString &group = {})
{
    ModuleParamDef p;
    p.key = key;
    p.label = label;
    p.type = ModuleParamType::String;
    p.defaultValue = def;
    p.displayOrder = order;
    p.tooltip = tooltip;
    p.group = group;
    return p;
}

static ModuleDescriptor makeBase(const QString &typeId, const QString &name,
                                 const QString &category, const QString &desc)
{
    ModuleDescriptor d;
    d.typeId = typeId;
    d.displayName = name;
    d.category = category;
    d.description = desc;
    d.helpText = desc;
    d.fixedSize = VisionNodeRegistry::fixedNodeSize();
    d.fillColor = Selt::fillColorForCategory(category);
    d.iconKey = typeId; // per-operator SVG under resources/icons/vision/
    d.accentColor = Selt::accentColorForCategory(category);
    d.borderColor = d.accentColor;
    if (category == QStringLiteral("测量") || category == QStringLiteral("定位")) {
        d.uiSchema.previewLayers = {QStringLiteral("image"), QStringLiteral("geometry"),
                                    QStringLiteral("measurement")};
        d.uiSchema.capabilityTags = {QStringLiteral("teachable")};
    } else if (category == QStringLiteral("预处理") || category == QStringLiteral("区域")) {
        d.uiSchema.previewLayers = {QStringLiteral("image"), QStringLiteral("contour")};
    } else     if (category == QStringLiteral("输入")) {
        d.uiSchema.previewLayers = {QStringLiteral("image")};
    }
    d.ports = {
        {QStringLiteral("in"), QStringLiteral("输入"), true, Selt::DataTypeId::Image, true,
         QStringLiteral("输入图像")},
        {QStringLiteral("out"), QStringLiteral("输出"), false, Selt::DataTypeId::Image, false,
         QStringLiteral("输出图像")}};
    return d;
}

static QVector<ModuleDescriptor> buildDescriptors()
{
    QVector<ModuleDescriptor> list;

    {
        ModuleDescriptor d = makeBase(VisionNodeIds::imageLoader(), QStringLiteral("图片输入"),
                                      QStringLiteral("输入"), QStringLiteral("从本地文件加载图像"));
        d.ports = {{QStringLiteral("out"), QStringLiteral("图像"), false, Selt::DataTypeId::Image, false,
                    QStringLiteral("加载的图像")}};
        d.parameters = {makeFileParam(QStringLiteral("path"), QStringLiteral("图片路径"), 0)};
        d.outputKeys = {QStringLiteral("image")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::grayscale(), QStringLiteral("灰度化"),
                                      QStringLiteral("预处理"), QStringLiteral("彩色图转灰度图"));
        d.parameters = {makeRoiParam(QStringLiteral("roi"), QStringLiteral("ROI"), 0)};
        d.inputKeys = {QStringLiteral("image")};
        d.outputKeys = {QStringLiteral("image")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::threshold(), QStringLiteral("二值化"),
                                      QStringLiteral("预处理"), QStringLiteral("灰度图阈值分割"));
        d.parameters = {
            makeIntParam(QStringLiteral("threshold"), QStringLiteral("阈值"), 128, 0, 255, 0,
                         QStringLiteral("阈值"), QStringLiteral("手动阈值；Otsu/自适应模式下仍作回退参考")),
            makeIntParam(QStringLiteral("maxValue"), QStringLiteral("最大值"), 255, 0, 255, 1,
                         QStringLiteral("阈值"), QStringLiteral("二值化前景填充值")),
            makeEnumParam(QStringLiteral("mode"), QStringLiteral("阈值模式"),
                          {QStringLiteral("binary"), QStringLiteral("binaryInv")},
                          {QStringLiteral("Binary"), QStringLiteral("BinaryInv")},
                          QStringLiteral("binary"), 2, QStringLiteral("阈值"),
                          QStringLiteral("Binary=亮前景；BinaryInv=暗前景")),
            makeRoiParam(QStringLiteral("roi"), QStringLiteral("ROI"), 3),
            makeRoiApplyModeParam(4)};
        d.inputKeys = {QStringLiteral("image")};
        d.outputKeys = {QStringLiteral("image")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::blur(), QStringLiteral("模糊"),
                                      QStringLiteral("预处理"), QStringLiteral("均值模糊滤波"));
        d.parameters = {
            makeIntParam(QStringLiteral("ksize"), QStringLiteral("核大小"), 5, 1, 51, 0),
            makeRoiParam(QStringLiteral("roi"), QStringLiteral("ROI"), 1)};
        d.inputKeys = {QStringLiteral("image")};
        d.outputKeys = {QStringLiteral("image")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::canny(), QStringLiteral("Canny边缘"),
                                      QStringLiteral("预处理"), QStringLiteral("Canny 边缘检测"));
        d.parameters = {
            makeDoubleParam(QStringLiteral("threshold1"), QStringLiteral("阈值1"), 50.0, 0.0, 1000.0, 0,
                            QStringLiteral("边缘"), QStringLiteral("Canny 低阈值")),
            makeDoubleParam(QStringLiteral("threshold2"), QStringLiteral("阈值2"), 150.0, 0.0, 1000.0, 1,
                            QStringLiteral("边缘"), QStringLiteral("Canny 高阈值")),
            makeIntParam(QStringLiteral("apertureSize"), QStringLiteral("Sobel孔径"), 3, 3, 7, 2,
                         QStringLiteral("边缘"), QStringLiteral("必须为 3/5/7；偶数将回退为 3")),
            makeRoiParam(QStringLiteral("roi"), QStringLiteral("ROI"), 3),
            makeRoiApplyModeParam(4)};
        d.inputKeys = {QStringLiteral("image")};
        d.outputKeys = {QStringLiteral("image")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::morphology(), QStringLiteral("形态学"),
                                      QStringLiteral("预处理"), QStringLiteral("腐蚀/膨胀/开/闭运算"));
        d.parameters = {
            makeEnumParam(QStringLiteral("op"), QStringLiteral("运算"),
                          {QStringLiteral("erode"), QStringLiteral("dilate"),
                           QStringLiteral("open"), QStringLiteral("close")},
                          {QStringLiteral("腐蚀"), QStringLiteral("膨胀"),
                           QStringLiteral("开运算"), QStringLiteral("闭运算")},
                          QStringLiteral("erode"), 0, QStringLiteral("形态学")),
            makeIntParam(QStringLiteral("ksize"), QStringLiteral("核大小"), 3, 1, 51, 1,
                         QStringLiteral("形态学"), QStringLiteral("奇数核更稳；偶数会自动+1")),
            makeIntParam(QStringLiteral("iterations"), QStringLiteral("迭代次数"), 1, 1, 20, 2,
                         QStringLiteral("形态学"), QStringLiteral("腐蚀/膨胀/开闭重复次数")),
            makeRoiParam(QStringLiteral("roi"), QStringLiteral("ROI"), 3),
            makeRoiApplyModeParam(4)};
        d.inputKeys = {QStringLiteral("image")};
        d.outputKeys = {QStringLiteral("image")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::resize(), QStringLiteral("缩放"),
                                      QStringLiteral("预处理"), QStringLiteral("调整图像尺寸"));
        d.parameters = {
            makeIntParam(QStringLiteral("width"), QStringLiteral("宽度"), 640, 1, 8192, 0),
            makeIntParam(QStringLiteral("height"), QStringLiteral("高度"), 480, 1, 8192, 1)};
        d.inputKeys = {QStringLiteral("image")};
        d.outputKeys = {QStringLiteral("image")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::templateMatch(), QStringLiteral("模板匹配"),
                                      QStringLiteral("测量"), QStringLiteral("归一化相关系数模板匹配"));
        d.ports = {
            {QStringLiteral("in"), QStringLiteral("图像"), true, Selt::DataTypeId::Image, true,
             QStringLiteral("待搜索图像")},
            {QStringLiteral("template"), QStringLiteral("模板"), true, Selt::DataTypeId::Image, false,
             QStringLiteral("模板图像（可改用模板路径/资源参数）")},
            {QStringLiteral("out"), QStringLiteral("图像"), false, Selt::DataTypeId::Image, false,
             QStringLiteral("标注后的图像")},
            {QStringLiteral("score"), QStringLiteral("得分"), false, Selt::DataTypeId::Real, false,
             QStringLiteral("匹配峰值")},
            {QStringLiteral("center"), QStringLiteral("中心"), false, Selt::DataTypeId::Point2D, false,
             QStringLiteral("匹配中心点")},
            {QStringLiteral("region"), QStringLiteral("区域"), false, Selt::DataTypeId::Region, false,
             QStringLiteral("匹配区域（供下游 ROI/几何）")},
            {QStringLiteral("overlay"), QStringLiteral("叠加"), false, Selt::DataTypeId::Overlay, false,
             QStringLiteral("匹配框叠加")}};
        d.parameters = {
            makeFileParam(QStringLiteral("templatePath"), QStringLiteral("模板路径"), 0),
            makeStringParam(QStringLiteral("templateResourceId"), QStringLiteral("模板资源ID"),
                            {}, 1, QStringLiteral("优先于文件路径；由教示模板写入")),
            makeDoubleParam(QStringLiteral("minScore"), QStringLiteral("最低得分"), 0.5, 0.0, 1.0, 2),
            makeBoolParam(QStringLiteral("drawOverlay"), QStringLiteral("绘制匹配框"), true, 3)};
        d.inputKeys = {QStringLiteral("image")};
        d.outputKeys = {QStringLiteral("image"), QStringLiteral("score"), QStringLiteral("center"),
                        QStringLiteral("region"), QStringLiteral("overlay")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::templateSource(), QStringLiteral("模板源"),
                                      QStringLiteral("输入"),
                                      QStringLiteral("从项目资源或文件加载模板图像"));
        d.ports = {{QStringLiteral("out"), QStringLiteral("模板"), false, Selt::DataTypeId::Image, false,
                    QStringLiteral("模板图像")}};
        d.parameters = {
            makeFileParam(QStringLiteral("templatePath"), QStringLiteral("模板路径"), 0),
            makeStringParam(QStringLiteral("templateResourceId"), QStringLiteral("模板资源ID"),
                            {}, 1, QStringLiteral("项目内相对资源，由教示模板生成"))};
        d.outputKeys = {QStringLiteral("image")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::rectangleMeasure(), QStringLiteral("矩形测量"),
                                      QStringLiteral("测量"), QStringLiteral("检测最大轮廓矩形并测量"));
        d.ports = {
            {QStringLiteral("in"), QStringLiteral("输入"), true, Selt::DataTypeId::Image, true,
             QStringLiteral("输入图像")},
            {QStringLiteral("out"), QStringLiteral("图像"), false, Selt::DataTypeId::Image, false,
             QStringLiteral("结果图像")},
            {QStringLiteral("measurement"), QStringLiteral("测量"), false, Selt::DataTypeId::Measurement,
             false, QStringLiteral("测量结果")},
            {QStringLiteral("width"), QStringLiteral("宽度"), false, Selt::DataTypeId::Real, false,
             QStringLiteral("矩形宽度")},
            {QStringLiteral("height"), QStringLiteral("高度"), false, Selt::DataTypeId::Real, false,
             QStringLiteral("矩形高度")},
            {QStringLiteral("overlay"), QStringLiteral("叠加"), false, Selt::DataTypeId::Overlay, false,
             QStringLiteral("叠加绘制")}};
        d.parameters = {
            makeDoubleParam(QStringLiteral("minArea"), QStringLiteral("最小面积"), 100.0, 0.0, 1e9, 0,
                            QStringLiteral("筛选")),
            makeBoolParam(QStringLiteral("drawOverlay"), QStringLiteral("绘制测量结果"), true, 1,
                          QStringLiteral("显示")),
            makeEnumParam(QStringLiteral("thresholdStrategy"), QStringLiteral("阈值策略"),
                          {QStringLiteral("manual"), QStringLiteral("otsu"), QStringLiteral("adaptive")},
                          {QStringLiteral("手动"), QStringLiteral("Otsu"), QStringLiteral("自适应")},
                          QStringLiteral("manual"), 2, QStringLiteral("阈值")),
            makeIntParam(QStringLiteral("threshold"), QStringLiteral("阈值"), 128, 0, 255, 3,
                         QStringLiteral("阈值")),
            makeIntParam(QStringLiteral("adaptiveBlockSize"), QStringLiteral("自适应块大小"), 31, 3, 101, 4,
                         QStringLiteral("阈值")),
            makeDoubleParam(QStringLiteral("adaptiveC"), QStringLiteral("自适应C"), 5.0, -50.0, 50.0, 5,
                            QStringLiteral("阈值")),
            makeRoiParam(QStringLiteral("roi"), QStringLiteral("ROI"), 6),
            makeRoiApplyModeParam(7),
            makeStringParam(QStringLiteral("calibrationHint"), QStringLiteral("标定提示"),
                            QStringLiteral("物理单位需有效标定快照"), 8,
                            QStringLiteral("无标定时结果单位保持 px，不伪造 mm"),
                            QStringLiteral("标定"))};
        d.inputKeys = {QStringLiteral("image")};
        d.outputKeys = {QStringLiteral("image"), QStringLiteral("measurement"), QStringLiteral("overlay")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::circleMeasure(), QStringLiteral("圆测量"),
                                      QStringLiteral("测量"), QStringLiteral("检测最大近似圆并测量直径/半径"));
        d.ports = {
            {QStringLiteral("in"), QStringLiteral("输入"), true, Selt::DataTypeId::Image, true,
             QStringLiteral("输入图像")},
            {QStringLiteral("out"), QStringLiteral("图像"), false, Selt::DataTypeId::Image, false,
             QStringLiteral("结果图像")},
            {QStringLiteral("measurement"), QStringLiteral("测量"), false, Selt::DataTypeId::Measurement,
             false, QStringLiteral("测量结果")},
            {QStringLiteral("diameter"), QStringLiteral("直径"), false, Selt::DataTypeId::Real, false,
             QStringLiteral("圆直径")},
            {QStringLiteral("radius"), QStringLiteral("半径"), false, Selt::DataTypeId::Real, false,
             QStringLiteral("圆半径")},
            {QStringLiteral("overlay"), QStringLiteral("叠加"), false, Selt::DataTypeId::Overlay, false,
             QStringLiteral("叠加绘制")}};
        d.parameters = {
            makeDoubleParam(QStringLiteral("minRadius"), QStringLiteral("最小半径"), 5.0, 0.0, 1e6, 0,
                            QStringLiteral("筛选")),
            makeBoolParam(QStringLiteral("drawOverlay"), QStringLiteral("绘制测量结果"), true, 1,
                          QStringLiteral("显示")),
            makeEnumParam(QStringLiteral("thresholdStrategy"), QStringLiteral("阈值策略"),
                          {QStringLiteral("manual"), QStringLiteral("otsu"), QStringLiteral("adaptive")},
                          {QStringLiteral("手动"), QStringLiteral("Otsu"), QStringLiteral("自适应")},
                          QStringLiteral("manual"), 2, QStringLiteral("阈值")),
            makeIntParam(QStringLiteral("threshold"), QStringLiteral("阈值"), 128, 0, 255, 3,
                         QStringLiteral("阈值")),
            makeIntParam(QStringLiteral("adaptiveBlockSize"), QStringLiteral("自适应块大小"), 31, 3, 101, 4,
                         QStringLiteral("阈值")),
            makeDoubleParam(QStringLiteral("adaptiveC"), QStringLiteral("自适应C"), 5.0, -50.0, 50.0, 5,
                            QStringLiteral("阈值")),
            makeRoiParam(QStringLiteral("roi"), QStringLiteral("ROI"), 6),
            makeRoiApplyModeParam(7),
            makeStringParam(QStringLiteral("calibrationHint"), QStringLiteral("标定提示"),
                            QStringLiteral("物理单位需有效标定快照"), 8,
                            QStringLiteral("无标定时结果单位保持 px，不伪造 mm"),
                            QStringLiteral("标定"))};
        d.inputKeys = {QStringLiteral("image")};
        d.outputKeys = {QStringLiteral("image"), QStringLiteral("measurement"), QStringLiteral("overlay")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::lineMeasure(), QStringLiteral("线段测量"),
                                      QStringLiteral("测量"), QStringLiteral("检测最长线段并输出长度/角度"));
        d.ports = {
            {QStringLiteral("in"), QStringLiteral("输入"), true, Selt::DataTypeId::Image, true,
             QStringLiteral("输入图像")},
            {QStringLiteral("out"), QStringLiteral("图像"), false, Selt::DataTypeId::Image, false,
             QStringLiteral("结果图像")},
            {QStringLiteral("measurement"), QStringLiteral("测量"), false, Selt::DataTypeId::Measurement,
             false, QStringLiteral("测量结果")},
            {QStringLiteral("length"), QStringLiteral("长度"), false, Selt::DataTypeId::Real, false,
             QStringLiteral("线段长度")},
            {QStringLiteral("angle"), QStringLiteral("角度"), false, Selt::DataTypeId::Real, false,
             QStringLiteral("线段角度")},
            {QStringLiteral("overlay"), QStringLiteral("叠加"), false, Selt::DataTypeId::Overlay, false,
             QStringLiteral("叠加绘制")}};
        d.parameters = {
            makeIntParam(QStringLiteral("cannyLow"), QStringLiteral("Canny低阈值"), 50, 0, 255, 0,
                         QStringLiteral("边缘")),
            makeIntParam(QStringLiteral("cannyHigh"), QStringLiteral("Canny高阈值"), 150, 0, 255, 1,
                         QStringLiteral("边缘")),
            makeBoolParam(QStringLiteral("drawOverlay"), QStringLiteral("绘制测量结果"), true, 2,
                          QStringLiteral("显示")),
            makeRoiParam(QStringLiteral("roi"), QStringLiteral("ROI"), 3),
            makeRoiApplyModeParam(4)};
        d.inputKeys = {QStringLiteral("image")};
        d.outputKeys = {QStringLiteral("image"), QStringLiteral("measurement"), QStringLiteral("overlay")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::distanceMeasure(), QStringLiteral("距离测量"),
                                      QStringLiteral("测量"),
                                      QStringLiteral("计算两点距离（由参数或上游 Real 输入）"));
        d.ports = {
            {QStringLiteral("x1"), QStringLiteral("X1"), true, Selt::DataTypeId::Real, true,
             QStringLiteral("起点X")},
            {QStringLiteral("y1"), QStringLiteral("Y1"), true, Selt::DataTypeId::Real, true,
             QStringLiteral("起点Y")},
            {QStringLiteral("x2"), QStringLiteral("X2"), true, Selt::DataTypeId::Real, true,
             QStringLiteral("终点X")},
            {QStringLiteral("y2"), QStringLiteral("Y2"), true, Selt::DataTypeId::Real, true,
             QStringLiteral("终点Y")},
            {QStringLiteral("distance"), QStringLiteral("距离"), false, Selt::DataTypeId::Real, false,
             QStringLiteral("两点距离")},
            {QStringLiteral("measurement"), QStringLiteral("测量"), false, Selt::DataTypeId::Measurement,
             false, QStringLiteral("测量结果")}};
        d.parameters = {
            makeDoubleParam(QStringLiteral("x1"), QStringLiteral("X1"), 0.0, -1e9, 1e9, 0),
            makeDoubleParam(QStringLiteral("y1"), QStringLiteral("Y1"), 0.0, -1e9, 1e9, 1),
            makeDoubleParam(QStringLiteral("x2"), QStringLiteral("X2"), 100.0, -1e9, 1e9, 2),
            makeDoubleParam(QStringLiteral("y2"), QStringLiteral("Y2"), 0.0, -1e9, 1e9, 3)};
        d.inputKeys = {QStringLiteral("x1"), QStringLiteral("y1"), QStringLiteral("x2"), QStringLiteral("y2")};
        d.outputKeys = {QStringLiteral("distance"), QStringLiteral("measurement")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::rangeDecision(), QStringLiteral("范围判定"),
                                      QStringLiteral("判定"),
                                      QStringLiteral("按上下限判定输入实数是否合格"));
        d.ports = {
            {QStringLiteral("value"), QStringLiteral("值"), true, Selt::DataTypeId::Real, true,
             QStringLiteral("待判定数值")},
            {QStringLiteral("ok"), QStringLiteral("OK"), false, Selt::DataTypeId::Bool, false,
             QStringLiteral("是否合格")},
            {QStringLiteral("state"), QStringLiteral("状态"), false, Selt::DataTypeId::String, false,
             QStringLiteral("判定状态")},
            {QStringLiteral("measurement"), QStringLiteral("测量"), false, Selt::DataTypeId::Measurement, false,
             QStringLiteral("判定测量结果")}};
        d.parameters = {
            makeDoubleParam(QStringLiteral("lower"), QStringLiteral("下限"), 0.0, -1e9, 1e9, 0),
            makeDoubleParam(QStringLiteral("upper"), QStringLiteral("上限"), 100.0, -1e9, 1e9, 1),
            makeDoubleParam(QStringLiteral("nominal"), QStringLiteral("标称值"), 50.0, -1e9, 1e9, 2)};
        d.inputKeys = {QStringLiteral("value")};
        d.outputKeys = {QStringLiteral("ok"), QStringLiteral("state"), QStringLiteral("measurement")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::resultPreview(), QStringLiteral("结果预览"),
                                      QStringLiteral("输出"), QStringLiteral("显示上游处理结果"));
        d.ports = {{QStringLiteral("in"), QStringLiteral("图像"), true, Selt::DataTypeId::Image, true,
                    QStringLiteral("预览图像")}};
        d.inputKeys = {QStringLiteral("image")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::barcodeDecode(), QStringLiteral("条码/二维码识别"),
                                      QStringLiteral("识别"),
                                      QStringLiteral("OpenCV QRCodeDetector / 可选 barcode 模块"));
        d.helpText = QStringLiteral("二维码优先；条码依赖 OpenCV barcode。工具箱提示显示后端能力。");
        d.ports = {
            {QStringLiteral("in"), QStringLiteral("图像"), true, Selt::DataTypeId::Image, true,
             QStringLiteral("待识别图像")},
            {QStringLiteral("out"), QStringLiteral("图像"), false, Selt::DataTypeId::Image, false,
             QStringLiteral("叠加预览")},
            {QStringLiteral("text"), QStringLiteral("内容"), false, Selt::DataTypeId::String, false,
             QStringLiteral("解码文本")},
            {QStringLiteral("confidence"), QStringLiteral("置信度"), false, Selt::DataTypeId::Real, false,
             QStringLiteral("解码置信度")},
            {QStringLiteral("symbology"), QStringLiteral("码制"), false, Selt::DataTypeId::String, false,
             QStringLiteral("qr / barcode 类型")},
            {QStringLiteral("found"), QStringLiteral("已找到"), false, Selt::DataTypeId::Bool, false,
             QStringLiteral("是否检测到目标")},
            {QStringLiteral("overlay"), QStringLiteral("叠加"), false, Selt::DataTypeId::Overlay, false,
             QStringLiteral("框与文本叠加")},
            {QStringLiteral("strategy"), QStringLiteral("策略"), false, Selt::DataTypeId::String, false,
             QStringLiteral("命中预处理策略"), QStringLiteral("Debug"), Selt::PortRole::Debug, false, true},
            {QStringLiteral("attempts"), QStringLiteral("尝试次数"), false, Selt::DataTypeId::Int, false,
             QStringLiteral("候选尝试次数"), QStringLiteral("Debug"), Selt::PortRole::Debug, false, true},
            {QStringLiteral("failureStage"), QStringLiteral("失败阶段"), false, Selt::DataTypeId::String, false,
             QStringLiteral("none/detect/decode/quality/backend"), QStringLiteral("Debug"),
             Selt::PortRole::Debug, false, true},
            {QStringLiteral("backendStatus"), QStringLiteral("后端状态"), false, Selt::DataTypeId::String, false,
             QStringLiteral("QR/Barcode 能力状态"), QStringLiteral("Debug"), Selt::PortRole::Debug, false, true},
            {QStringLiteral("debug"), QStringLiteral("调试图"), false, Selt::DataTypeId::Image, false,
             QStringLiteral("最后一次候选预处理图"), QStringLiteral("Debug"), Selt::PortRole::Debug, false, true}};
        d.parameters = {
            makeEnumParam(QStringLiteral("symbology"), QStringLiteral("码制"),
                          {QStringLiteral("auto"), QStringLiteral("qr"), QStringLiteral("barcode")},
                          {QStringLiteral("自动"), QStringLiteral("二维码"), QStringLiteral("条码")},
                          QStringLiteral("auto"), 0, QStringLiteral("识别"),
                          QStringLiteral("barcode 模式在缺少 OpenCV barcode 时返回 backend_missing")),
            makeIntParam(QStringLiteral("maxCount"), QStringLiteral("最大候选数"), 8, 1, 64, 1,
                         QStringLiteral("识别")),
            makeEnumParam(QStringLiteral("enhanceMode"), QStringLiteral("增强模式"),
                          {QStringLiteral("none"), QStringLiteral("auto"), QStringLiteral("full")},
                          {QStringLiteral("关闭"), QStringLiteral("自动"), QStringLiteral("全力")},
                          QStringLiteral("auto"), 2, QStringLiteral("识别"),
                          QStringLiteral("自动：快速失败后再 CLAHE/锐化/自适应/反色")),
            makeDoubleParam(QStringLiteral("scale"), QStringLiteral("基础缩放"), 1.0, 0.5, 4.0, 3,
                            QStringLiteral("识别"), QStringLiteral("识别前放大，低分辨率场景建议 1.5~2.0")),
            makeBoolParam(QStringLiteral("tryRotate"), QStringLiteral("尝试旋转"), true, 4,
                          QStringLiteral("识别"), QStringLiteral("未找到时尝试 90° 旋转再识别")),
            makeBoolParam(QStringLiteral("tryInvert"), QStringLiteral("尝试反色"), true, 5,
                          QStringLiteral("高级"), QStringLiteral("反光/白底黑码极性相反时有效")),
            makeBoolParam(QStringLiteral("tryClahe"), QStringLiteral("CLAHE增强"), true, 6,
                          QStringLiteral("高级"), QStringLiteral("亮度不均时提升局部对比度")),
            makeBoolParam(QStringLiteral("tryAdaptive"), QStringLiteral("自适应二值化"), true, 7,
                          QStringLiteral("高级")),
            makeBoolParam(QStringLiteral("trySharpen"), QStringLiteral("锐化"), true, 8,
                          QStringLiteral("高级")),
            makeBoolParam(QStringLiteral("tryDenoise"), QStringLiteral("去噪"), false, 9,
                          QStringLiteral("高级")),
            makeIntParam(QStringLiteral("maxAttempts"), QStringLiteral("最大尝试次数"), 12, 1, 32, 10,
                         QStringLiteral("高级")),
            makeIntParam(QStringLiteral("maxTimeMs"), QStringLiteral("超时(ms)"), 800, 0, 10000, 11,
                         QStringLiteral("高级"), QStringLiteral("0=不限制；困难图避免耗时失控")),
            makeDoubleParam(QStringLiteral("minConfidence"), QStringLiteral("最低置信度"), 0.0, 0.0, 1.0, 12,
                            QStringLiteral("高级")),
            makeBoolParam(QStringLiteral("keepDebugImage"), QStringLiteral("保留调试图"), true, 13,
                          QStringLiteral("调试")),
            makeBoolParam(QStringLiteral("passthroughOnFail"), QStringLiteral("失败透传图像"), true, 14,
                          QStringLiteral("识别")),
            makeRoiParam(QStringLiteral("roi"), QStringLiteral("ROI"), 15)};
        d.inputKeys = {QStringLiteral("image")};
        d.outputKeys = {QStringLiteral("image"), QStringLiteral("text"), QStringLiteral("confidence"),
                        QStringLiteral("symbology"), QStringLiteral("found"), QStringLiteral("overlay"),
                        QStringLiteral("strategy"), QStringLiteral("attempts"),
                        QStringLiteral("failureStage"), QStringLiteral("backendStatus"),
                        QStringLiteral("debug")};
        d.uiSchema.previewLayers = {QStringLiteral("image"), QStringLiteral("geometry")};
        d.uiSchema.capabilityTags = {QStringLiteral("recognition")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::ocr(), QStringLiteral("OCR识别"),
                                      QStringLiteral("识别"),
                                      QStringLiteral("可选 Tesseract OCR；语言包由 tessdataPath 指定"));
        d.helpText = QStringLiteral("未安装 Tesseract 时返回 capability_limited；中文需 chi_sim.traineddata。");
        d.ports = {
            {QStringLiteral("in"), QStringLiteral("图像"), true, Selt::DataTypeId::Image, true,
             QStringLiteral("待识别图像")},
            {QStringLiteral("out"), QStringLiteral("图像"), false, Selt::DataTypeId::Image, false,
             QStringLiteral("叠加预览")},
            {QStringLiteral("text"), QStringLiteral("文本"), false, Selt::DataTypeId::String, false,
             QStringLiteral("识别文本")},
            {QStringLiteral("confidence"), QStringLiteral("置信度"), false, Selt::DataTypeId::Real, false,
             QStringLiteral("平均置信度 0~1")},
            {QStringLiteral("found"), QStringLiteral("已找到"), false, Selt::DataTypeId::Bool, false,
             QStringLiteral("是否识别到文本")},
            {QStringLiteral("overlay"), QStringLiteral("叠加"), false, Selt::DataTypeId::Overlay, false,
             QStringLiteral("文本框叠加")}};
        d.parameters = {
            makeStringParam(QStringLiteral("language"), QStringLiteral("语言"), QStringLiteral("eng"), 0,
                            QStringLiteral("如 eng / chi_sim / eng+chi_sim"), QStringLiteral("OCR")),
            makeFileParam(QStringLiteral("tessdataPath"), QStringLiteral("tessdata目录"), 1,
                          QStringLiteral("OCR"), QStringLiteral("含 *.traineddata 的目录；可空走 TESSDATA_PREFIX")),
            makeStringParam(QStringLiteral("whitelist"), QStringLiteral("字符白名单"), QString(), 2,
                            {}, QStringLiteral("OCR")),
            makeBoolParam(QStringLiteral("multiLine"), QStringLiteral("多行"), true, 3, QStringLiteral("OCR")),
            makeDoubleParam(QStringLiteral("scale"), QStringLiteral("缩放倍数"), 1.0, 0.5, 4.0, 4,
                            QStringLiteral("OCR")),
            makeDoubleParam(QStringLiteral("minConfidence"), QStringLiteral("最小置信度"), 0.0, 0.0, 1.0, 5,
                            QStringLiteral("OCR")),
            makeBoolParam(QStringLiteral("binarize"), QStringLiteral("二值化"), false, 6, QStringLiteral("OCR")),
            makeBoolParam(QStringLiteral("passthroughOnFail"), QStringLiteral("失败透传图像"), true, 7,
                          QStringLiteral("OCR")),
            makeRoiParam(QStringLiteral("roi"), QStringLiteral("ROI"), 8)};
        d.inputKeys = {QStringLiteral("image")};
        d.outputKeys = {QStringLiteral("image"), QStringLiteral("text"), QStringLiteral("confidence"),
                        QStringLiteral("found"), QStringLiteral("overlay")};
        d.uiSchema.previewLayers = {QStringLiteral("image"), QStringLiteral("geometry")};
        d.uiSchema.capabilityTags = {QStringLiteral("recognition"), QStringLiteral("needs_tesseract")};
        list.append(d);
    }

    // ---- O1 preprocess ----
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::gaussianBlur(), QStringLiteral("高斯滤波"),
                                      QStringLiteral("预处理"), QStringLiteral("高斯模糊平滑"));
        d.parameters = {
            makeIntParam(QStringLiteral("ksizeX"), QStringLiteral("核宽"), 5, 1, 51, 0,
                         QStringLiteral("滤波"), QStringLiteral("奇数核；偶数会自动纠正")),
            makeIntParam(QStringLiteral("ksizeY"), QStringLiteral("核高"), 5, 1, 51, 1,
                         QStringLiteral("滤波")),
            makeDoubleParam(QStringLiteral("sigmaX"), QStringLiteral("SigmaX"), 0.0, 0.0, 100.0, 2,
                            QStringLiteral("滤波"), QStringLiteral("0 表示由核大小推导")),
            makeDoubleParam(QStringLiteral("sigmaY"), QStringLiteral("SigmaY"), 0.0, 0.0, 100.0, 3,
                            QStringLiteral("滤波")),
            makeRoiParam(QStringLiteral("roi"), QStringLiteral("ROI"), 4),
            makeRoiApplyModeParam(5)};
        d.inputKeys = {QStringLiteral("image")};
        d.outputKeys = {QStringLiteral("image")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::medianBlur(), QStringLiteral("中值滤波"),
                                      QStringLiteral("预处理"), QStringLiteral("中值滤波去噪"));
        d.parameters = {
            makeIntParam(QStringLiteral("ksize"), QStringLiteral("核大小"), 5, 1, 51, 0),
            makeRoiParam(QStringLiteral("roi"), QStringLiteral("ROI"), 1)};
        d.inputKeys = {QStringLiteral("image")};
        d.outputKeys = {QStringLiteral("image")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::bilateralFilter(), QStringLiteral("双边滤波"),
                                      QStringLiteral("预处理"), QStringLiteral("保边平滑滤波"));
        d.parameters = {
            makeIntParam(QStringLiteral("diameter"), QStringLiteral("直径"), 9, 1, 51, 0),
            makeDoubleParam(QStringLiteral("sigmaColor"), QStringLiteral("颜色Sigma"), 75.0, 0.0, 500.0, 1),
            makeDoubleParam(QStringLiteral("sigmaSpace"), QStringLiteral("空间Sigma"), 75.0, 0.0, 500.0, 2),
            makeRoiParam(QStringLiteral("roi"), QStringLiteral("ROI"), 3)};
        d.inputKeys = {QStringLiteral("image")};
        d.outputKeys = {QStringLiteral("image")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::otsuThreshold(), QStringLiteral("Otsu阈值"),
                                      QStringLiteral("预处理"), QStringLiteral("Otsu 自动阈值"));
        d.parameters = {
            makeDoubleParam(QStringLiteral("maxValue"), QStringLiteral("最大值"), 255.0, 0.0, 255.0, 0),
            makeBoolParam(QStringLiteral("invert"), QStringLiteral("反相"), false, 1),
            makeRoiParam(QStringLiteral("roi"), QStringLiteral("ROI"), 2)};
        d.inputKeys = {QStringLiteral("image")};
        d.outputKeys = {QStringLiteral("image")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::adaptiveThreshold(), QStringLiteral("自适应阈值"),
                                      QStringLiteral("预处理"), QStringLiteral("局部自适应阈值"));
        d.parameters = {
            makeEnumParam(QStringLiteral("method"), QStringLiteral("方法"),
                          {{QStringLiteral("gaussian"), QStringLiteral("高斯")},
                           {QStringLiteral("mean"), QStringLiteral("均值")}},
                          QStringLiteral("gaussian"), 0),
            makeIntParam(QStringLiteral("blockSize"), QStringLiteral("块大小"), 11, 3, 99, 1),
            makeDoubleParam(QStringLiteral("C"), QStringLiteral("常数C"), 2.0, -50.0, 50.0, 2),
            makeDoubleParam(QStringLiteral("maxValue"), QStringLiteral("最大值"), 255.0, 0.0, 255.0, 3),
            makeBoolParam(QStringLiteral("invert"), QStringLiteral("反相"), false, 4),
            makeRoiParam(QStringLiteral("roi"), QStringLiteral("ROI"), 5)};
        d.inputKeys = {QStringLiteral("image")};
        d.outputKeys = {QStringLiteral("image")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::sobel(), QStringLiteral("Sobel边缘"),
                                      QStringLiteral("预处理"), QStringLiteral("Sobel 梯度边缘"));
        d.parameters = {
            makeIntParam(QStringLiteral("dx"), QStringLiteral("dx"), 1, 0, 2, 0),
            makeIntParam(QStringLiteral("dy"), QStringLiteral("dy"), 0, 0, 2, 1),
            makeIntParam(QStringLiteral("ksize"), QStringLiteral("核大小"), 3, 1, 7, 2),
            makeDoubleParam(QStringLiteral("scale"), QStringLiteral("缩放"), 1.0, 0.0, 10.0, 3),
            makeRoiParam(QStringLiteral("roi"), QStringLiteral("ROI"), 4)};
        d.inputKeys = {QStringLiteral("image")};
        d.outputKeys = {QStringLiteral("image")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::colorConvert(), QStringLiteral("颜色转换"),
                                      QStringLiteral("预处理"), QStringLiteral("颜色空间转换"));
        d.parameters = {
            makeEnumParam(QStringLiteral("mode"), QStringLiteral("模式"),
                          {{QStringLiteral("gray"), QStringLiteral("灰度")},
                           {QStringLiteral("hsv"), QStringLiteral("HSV")},
                           {QStringLiteral("lab"), QStringLiteral("Lab")},
                           {QStringLiteral("bgr"), QStringLiteral("BGR")}},
                          QStringLiteral("gray"), 0),
            makeRoiParam(QStringLiteral("roi"), QStringLiteral("ROI"), 1)};
        d.inputKeys = {QStringLiteral("image")};
        d.outputKeys = {QStringLiteral("image")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::geometricTransform(), QStringLiteral("几何变换"),
                                      QStringLiteral("预处理"), QStringLiteral("旋转/翻转"));
        d.parameters = {
            makeEnumParam(QStringLiteral("op"), QStringLiteral("操作"),
                          {{QStringLiteral("rotate90"), QStringLiteral("旋转90")},
                           {QStringLiteral("rotate180"), QStringLiteral("旋转180")},
                           {QStringLiteral("rotate270"), QStringLiteral("旋转270")},
                           {QStringLiteral("flipX"), QStringLiteral("水平翻转")},
                           {QStringLiteral("flipY"), QStringLiteral("垂直翻转")},
                           {QStringLiteral("flipXY"), QStringLiteral("双向翻转")}},
                          QStringLiteral("rotate90"), 0)};
        d.inputKeys = {QStringLiteral("image")};
        d.outputKeys = {QStringLiteral("image")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::gammaCorrect(), QStringLiteral("Gamma校正"),
                                      QStringLiteral("预处理"), QStringLiteral("非线性亮度校正"));
        d.parameters = {
            makeDoubleParam(QStringLiteral("gamma"), QStringLiteral("Gamma"), 1.0, 0.05, 5.0, 0,
                            QStringLiteral("基本"), QStringLiteral("<1 提亮暗部；>1 压暗")),
            makeRoiParam(QStringLiteral("roi"), QStringLiteral("ROI"), 1),
            makeRoiApplyModeParam(2)};
        d.inputKeys = {QStringLiteral("image")};
        d.outputKeys = {QStringLiteral("image")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::contrastBrightness(), QStringLiteral("对比度亮度"),
                                      QStringLiteral("预处理"), QStringLiteral("线性对比度与亮度调整"));
        d.parameters = {
            makeDoubleParam(QStringLiteral("alpha"), QStringLiteral("对比度"), 1.0, 0.0, 5.0, 0,
                            QStringLiteral("基本")),
            makeDoubleParam(QStringLiteral("beta"), QStringLiteral("亮度"), 0.0, -255.0, 255.0, 1,
                            QStringLiteral("基本")),
            makeRoiParam(QStringLiteral("roi"), QStringLiteral("ROI"), 2),
            makeRoiApplyModeParam(3)};
        d.inputKeys = {QStringLiteral("image")};
        d.outputKeys = {QStringLiteral("image")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::claheEnhance(), QStringLiteral("CLAHE增强"),
                                      QStringLiteral("预处理"), QStringLiteral("自适应直方图均衡"));
        d.parameters = {
            makeDoubleParam(QStringLiteral("clipLimit"), QStringLiteral("裁剪限"), 2.0, 0.5, 40.0, 0,
                            QStringLiteral("基本")),
            makeIntParam(QStringLiteral("tileSize"), QStringLiteral("分块大小"), 8, 2, 64, 1,
                         QStringLiteral("基本")),
            makeRoiParam(QStringLiteral("roi"), QStringLiteral("ROI"), 2),
            makeRoiApplyModeParam(3)};
        d.inputKeys = {QStringLiteral("image")};
        d.outputKeys = {QStringLiteral("image")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::sharpen(), QStringLiteral("锐化"),
                                      QStringLiteral("预处理"), QStringLiteral("反锐化掩模增强边缘"));
        d.parameters = {
            makeDoubleParam(QStringLiteral("amount"), QStringLiteral("强度"), 0.5, 0.0, 5.0, 0,
                            QStringLiteral("基本")),
            makeDoubleParam(QStringLiteral("sigma"), QStringLiteral("模糊Sigma"), 1.0, 0.1, 10.0, 1,
                            QStringLiteral("基本")),
            makeRoiParam(QStringLiteral("roi"), QStringLiteral("ROI"), 2),
            makeRoiApplyModeParam(3)};
        d.inputKeys = {QStringLiteral("image")};
        d.outputKeys = {QStringLiteral("image")};
        list.append(d);
    }

    // ---- O2 region ----
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::maskCombine(), QStringLiteral("掩膜合成"),
                                      QStringLiteral("区域"), QStringLiteral("图像逻辑运算"));
        d.ports = {
            {QStringLiteral("inA"), QStringLiteral("图像A"), true, Selt::DataTypeId::Image, true, {}},
            {QStringLiteral("inB"), QStringLiteral("图像B"), true, Selt::DataTypeId::Image, false, {}},
            {QStringLiteral("out"), QStringLiteral("输出"), false, Selt::DataTypeId::Image, false, {}}};
        d.parameters = {
            makeEnumParam(QStringLiteral("op"), QStringLiteral("运算"),
                          {{QStringLiteral("and"), QStringLiteral("AND")},
                           {QStringLiteral("or"), QStringLiteral("OR")},
                           {QStringLiteral("xor"), QStringLiteral("XOR")},
                           {QStringLiteral("not"), QStringLiteral("NOT")}},
                          QStringLiteral("and"), 0)};
        d.inputKeys = {QStringLiteral("imageA"), QStringLiteral("imageB")};
        d.outputKeys = {QStringLiteral("image")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::findContours(), QStringLiteral("轮廓提取"),
                                      QStringLiteral("区域"), QStringLiteral("提取外轮廓"));
        d.ports = {
            {QStringLiteral("in"), QStringLiteral("输入"), true, Selt::DataTypeId::Image, true, {}},
            {QStringLiteral("out"), QStringLiteral("图像"), false, Selt::DataTypeId::Image, false, {}},
            {QStringLiteral("contours"), QStringLiteral("轮廓"), false, Selt::DataTypeId::Contour, false, {}},
            {QStringLiteral("count"), QStringLiteral("数量"), false, Selt::DataTypeId::Int, false, {}}};
        d.parameters = {makeRoiParam(QStringLiteral("roi"), QStringLiteral("ROI"), 0)};
        d.inputKeys = {QStringLiteral("image")};
        d.outputKeys = {QStringLiteral("image"), QStringLiteral("contours"), QStringLiteral("count")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::connectedComponents(), QStringLiteral("连通域"),
                                      QStringLiteral("区域"), QStringLiteral("连通域标记与统计"));
        d.ports = {
            {QStringLiteral("in"), QStringLiteral("输入"), true, Selt::DataTypeId::Image, true, {}},
            {QStringLiteral("out"), QStringLiteral("标签图"), false, Selt::DataTypeId::Image, false, {}},
            {QStringLiteral("region"), QStringLiteral("区域"), false, Selt::DataTypeId::Region, false, {}},
            {QStringLiteral("table"), QStringLiteral("表格"), false, Selt::DataTypeId::Table, false, {}},
            {QStringLiteral("count"), QStringLiteral("数量"), false, Selt::DataTypeId::Int, false, {}}};
        d.parameters = {makeRoiParam(QStringLiteral("roi"), QStringLiteral("ROI"), 0)};
        d.inputKeys = {QStringLiteral("image")};
        d.outputKeys = {QStringLiteral("image"), QStringLiteral("region"), QStringLiteral("table"), QStringLiteral("count")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::regionFill(), QStringLiteral("区域填充"),
                                      QStringLiteral("区域"), QStringLiteral("填充外部轮廓"));
        d.parameters = {makeRoiParam(QStringLiteral("roi"), QStringLiteral("ROI"), 0)};
        d.inputKeys = {QStringLiteral("image")};
        d.outputKeys = {QStringLiteral("image")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::blobAnalyze(), QStringLiteral("Blob分析"),
                                      QStringLiteral("区域"), QStringLiteral("按面积/圆度筛选目标"));
        d.ports = {
            {QStringLiteral("in"), QStringLiteral("输入"), true, Selt::DataTypeId::Image, true, {}},
            {QStringLiteral("out"), QStringLiteral("图像"), false, Selt::DataTypeId::Image, false, {}},
            {QStringLiteral("region"), QStringLiteral("区域"), false, Selt::DataTypeId::Region, false, {}},
            {QStringLiteral("count"), QStringLiteral("数量"), false, Selt::DataTypeId::Int, false, {}},
            {QStringLiteral("center"), QStringLiteral("中心"), false, Selt::DataTypeId::Point2D, false, {}},
            {QStringLiteral("area"), QStringLiteral("面积"), false, Selt::DataTypeId::Real, false, {}},
            {QStringLiteral("circularity"), QStringLiteral("圆度"), false, Selt::DataTypeId::Real, false, {}},
            {QStringLiteral("selectedIndex"), QStringLiteral("选中索引"), false, Selt::DataTypeId::Int, false, {}},
            {QStringLiteral("confidence"), QStringLiteral("置信度"), false, Selt::DataTypeId::Real, false, {}},
            {QStringLiteral("candidateCount"), QStringLiteral("候选数"), false, Selt::DataTypeId::Int, false, {}}};
        d.parameters = {
            makeDoubleParam(QStringLiteral("minArea"), QStringLiteral("最小面积"), 50.0, 0.0, 1e12, 0),
            makeDoubleParam(QStringLiteral("maxArea"), QStringLiteral("最大面积"), 1e9, 0.0, 1e12, 1),
            makeDoubleParam(QStringLiteral("minCircularity"), QStringLiteral("最小圆度"), 0.0, 0.0, 1.0, 2),
            makeDoubleParam(QStringLiteral("minAspectRatio"), QStringLiteral("最小长宽比"), 0.0, 0.0, 100.0, 3),
            makeDoubleParam(QStringLiteral("maxAspectRatio"), QStringLiteral("最大长宽比"), 100.0, 0.0, 100.0, 4),
            makeDoubleParam(QStringLiteral("minExtent"), QStringLiteral("最小矩形度"), 0.0, 0.0, 1.0, 5),
            makeDoubleParam(QStringLiteral("minSolidity"), QStringLiteral("最小实心度"), 0.0, 0.0, 1.0, 6),
            makeIntParam(QStringLiteral("maxCount"), QStringLiteral("最大数量"), 100, 1, 1000, 7),
            makeEnumParam(QStringLiteral("sortBy"), QStringLiteral("排序"),
                          {QStringLiteral("area"), QStringLiteral("circularity"),
                           QStringLiteral("x"), QStringLiteral("y")},
                          {QStringLiteral("面积"), QStringLiteral("圆度"),
                           QStringLiteral("X坐标"), QStringLiteral("Y坐标")},
                          QStringLiteral("area"), 8),
            makeIntParam(QStringLiteral("selectIndex"), QStringLiteral("选中索引"), 0, 0, 999, 9,
                         QStringLiteral("筛选"), QStringLiteral("排序后取第几个目标作为主输出")),
            makeBoolParam(QStringLiteral("requireTarget"), QStringLiteral("必须有目标"), false, 10,
                          QStringLiteral("筛选"),
                          QStringLiteral("开启后空目标或索引越界将失败；数量检测可关闭")),
            makeRoiParam(QStringLiteral("roi"), QStringLiteral("ROI"), 11)};
        d.inputKeys = {QStringLiteral("image")};
        d.outputKeys = {QStringLiteral("image"), QStringLiteral("region"), QStringLiteral("count"),
                        QStringLiteral("center"), QStringLiteral("area"), QStringLiteral("circularity"),
                        QStringLiteral("selectedIndex"), QStringLiteral("confidence"),
                        QStringLiteral("candidateCount")};
        list.append(d);
    }

    // ---- O3 locate ----
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::houghLines(), QStringLiteral("Hough直线"),
                                      QStringLiteral("定位"), QStringLiteral("Hough 线段检测"));
        d.ports = {
            {QStringLiteral("in"), QStringLiteral("输入"), true, Selt::DataTypeId::Image, true, {}},
            {QStringLiteral("out"), QStringLiteral("图像"), false, Selt::DataTypeId::Image, false, {}},
            {QStringLiteral("line"), QStringLiteral("直线"), false, Selt::DataTypeId::Line, false, {}},
            {QStringLiteral("count"), QStringLiteral("数量"), false, Selt::DataTypeId::Int, false, {}}};
        d.parameters = {
            makeDoubleParam(QStringLiteral("cannyLow"), QStringLiteral("Canny低"), 50.0, 0.0, 255.0, 0),
            makeDoubleParam(QStringLiteral("cannyHigh"), QStringLiteral("Canny高"), 150.0, 0.0, 255.0, 1),
            makeIntParam(QStringLiteral("accumulatorThreshold"), QStringLiteral("累加器阈值"), 50, 1, 1000, 2),
            makeDoubleParam(QStringLiteral("minLineLength"), QStringLiteral("最小线长"), 30.0, 0.0, 1e6, 3),
            makeDoubleParam(QStringLiteral("maxLineGap"), QStringLiteral("最大间隙"), 10.0, 0.0, 1e6, 4),
            makeIntParam(QStringLiteral("maxCount"), QStringLiteral("最大数量"), 50, 1, 500, 5),
            makeEnumParam(QStringLiteral("sortBy"), QStringLiteral("排序"),
                          {QStringLiteral("length"), QStringLiteral("score")},
                          {QStringLiteral("长度"), QStringLiteral("得分")},
                          QStringLiteral("length"), 6),
            makeRoiParam(QStringLiteral("roi"), QStringLiteral("ROI"), 7)};
        d.inputKeys = {QStringLiteral("image")};
        d.outputKeys = {QStringLiteral("image"), QStringLiteral("line"), QStringLiteral("count")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::houghCircles(), QStringLiteral("Hough圆"),
                                      QStringLiteral("定位"), QStringLiteral("Hough 圆检测"));
        d.ports = {
            {QStringLiteral("in"), QStringLiteral("输入"), true, Selt::DataTypeId::Image, true, {}},
            {QStringLiteral("out"), QStringLiteral("图像"), false, Selt::DataTypeId::Image, false, {}},
            {QStringLiteral("circle"), QStringLiteral("圆"), false, Selt::DataTypeId::Circle, false, {}},
            {QStringLiteral("center"), QStringLiteral("中心"), false, Selt::DataTypeId::Point2D, false, {}},
            {QStringLiteral("radius"), QStringLiteral("半径"), false, Selt::DataTypeId::Real, false, {}}};
        d.parameters = {
            makeDoubleParam(QStringLiteral("minRadius"), QStringLiteral("最小半径"), 5.0, 0.0, 1e6, 0),
            makeDoubleParam(QStringLiteral("maxRadius"), QStringLiteral("最大半径"), 0.0, 0.0, 1e6, 1),
            makeDoubleParam(QStringLiteral("dp"), QStringLiteral("累加器分辨率"), 1.2, 0.5, 10.0, 2),
            makeDoubleParam(QStringLiteral("minDist"), QStringLiteral("最小圆心距"), 0.0, 0.0, 1e6, 3),
            makeDoubleParam(QStringLiteral("param1"), QStringLiteral("Canny高阈值"), 100.0, 1.0, 500.0, 4),
            makeDoubleParam(QStringLiteral("param2"), QStringLiteral("累加器阈值"), 30.0, 1.0, 500.0, 5),
            makeIntParam(QStringLiteral("maxCount"), QStringLiteral("最大数量"), 50, 1, 500, 6),
            makeRoiParam(QStringLiteral("roi"), QStringLiteral("ROI"), 7)};
        d.inputKeys = {QStringLiteral("image")};
        d.outputKeys = {QStringLiteral("image"), QStringLiteral("circle"), QStringLiteral("center"), QStringLiteral("radius")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::blobLocate(), QStringLiteral("Blob定位"),
                                      QStringLiteral("定位"), QStringLiteral("Blob 中心定位"));
        d.ports = {
            {QStringLiteral("in"), QStringLiteral("输入"), true, Selt::DataTypeId::Image, true, {}},
            {QStringLiteral("out"), QStringLiteral("图像"), false, Selt::DataTypeId::Image, false, {}},
            {QStringLiteral("region"), QStringLiteral("区域"), false, Selt::DataTypeId::Region, false, {}},
            {QStringLiteral("center"), QStringLiteral("中心"), false, Selt::DataTypeId::Point2D, false, {}},
            {QStringLiteral("area"), QStringLiteral("面积"), false, Selt::DataTypeId::Real, false, {}},
            {QStringLiteral("count"), QStringLiteral("数量"), false, Selt::DataTypeId::Int, false, {}},
            {QStringLiteral("selectedIndex"), QStringLiteral("选中索引"), false, Selt::DataTypeId::Int, false, {}},
            {QStringLiteral("confidence"), QStringLiteral("置信度"), false, Selt::DataTypeId::Real, false, {}},
            {QStringLiteral("candidateCount"), QStringLiteral("候选数"), false, Selt::DataTypeId::Int, false, {}}};
        d.parameters = {
            makeDoubleParam(QStringLiteral("minArea"), QStringLiteral("最小面积"), 50.0, 0.0, 1e12, 0),
            makeDoubleParam(QStringLiteral("maxArea"), QStringLiteral("最大面积"), 1e9, 0.0, 1e12, 1),
            makeDoubleParam(QStringLiteral("minCircularity"), QStringLiteral("最小圆度"), 0.2, 0.0, 1.0, 2),
            makeDoubleParam(QStringLiteral("minAspectRatio"), QStringLiteral("最小长宽比"), 0.0, 0.0, 100.0, 3),
            makeDoubleParam(QStringLiteral("maxAspectRatio"), QStringLiteral("最大长宽比"), 100.0, 0.0, 100.0, 4),
            makeDoubleParam(QStringLiteral("minExtent"), QStringLiteral("最小矩形度"), 0.0, 0.0, 1.0, 5),
            makeDoubleParam(QStringLiteral("minSolidity"), QStringLiteral("最小实心度"), 0.0, 0.0, 1.0, 6),
            makeIntParam(QStringLiteral("maxCount"), QStringLiteral("最大数量"), 100, 1, 1000, 7),
            makeEnumParam(QStringLiteral("sortBy"), QStringLiteral("排序"),
                          {QStringLiteral("area"), QStringLiteral("circularity"),
                           QStringLiteral("x"), QStringLiteral("y")},
                          {QStringLiteral("面积"), QStringLiteral("圆度"),
                           QStringLiteral("X坐标"), QStringLiteral("Y坐标")},
                          QStringLiteral("area"), 8),
            makeIntParam(QStringLiteral("selectIndex"), QStringLiteral("选中索引"), 0, 0, 999, 9,
                         QStringLiteral("筛选"), QStringLiteral("排序后取第几个目标作为主输出")),
            makeRoiParam(QStringLiteral("roi"), QStringLiteral("ROI"), 10)};
        d.inputKeys = {QStringLiteral("image")};
        d.outputKeys = {QStringLiteral("image"), QStringLiteral("region"), QStringLiteral("center"),
                        QStringLiteral("area"), QStringLiteral("count"), QStringLiteral("selectedIndex"),
                        QStringLiteral("confidence"), QStringLiteral("candidateCount")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::featureMatch(), QStringLiteral("特征匹配"),
                                      QStringLiteral("定位"), QStringLiteral("ORB 特征匹配定位"));
        d.ports = {
            {QStringLiteral("in"), QStringLiteral("图像"), true, Selt::DataTypeId::Image, true, {}},
            {QStringLiteral("template"), QStringLiteral("模板"), true, Selt::DataTypeId::Image, false, {}},
            {QStringLiteral("out"), QStringLiteral("图像"), false, Selt::DataTypeId::Image, false, {}},
            {QStringLiteral("center"), QStringLiteral("中心"), false, Selt::DataTypeId::Point2D, false, {}},
            {QStringLiteral("score"), QStringLiteral("得分"), false, Selt::DataTypeId::Real, false, {}}};
        d.parameters = {
            makeFileParam(QStringLiteral("templatePath"), QStringLiteral("模板路径"), 0),
            makeStringParam(QStringLiteral("templateResourceId"), QStringLiteral("模板资源ID"), {}, 1),
            makeDoubleParam(QStringLiteral("loweRatio"), QStringLiteral("Lowe比率"), 0.75, 0.1, 1.0, 2),
            makeIntParam(QStringLiteral("minMatches"), QStringLiteral("最小匹配数"), 8, 1, 500, 3),
            makeDoubleParam(QStringLiteral("ransacReprojThreshold"), QStringLiteral("RANSAC阈值"), 3.0, 0.1, 50.0, 4),
            makeDoubleParam(QStringLiteral("minInlierRatio"), QStringLiteral("最小内点比"), 0.3, 0.0, 1.0, 5)};
        d.inputKeys = {QStringLiteral("image"), QStringLiteral("template")};
        d.outputKeys = {QStringLiteral("image"), QStringLiteral("center"), QStringLiteral("score")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::templateMatchMulti(), QStringLiteral("多模板匹配"),
                                      QStringLiteral("定位"), QStringLiteral("多目标模板匹配"));
        d.ports = {
            {QStringLiteral("in"), QStringLiteral("图像"), true, Selt::DataTypeId::Image, true, {}},
            {QStringLiteral("template"), QStringLiteral("模板"), true, Selt::DataTypeId::Image, false, {}},
            {QStringLiteral("out"), QStringLiteral("图像"), false, Selt::DataTypeId::Image, false, {}},
            {QStringLiteral("center"), QStringLiteral("中心"), false, Selt::DataTypeId::Point2D, false, {}},
            {QStringLiteral("score"), QStringLiteral("得分"), false, Selt::DataTypeId::Real, false, {}},
            {QStringLiteral("count"), QStringLiteral("数量"), false, Selt::DataTypeId::Int, false, {}},
            {QStringLiteral("scale"), QStringLiteral("尺度"), false, Selt::DataTypeId::Real, false, {}},
            {QStringLiteral("angle"), QStringLiteral("角度"), false, Selt::DataTypeId::Real, false, {}},
            {QStringLiteral("found"), QStringLiteral("已找到"), false, Selt::DataTypeId::Bool, false, {}}};
        d.parameters = {
            makeFileParam(QStringLiteral("templatePath"), QStringLiteral("模板路径"), 0),
            makeStringParam(QStringLiteral("templateResourceId"), QStringLiteral("模板资源ID"), {}, 1),
            makeDoubleParam(QStringLiteral("minScore"), QStringLiteral("最低得分"), 0.6, 0.0, 1.0, 2),
            makeIntParam(QStringLiteral("maxCount"), QStringLiteral("最大数量"), 5, 1, 100, 3),
            makeDoubleParam(QStringLiteral("nmsIoU"), QStringLiteral("NMS IoU"), 0.3, 0.0, 1.0, 4),
            makeDoubleParam(QStringLiteral("nmsCenterDistance"), QStringLiteral("NMS中心距"), 0.0, 0.0, 1e6, 5),
            makeDoubleParam(QStringLiteral("scaleMin"), QStringLiteral("最小尺度"), 1.0, 0.3, 3.0, 6,
                            QStringLiteral("高级"), QStringLiteral("1.0 表示不搜索尺度")),
            makeDoubleParam(QStringLiteral("scaleMax"), QStringLiteral("最大尺度"), 1.0, 0.3, 3.0, 7,
                            QStringLiteral("高级")),
            makeDoubleParam(QStringLiteral("scaleStep"), QStringLiteral("尺度步长"), 0.1, 0.05, 1.0, 8,
                            QStringLiteral("高级")),
            makeDoubleParam(QStringLiteral("angleMin"), QStringLiteral("最小角度"), 0.0, -180.0, 180.0, 9,
                            QStringLiteral("高级"), QStringLiteral("与最大角相同则不搜索旋转")),
            makeDoubleParam(QStringLiteral("angleMax"), QStringLiteral("最大角度"), 0.0, -180.0, 180.0, 10,
                            QStringLiteral("高级")),
            makeDoubleParam(QStringLiteral("angleStep"), QStringLiteral("角度步长"), 15.0, 1.0, 90.0, 11,
                            QStringLiteral("高级"))};
        d.inputKeys = {QStringLiteral("image"), QStringLiteral("template")};
        d.outputKeys = {QStringLiteral("image"), QStringLiteral("center"), QStringLiteral("score"),
                        QStringLiteral("count"), QStringLiteral("scale"), QStringLiteral("angle")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::subpixelRefine(), QStringLiteral("亚像素细化"),
                                      QStringLiteral("定位"), QStringLiteral("局部邻域亚像素细化"));
        d.ports = {
            {QStringLiteral("image"), QStringLiteral("图像"), true, Selt::DataTypeId::Image, false, {}},
            {QStringLiteral("point"), QStringLiteral("点"), true, Selt::DataTypeId::Point2D, false, {}},
            {QStringLiteral("out"), QStringLiteral("点"), false, Selt::DataTypeId::Point2D, false, {}},
            {QStringLiteral("x"), QStringLiteral("X"), false, Selt::DataTypeId::Real, false, {}},
            {QStringLiteral("y"), QStringLiteral("Y"), false, Selt::DataTypeId::Real, false, {}}};
        d.parameters = {
            makeDoubleParam(QStringLiteral("x"), QStringLiteral("X"), 0.0, -1e9, 1e9, 0,
                            QStringLiteral("输入"), QStringLiteral("无 point 端口绑定时使用")),
            makeDoubleParam(QStringLiteral("y"), QStringLiteral("Y"), 0.0, -1e9, 1e9, 1,
                            QStringLiteral("输入")),
            makeEnumParam(QStringLiteral("mode"), QStringLiteral("模式"),
                          {QStringLiteral("corner"), QStringLiteral("peak")},
                          {QStringLiteral("角点"), QStringLiteral("峰值")},
                          QStringLiteral("corner"), 2, QStringLiteral("细化")),
            makeIntParam(QStringLiteral("windowSize"), QStringLiteral("窗口"), 5, 3, 31, 3,
                         QStringLiteral("细化")),
            makeDoubleParam(QStringLiteral("terminateEps"), QStringLiteral("终止精度"), 0.01, 1e-6, 1.0, 4,
                            QStringLiteral("细化"), QStringLiteral("角点迭代终止阈值")),
            makeIntParam(QStringLiteral("maxIterations"), QStringLiteral("最大迭代"), 40, 1, 200, 5,
                         QStringLiteral("细化")),
            makeBoolParam(QStringLiteral("passthrough"), QStringLiteral("兼容透传"), false, 6,
                          QStringLiteral("兼容"), QStringLiteral("无图像时透传输入点"))};
        d.inputKeys = {QStringLiteral("image"), QStringLiteral("point")};
        d.outputKeys = {QStringLiteral("point"), QStringLiteral("x"), QStringLiteral("y")};
        list.append(d);
    }

    // ---- O4 measure ----
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::angleMeasure(), QStringLiteral("角度测量"),
                                      QStringLiteral("测量"), QStringLiteral("两直线夹角"));
        d.ports = {
            {QStringLiteral("line1"), QStringLiteral("线1"), true, Selt::DataTypeId::Line, false, {}},
            {QStringLiteral("line2"), QStringLiteral("线2"), true, Selt::DataTypeId::Line, false, {}},
            {QStringLiteral("angle"), QStringLiteral("角度"), false, Selt::DataTypeId::Real, false, {}},
            {QStringLiteral("measurement"), QStringLiteral("测量"), false, Selt::DataTypeId::Measurement, false, {}}};
        d.parameters = {
            makeDoubleParam(QStringLiteral("x1"), QStringLiteral("X1"), 0.0, -1e9, 1e9, 0),
            makeDoubleParam(QStringLiteral("y1"), QStringLiteral("Y1"), 0.0, -1e9, 1e9, 1),
            makeDoubleParam(QStringLiteral("x2"), QStringLiteral("X2"), 100.0, -1e9, 1e9, 2),
            makeDoubleParam(QStringLiteral("y2"), QStringLiteral("Y2"), 0.0, -1e9, 1e9, 3),
            makeDoubleParam(QStringLiteral("x3"), QStringLiteral("X3"), 0.0, -1e9, 1e9, 4),
            makeDoubleParam(QStringLiteral("y3"), QStringLiteral("Y3"), 0.0, -1e9, 1e9, 5),
            makeDoubleParam(QStringLiteral("x4"), QStringLiteral("X4"), 0.0, -1e9, 1e9, 6),
            makeDoubleParam(QStringLiteral("y4"), QStringLiteral("Y4"), 100.0, -1e9, 1e9, 7)};
        d.inputKeys = {QStringLiteral("line1"), QStringLiteral("line2")};
        d.outputKeys = {QStringLiteral("angle"), QStringLiteral("measurement")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::parallelDistance(), QStringLiteral("平行距"),
                                      QStringLiteral("测量"), QStringLiteral("两直线平行距离"));
        d.ports = {
            {QStringLiteral("line1"), QStringLiteral("线1"), true, Selt::DataTypeId::Line, true, {}},
            {QStringLiteral("line2"), QStringLiteral("线2"), true, Selt::DataTypeId::Line, true, {}},
            {QStringLiteral("distance"), QStringLiteral("距离"), false, Selt::DataTypeId::Real, false, {}},
            {QStringLiteral("measurement"), QStringLiteral("测量"), false, Selt::DataTypeId::Measurement, false, {}}};
        d.inputKeys = {QStringLiteral("line1"), QStringLiteral("line2")};
        d.outputKeys = {QStringLiteral("distance"), QStringLiteral("measurement")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::caliperMeasure(), QStringLiteral("卡尺测量"),
                                      QStringLiteral("测量"), QStringLiteral("卡尺边缘采样测距"));
        d.ports = {
            {QStringLiteral("in"), QStringLiteral("图像"), true, Selt::DataTypeId::Image, true, {}},
            {QStringLiteral("center"), QStringLiteral("中心"), true, Selt::DataTypeId::Point2D, false, {}},
            {QStringLiteral("out"), QStringLiteral("图像"), false, Selt::DataTypeId::Image, false, {}},
            {QStringLiteral("distance"), QStringLiteral("距离"), false, Selt::DataTypeId::Real, false, {}},
            {QStringLiteral("point"), QStringLiteral("边缘点"), false, Selt::DataTypeId::Point2D, false, {}},
            {QStringLiteral("point2"), QStringLiteral("第二边缘"), false, Selt::DataTypeId::Point2D, false, {}},
            {QStringLiteral("edgeStrength"), QStringLiteral("边缘强度"), false, Selt::DataTypeId::Real, false, {}},
            {QStringLiteral("confidence"), QStringLiteral("置信度"), false, Selt::DataTypeId::Real, false, {}},
            {QStringLiteral("unit"), QStringLiteral("单位"), false, Selt::DataTypeId::String, false, {}},
            {QStringLiteral("calibrationId"), QStringLiteral("标定"), false, Selt::DataTypeId::String, false, {}},
            {QStringLiteral("measurement"), QStringLiteral("测量"), false, Selt::DataTypeId::Measurement, false, {}}};
        d.parameters = {
            makeDoubleParam(QStringLiteral("cx"), QStringLiteral("中心X"), 0.0, -1e9, 1e9, 0,
                            QStringLiteral("几何"), QStringLiteral("无 center 端口绑定时使用")),
            makeDoubleParam(QStringLiteral("cy"), QStringLiteral("中心Y"), 0.0, -1e9, 1e9, 1,
                            QStringLiteral("几何")),
            makeDoubleParam(QStringLiteral("angle"), QStringLiteral("角度"), 0.0, -360.0, 360.0, 2,
                            QStringLiteral("几何"), QStringLiteral("卡尺方向（度）")),
            makeDoubleParam(QStringLiteral("length"), QStringLiteral("长度"), 80.0, 1.0, 1e6, 3,
                            QStringLiteral("几何")),
            makeDoubleParam(QStringLiteral("width"), QStringLiteral("宽度"), 8.0, 1.0, 1e4, 4,
                            QStringLiteral("几何")),
            makeDoubleParam(QStringLiteral("sampleStep"), QStringLiteral("采样步长"), 1.0, 0.25, 20.0, 5,
                            QStringLiteral("采样")),
            makeDoubleParam(QStringLiteral("smoothSigma"), QStringLiteral("平滑Sigma"), 0.0, 0.0, 20.0, 6,
                            QStringLiteral("采样")),
            makeEnumParam(QStringLiteral("polarity"), QStringLiteral("边缘极性"),
                          {QStringLiteral("any"), QStringLiteral("dark_to_light"), QStringLiteral("light_to_dark")},
                          {QStringLiteral("任意"), QStringLiteral("暗到亮"), QStringLiteral("亮到暗")},
                          QStringLiteral("any"), 7, QStringLiteral("边缘")),
            makeDoubleParam(QStringLiteral("gradientThreshold"), QStringLiteral("梯度阈值"), 1.0, 0.0, 255.0, 8,
                            QStringLiteral("边缘")),
            makeDoubleParam(QStringLiteral("minEdgeGap"), QStringLiteral("最小边距"), 4.0, 0.0, 1e4, 9,
                            QStringLiteral("边缘"), QStringLiteral("双边测距时的最小间隔")),
            makeDoubleParam(QStringLiteral("secondPeakRatio"), QStringLiteral("次峰比例"), 0.35, 0.0, 1.0, 10,
                            QStringLiteral("边缘"), QStringLiteral("相对主峰的次峰接受比例")),
            makeRoiParam(QStringLiteral("roi"), QStringLiteral("ROI"), 11),
            makeRoiApplyModeParam(12)};
        d.inputKeys = {QStringLiteral("image"), QStringLiteral("center")};
        d.outputKeys = {QStringLiteral("image"), QStringLiteral("distance"), QStringLiteral("point"), QStringLiteral("measurement")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::fitCircle(), QStringLiteral("拟合圆"),
                                      QStringLiteral("测量"), QStringLiteral("由点/轮廓拟合圆"));
        d.ports = {
            {QStringLiteral("contours"), QStringLiteral("轮廓"), true, Selt::DataTypeId::Contour, false, {}},
            {QStringLiteral("circle"), QStringLiteral("圆"), false, Selt::DataTypeId::Circle, false, {}},
            {QStringLiteral("center"), QStringLiteral("中心"), false, Selt::DataTypeId::Point2D, false, {}},
            {QStringLiteral("radius"), QStringLiteral("半径"), false, Selt::DataTypeId::Real, false, {}},
            {QStringLiteral("diameter"), QStringLiteral("直径"), false, Selt::DataTypeId::Real, false, {}},
            {QStringLiteral("measurement"), QStringLiteral("测量"), false, Selt::DataTypeId::Measurement, false, {}}};
        d.parameters = {
            makeEnumParam(QStringLiteral("fitMode"), QStringLiteral("拟合模式"),
                          {QStringLiteral("algebraic"), QStringLiteral("ransac")},
                          {QStringLiteral("代数"), QStringLiteral("RANSAC")},
                          QStringLiteral("algebraic"), 0, QStringLiteral("拟合")),
            makeDoubleParam(QStringLiteral("residualThreshold"), QStringLiteral("残差阈值"), 2.0, 0.1, 100.0, 1,
                            QStringLiteral("拟合")),
            makeIntParam(QStringLiteral("maxIterations"), QStringLiteral("最大迭代"), 100, 1, 5000, 2,
                         QStringLiteral("拟合")),
            makeDoubleParam(QStringLiteral("minInlierRatio"), QStringLiteral("最小内点比"), 0.6, 0.0, 1.0, 3,
                            QStringLiteral("拟合")),
            makeIntParam(QStringLiteral("minInliers"), QStringLiteral("最小内点数"), 3, 2, 10000, 4,
                         QStringLiteral("拟合"), QStringLiteral("RANSAC 最少内点绝对数量")),
            makeEnumParam(QStringLiteral("contourSelect"), QStringLiteral("轮廓选择"),
                          {QStringLiteral("maxPoints"), QStringLiteral("index")},
                          {QStringLiteral("最多点"), QStringLiteral("指定索引")},
                          QStringLiteral("maxPoints"), 5, QStringLiteral("输入")),
            makeIntParam(QStringLiteral("contourIndex"), QStringLiteral("轮廓索引"), 0, 0, 10000, 6,
                         QStringLiteral("输入"))};
        d.inputKeys = {QStringLiteral("contours")};
        d.outputKeys = {QStringLiteral("circle"), QStringLiteral("center"), QStringLiteral("radius"), QStringLiteral("diameter"), QStringLiteral("measurement")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::fitLine(), QStringLiteral("拟合直线"),
                                      QStringLiteral("测量"), QStringLiteral("由点/轮廓拟合直线"));
        d.ports = {
            {QStringLiteral("contours"), QStringLiteral("轮廓"), true, Selt::DataTypeId::Contour, false, {}},
            {QStringLiteral("line"), QStringLiteral("直线"), false, Selt::DataTypeId::Line, false, {}},
            {QStringLiteral("angle"), QStringLiteral("角度"), false, Selt::DataTypeId::Real, false, {}},
            {QStringLiteral("length"), QStringLiteral("长度"), false, Selt::DataTypeId::Real, false, {}},
            {QStringLiteral("measurement"), QStringLiteral("测量"), false, Selt::DataTypeId::Measurement, false, {}}};
        d.parameters = {
            makeEnumParam(QStringLiteral("fitMode"), QStringLiteral("拟合模式"),
                          {QStringLiteral("algebraic"), QStringLiteral("ransac")},
                          {QStringLiteral("最小二乘"), QStringLiteral("RANSAC")},
                          QStringLiteral("algebraic"), 0, QStringLiteral("拟合")),
            makeDoubleParam(QStringLiteral("residualThreshold"), QStringLiteral("残差阈值"), 2.0, 0.1, 100.0, 1,
                            QStringLiteral("拟合")),
            makeIntParam(QStringLiteral("maxIterations"), QStringLiteral("最大迭代"), 100, 1, 5000, 2,
                         QStringLiteral("拟合")),
            makeDoubleParam(QStringLiteral("minInlierRatio"), QStringLiteral("最小内点比"), 0.6, 0.0, 1.0, 3,
                            QStringLiteral("拟合")),
            makeIntParam(QStringLiteral("minInliers"), QStringLiteral("最小内点数"), 2, 2, 10000, 4,
                         QStringLiteral("拟合"), QStringLiteral("RANSAC 最少内点绝对数量")),
            makeEnumParam(QStringLiteral("contourSelect"), QStringLiteral("轮廓选择"),
                          {QStringLiteral("maxPoints"), QStringLiteral("index")},
                          {QStringLiteral("最多点"), QStringLiteral("指定索引")},
                          QStringLiteral("maxPoints"), 5, QStringLiteral("输入")),
            makeIntParam(QStringLiteral("contourIndex"), QStringLiteral("轮廓索引"), 0, 0, 10000, 6,
                         QStringLiteral("输入"))};
        d.inputKeys = {QStringLiteral("contours")};
        d.outputKeys = {QStringLiteral("line"), QStringLiteral("angle"), QStringLiteral("length"), QStringLiteral("measurement")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::measureStatistics(), QStringLiteral("测量统计"),
                                      QStringLiteral("测量"), QStringLiteral("多值统计汇总"));
        d.ports = {
            {QStringLiteral("value"), QStringLiteral("值"), true, Selt::DataTypeId::Real, false, {}},
            {QStringLiteral("region"), QStringLiteral("区域"), true, Selt::DataTypeId::Region, false, {}},
            {QStringLiteral("mean"), QStringLiteral("均值"), false, Selt::DataTypeId::Real, false, {}},
            {QStringLiteral("min"), QStringLiteral("最小"), false, Selt::DataTypeId::Real, false, {}},
            {QStringLiteral("max"), QStringLiteral("最大"), false, Selt::DataTypeId::Real, false, {}},
            {QStringLiteral("stddev"), QStringLiteral("标准差"), false, Selt::DataTypeId::Real, false, {}},
            {QStringLiteral("table"), QStringLiteral("表格"), false, Selt::DataTypeId::Table, false, {}}};
        d.inputKeys = {QStringLiteral("value"), QStringLiteral("region")};
        d.outputKeys = {QStringLiteral("mean"), QStringLiteral("min"), QStringLiteral("max"), QStringLiteral("stddev"), QStringLiteral("table")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::toleranceDecision(), QStringLiteral("容差判定"),
                                      QStringLiteral("判定"), QStringLiteral("带标称值的范围判定"));
        d.ports = {
            {QStringLiteral("value"), QStringLiteral("值"), true, Selt::DataTypeId::Real, false, {}},
            {QStringLiteral("ok"), QStringLiteral("OK"), false, Selt::DataTypeId::Bool, false, {}},
            {QStringLiteral("state"), QStringLiteral("状态"), false, Selt::DataTypeId::String, false, {}},
            {QStringLiteral("measurement"), QStringLiteral("测量"), false, Selt::DataTypeId::Measurement, false, {}}};
        d.parameters = {
            makeDoubleParam(QStringLiteral("lower"), QStringLiteral("下限"), 0.0, -1e9, 1e9, 0),
            makeDoubleParam(QStringLiteral("upper"), QStringLiteral("上限"), 100.0, -1e9, 1e9, 1),
            makeDoubleParam(QStringLiteral("nominal"), QStringLiteral("标称值"), 50.0, -1e9, 1e9, 2),
            makeDoubleParam(QStringLiteral("value"), QStringLiteral("值(无绑定时)"), 0.0, -1e9, 1e9, 3)};
        d.inputKeys = {QStringLiteral("value")};
        d.outputKeys = {QStringLiteral("ok"), QStringLiteral("state"), QStringLiteral("measurement")};
        list.append(d);
    }

    // ---- O5 logic/output ----
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::boolAnd(), QStringLiteral("逻辑与"),
                                      QStringLiteral("逻辑"), QStringLiteral("布尔 AND"));
        d.ports = {
            {QStringLiteral("a"), QStringLiteral("A"), true, Selt::DataTypeId::Bool, true, {}},
            {QStringLiteral("b"), QStringLiteral("B"), true, Selt::DataTypeId::Bool, true, {}},
            {QStringLiteral("ok"), QStringLiteral("OK"), false, Selt::DataTypeId::Bool, false, {}}};
        d.inputKeys = {QStringLiteral("a"), QStringLiteral("b")};
        d.outputKeys = {QStringLiteral("ok")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::boolOr(), QStringLiteral("逻辑或"),
                                      QStringLiteral("逻辑"), QStringLiteral("布尔 OR"));
        d.ports = {
            {QStringLiteral("a"), QStringLiteral("A"), true, Selt::DataTypeId::Bool, true, {}},
            {QStringLiteral("b"), QStringLiteral("B"), true, Selt::DataTypeId::Bool, true, {}},
            {QStringLiteral("ok"), QStringLiteral("OK"), false, Selt::DataTypeId::Bool, false, {}}};
        d.inputKeys = {QStringLiteral("a"), QStringLiteral("b")};
        d.outputKeys = {QStringLiteral("ok")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::boolNot(), QStringLiteral("逻辑非"),
                                      QStringLiteral("逻辑"), QStringLiteral("布尔 NOT"));
        d.ports = {
            {QStringLiteral("a"), QStringLiteral("A"), true, Selt::DataTypeId::Bool, true, {}},
            {QStringLiteral("ok"), QStringLiteral("OK"), false, Selt::DataTypeId::Bool, false, {}}};
        d.inputKeys = {QStringLiteral("a")};
        d.outputKeys = {QStringLiteral("ok")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::numericCompare(), QStringLiteral("数值比较"),
                                      QStringLiteral("逻辑"), QStringLiteral("数值关系比较"));
        d.ports = {
            {QStringLiteral("value"), QStringLiteral("值"), true, Selt::DataTypeId::Real, false, {}},
            {QStringLiteral("ok"), QStringLiteral("OK"), false, Selt::DataTypeId::Bool, false, {}},
            {QStringLiteral("state"), QStringLiteral("状态"), false, Selt::DataTypeId::String, false, {}}};
        d.parameters = {
            makeDoubleParam(QStringLiteral("reference"), QStringLiteral("参考值"), 0.0, -1e9, 1e9, 0),
            makeEnumParam(QStringLiteral("op"), QStringLiteral("比较"),
                          {{QStringLiteral("eq"), QStringLiteral("等于")},
                           {QStringLiteral("gt"), QStringLiteral("大于")},
                           {QStringLiteral("lt"), QStringLiteral("小于")},
                           {QStringLiteral("ge"), QStringLiteral("大于等于")},
                           {QStringLiteral("le"), QStringLiteral("小于等于")}},
                          QStringLiteral("eq"), 1),
            makeDoubleParam(QStringLiteral("epsilon"), QStringLiteral("容差"), 1e-6, 0.0, 1e6, 2),
            makeDoubleParam(QStringLiteral("value"), QStringLiteral("值(无绑定时)"), 0.0, -1e9, 1e9, 3)};
        d.inputKeys = {QStringLiteral("value")};
        d.outputKeys = {QStringLiteral("ok"), QStringLiteral("state")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::switchNode(), QStringLiteral("条件选择"),
                                      QStringLiteral("逻辑"), QStringLiteral("按条件选择输出"));
        d.ports = {
            {QStringLiteral("condition"), QStringLiteral("条件"), true, Selt::DataTypeId::Bool, true, {}},
            {QStringLiteral("a"), QStringLiteral("A"), true, Selt::DataTypeId::Image, false, {}},
            {QStringLiteral("b"), QStringLiteral("B"), true, Selt::DataTypeId::Image, false, {}},
            {QStringLiteral("out"), QStringLiteral("输出"), false, Selt::DataTypeId::Image, false, {}}};
        d.inputKeys = {QStringLiteral("condition"), QStringLiteral("a"), QStringLiteral("b")};
        d.outputKeys = {QStringLiteral("out"), QStringLiteral("image")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::counter(), QStringLiteral("计数器"),
                                      QStringLiteral("逻辑"), QStringLiteral("批次 OK/NG 计数"));
        d.ports = {
            {QStringLiteral("ok"), QStringLiteral("OK"), true, Selt::DataTypeId::Bool, false, {}},
            {QStringLiteral("count"), QStringLiteral("总数"), false, Selt::DataTypeId::Int, false, {}},
            {QStringLiteral("okCount"), QStringLiteral("OK数"), false, Selt::DataTypeId::Int, false, {}},
            {QStringLiteral("ngCount"), QStringLiteral("NG数"), false, Selt::DataTypeId::Int, false, {}}};
        d.parameters = {
            makeStringParam(QStringLiteral("key"), QStringLiteral("计数键"), QStringLiteral("counter"), 0),
            makeBoolParam(QStringLiteral("reset"), QStringLiteral("复位"), false, 1)};
        d.inputKeys = {QStringLiteral("ok")};
        d.outputKeys = {QStringLiteral("count"), QStringLiteral("okCount"), QStringLiteral("ngCount")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::aggregate(), QStringLiteral("聚合"),
                                      QStringLiteral("逻辑"), QStringLiteral("连续值求和/均值"));
        d.ports = {
            {QStringLiteral("value"), QStringLiteral("值"), true, Selt::DataTypeId::Real, true, {}},
            {QStringLiteral("count"), QStringLiteral("次数"), false, Selt::DataTypeId::Int, false, {}},
            {QStringLiteral("sum"), QStringLiteral("和"), false, Selt::DataTypeId::Real, false, {}},
            {QStringLiteral("mean"), QStringLiteral("均值"), false, Selt::DataTypeId::Real, false, {}}};
        d.parameters = {makeStringParam(QStringLiteral("key"), QStringLiteral("聚合键"), QStringLiteral("aggregate"), 0)};
        d.inputKeys = {QStringLiteral("value")};
        d.outputKeys = {QStringLiteral("count"), QStringLiteral("sum"), QStringLiteral("mean")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::resultWriter(), QStringLiteral("结果写出"),
                                      QStringLiteral("输出"), QStringLiteral("写出到结果 Sink"));
        d.ports = {
            {QStringLiteral("measurement"), QStringLiteral("测量"), true, Selt::DataTypeId::Measurement, false, {}},
            {QStringLiteral("state"), QStringLiteral("状态"), true, Selt::DataTypeId::String, false, {}},
            {QStringLiteral("ok"), QStringLiteral("成功"), false, Selt::DataTypeId::Bool, false, {}}};
        d.parameters = {
            makeStringParam(QStringLiteral("batchId"), QStringLiteral("批次ID"), {}, 0),
            makeStringParam(QStringLiteral("frameId"), QStringLiteral("帧ID"), {}, 1)};
        d.inputKeys = {QStringLiteral("measurement"), QStringLiteral("state")};
        d.outputKeys = {QStringLiteral("ok")};
        list.append(d);
    }

    // ---- Communication / data helpers ----
    {
        ModuleDescriptor d = makeBase(CommunicationNodeIds::serialRequest(), QStringLiteral("COM请求"),
                                      QStringLiteral("通讯"), QStringLiteral("串口发送/接收原始报文"));
        d.ports = {
            {QStringLiteral("payload"), QStringLiteral("报文"), true, Selt::DataTypeId::ByteArray, false, {}},
            {QStringLiteral("response"), QStringLiteral("响应"), false, Selt::DataTypeId::ByteArray, false, {}},
            {QStringLiteral("ok"), QStringLiteral("成功"), false, Selt::DataTypeId::Bool, false, {}},
            {QStringLiteral("error"), QStringLiteral("错误"), false, Selt::DataTypeId::String, false, {}}};
        d.parameters = {
            makeStringParam(QStringLiteral("portName"), QStringLiteral("端口名"), QStringLiteral("COM1"), 0,
                            QStringLiteral("例如 COM3"), QStringLiteral("连接")),
            makeIntParam(QStringLiteral("baudRate"), QStringLiteral("波特率"), 9600, 1200, 921600, 1,
                         QStringLiteral("连接")),
            makeEnumParam(QStringLiteral("encoding"), QStringLiteral("编码"),
                          {{QStringLiteral("raw"), QStringLiteral("原始/UTF-8")},
                           {QStringLiteral("ascii"), QStringLiteral("ASCII")},
                           {QStringLiteral("hex"), QStringLiteral("HEX")}},
                          QStringLiteral("hex"), 2, QStringLiteral("报文")),
            makeStringParam(QStringLiteral("payloadText"), QStringLiteral("报文文本"), QStringLiteral("01 03 00 00 00 01"), 3,
                            QStringLiteral("无输入端口时使用"), QStringLiteral("报文")),
            makeIntParam(QStringLiteral("expectMinBytes"), QStringLiteral("最少接收字节"), 1, 0, 65535, 4,
                         QStringLiteral("报文")),
            makeEnumParam(QStringLiteral("frameMode"), QStringLiteral("收帧策略"),
                          {{QStringLiteral("fixed_length"), QStringLiteral("固定长度")},
                           {QStringLiteral("idle_gap"), QStringLiteral("空闲间隔")},
                           {QStringLiteral("terminator"), QStringLiteral("帧尾")},
                           {QStringLiteral("max_bytes"), QStringLiteral("最大长度")}},
                          QStringLiteral("fixed_length"), 5, QStringLiteral("报文")),
            makeStringParam(QStringLiteral("connectionId"), QStringLiteral("连接资源ID"), {}, 6,
                            QStringLiteral("填写后走 CommunicationManager 复用连接"),
                            QStringLiteral("连接")),
            makeIntParam(QStringLiteral("timeoutMs"), QStringLiteral("超时ms"), 1000, 1, 60000, 7,
                         QStringLiteral("连接")),
            makeIntParam(QStringLiteral("retries"), QStringLiteral("重试"), 0, 0, 5, 8, QStringLiteral("连接")),
            makeBoolParam(QStringLiteral("useMock"), QStringLiteral("使用Mock传输"), true, 9,
                          QStringLiteral("安全"), QStringLiteral("测试/无硬件；现场请关闭"))};
        d.inputKeys = {QStringLiteral("payload")};
        d.outputKeys = {QStringLiteral("response"), QStringLiteral("ok"), QStringLiteral("error")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(CommunicationNodeIds::tcpRequest(), QStringLiteral("TCP请求"),
                                      QStringLiteral("通讯"), QStringLiteral("TCP/IP 客户端发送/接收"));
        d.ports = {
            {QStringLiteral("payload"), QStringLiteral("报文"), true, Selt::DataTypeId::ByteArray, false, {}},
            {QStringLiteral("response"), QStringLiteral("响应"), false, Selt::DataTypeId::ByteArray, false, {}},
            {QStringLiteral("ok"), QStringLiteral("成功"), false, Selt::DataTypeId::Bool, false, {}},
            {QStringLiteral("error"), QStringLiteral("错误"), false, Selt::DataTypeId::String, false, {}}};
        d.parameters = {
            makeStringParam(QStringLiteral("host"), QStringLiteral("主机"), QStringLiteral("127.0.0.1"), 0, {},
                            QStringLiteral("连接")),
            makeIntParam(QStringLiteral("port"), QStringLiteral("端口"), 502, 1, 65535, 1, QStringLiteral("连接")),
            makeEnumParam(QStringLiteral("encoding"), QStringLiteral("编码"),
                          {{QStringLiteral("raw"), QStringLiteral("原始/UTF-8")},
                           {QStringLiteral("ascii"), QStringLiteral("ASCII")},
                           {QStringLiteral("hex"), QStringLiteral("HEX")}},
                          QStringLiteral("hex"), 2, QStringLiteral("报文")),
            makeStringParam(QStringLiteral("payloadText"), QStringLiteral("报文文本"), QStringLiteral("Hello"), 3, {},
                            QStringLiteral("报文")),
            makeIntParam(QStringLiteral("expectMinBytes"), QStringLiteral("最少接收字节"), 1, 0, 65535, 4,
                         QStringLiteral("报文")),
            makeEnumParam(QStringLiteral("frameMode"), QStringLiteral("收帧策略"),
                          {{QStringLiteral("fixed_length"), QStringLiteral("固定长度")},
                           {QStringLiteral("idle_gap"), QStringLiteral("空闲间隔")},
                           {QStringLiteral("terminator"), QStringLiteral("帧尾")},
                           {QStringLiteral("max_bytes"), QStringLiteral("最大长度")}},
                          QStringLiteral("fixed_length"), 5, QStringLiteral("报文")),
            makeStringParam(QStringLiteral("connectionId"), QStringLiteral("连接资源ID"), {}, 6,
                            QStringLiteral("填写后走 CommunicationManager 复用连接"),
                            QStringLiteral("连接")),
            makeIntParam(QStringLiteral("timeoutMs"), QStringLiteral("超时ms"), 1000, 1, 60000, 7,
                         QStringLiteral("连接")),
            makeIntParam(QStringLiteral("retries"), QStringLiteral("重试"), 0, 0, 5, 8, QStringLiteral("连接")),
            makeBoolParam(QStringLiteral("useMock"), QStringLiteral("使用Mock传输"), true, 9,
                          QStringLiteral("安全"), QStringLiteral("测试/无硬件；现场请关闭"))};
        d.inputKeys = {QStringLiteral("payload")};
        d.outputKeys = {QStringLiteral("response"), QStringLiteral("ok"), QStringLiteral("error")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(CommunicationNodeIds::modbusRead(), QStringLiteral("Modbus读"),
                                      QStringLiteral("通讯"),
                                      QStringLiteral("读线圈/离散输入/保持/输入寄存器（零基地址）"));
        d.ports = {
            {QStringLiteral("value"), QStringLiteral("数值"), false, Selt::DataTypeId::Real, false, {}},
            {QStringLiteral("text"), QStringLiteral("文本"), false, Selt::DataTypeId::String, false, {}},
            {QStringLiteral("ok"), QStringLiteral("成功"), false, Selt::DataTypeId::Bool, false, {}},
            {QStringLiteral("error"), QStringLiteral("错误"), false, Selt::DataTypeId::String, false, {}}};
        d.parameters = {
            makeEnumParam(QStringLiteral("transport"), QStringLiteral("传输"),
                          {{QStringLiteral("tcp"), QStringLiteral("Modbus TCP")},
                           {QStringLiteral("rtu"), QStringLiteral("Modbus RTU")}},
                          QStringLiteral("tcp"), 0, QStringLiteral("连接")),
            makeStringParam(QStringLiteral("host"), QStringLiteral("主机"), QStringLiteral("127.0.0.1"), 1, {},
                            QStringLiteral("连接")),
            makeIntParam(QStringLiteral("port"), QStringLiteral("端口"), 502, 1, 65535, 2, QStringLiteral("连接")),
            makeStringParam(QStringLiteral("portName"), QStringLiteral("串口"), QStringLiteral("COM1"), 3, {},
                            QStringLiteral("连接")),
            makeIntParam(QStringLiteral("unitId"), QStringLiteral("站号"), 1, 0, 255, 4, QStringLiteral("连接")),
            makeEnumParam(QStringLiteral("table"), QStringLiteral("功能区"),
                          {{QStringLiteral("coils"), QStringLiteral("线圈")},
                           {QStringLiteral("discrete"), QStringLiteral("离散输入")},
                           {QStringLiteral("holding"), QStringLiteral("保持寄存器")},
                           {QStringLiteral("input"), QStringLiteral("输入寄存器")}},
                          QStringLiteral("holding"), 5, QStringLiteral("访问")),
            makeIntParam(QStringLiteral("address"), QStringLiteral("地址(0基)"), 0, 0, 65535, 6,
                         QStringLiteral("访问"), QStringLiteral("协议零基地址；设备面板若显示 40001 请减一")),
            makeIntParam(QStringLiteral("quantity"), QStringLiteral("数量"), 1, 1, 125, 7, QStringLiteral("访问")),
            makeEnumParam(QStringLiteral("valueKind"), QStringLiteral("值类型"),
                          {{QStringLiteral("uint16"), QStringLiteral("UInt16")},
                           {QStringLiteral("int16"), QStringLiteral("Int16")},
                           {QStringLiteral("uint32"), QStringLiteral("UInt32")},
                           {QStringLiteral("int32"), QStringLiteral("Int32")},
                           {QStringLiteral("float32"), QStringLiteral("Float32")},
                           {QStringLiteral("float64"), QStringLiteral("Float64")},
                           {QStringLiteral("ascii"), QStringLiteral("ASCII字符串")},
                           {QStringLiteral("hex"), QStringLiteral("HEX字符串")},
                           {QStringLiteral("coil"), QStringLiteral("线圈布尔")}},
                          QStringLiteral("uint16"), 8, QStringLiteral("编解码")),
            makeEnumParam(QStringLiteral("wordOrder"), QStringLiteral("字序"),
                          {{QStringLiteral("ABCD"), QStringLiteral("ABCD")},
                           {QStringLiteral("CDAB"), QStringLiteral("CDAB")},
                           {QStringLiteral("BADC"), QStringLiteral("BADC")},
                           {QStringLiteral("DCBA"), QStringLiteral("DCBA")}},
                          QStringLiteral("ABCD"), 9, QStringLiteral("编解码")),
            makeIntParam(QStringLiteral("timeoutMs"), QStringLiteral("超时ms"), 1000, 1, 60000, 10,
                         QStringLiteral("连接")),
            makeStringParam(QStringLiteral("connectionId"), QStringLiteral("连接资源ID"), {}, 11,
                            QStringLiteral("填写后复用 CommunicationManager 连接"), QStringLiteral("连接")),
            makeBoolParam(QStringLiteral("useMock"), QStringLiteral("使用Mock传输"), true, 12,
                          QStringLiteral("安全"), QStringLiteral("测试/无硬件"))};
        d.outputKeys = {QStringLiteral("value"), QStringLiteral("text"), QStringLiteral("ok"),
                        QStringLiteral("error")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(CommunicationNodeIds::modbusWrite(), QStringLiteral("Modbus写"),
                                      QStringLiteral("通讯"),
                                      QStringLiteral("写线圈/保持寄存器；默认写保护"));
        d.ports = {
            {QStringLiteral("value"), QStringLiteral("数值"), true, Selt::DataTypeId::Real, false,
             QStringLiteral("测量值或门控数值；布尔 OK 亦可连入并转为 0/1")},
            {QStringLiteral("okIn"), QStringLiteral("OK门控"), true, Selt::DataTypeId::Bool, false,
             QStringLiteral("可选：OK/NG 写线圈时优先使用")},
            {QStringLiteral("text"), QStringLiteral("文本"), true, Selt::DataTypeId::String, false, {}},
            {QStringLiteral("ok"), QStringLiteral("成功"), false, Selt::DataTypeId::Bool, false, {}},
            {QStringLiteral("error"), QStringLiteral("错误"), false, Selt::DataTypeId::String, false, {}}};
        d.parameters = {
            makeEnumParam(QStringLiteral("transport"), QStringLiteral("传输"),
                          {{QStringLiteral("tcp"), QStringLiteral("Modbus TCP")},
                           {QStringLiteral("rtu"), QStringLiteral("Modbus RTU")}},
                          QStringLiteral("tcp"), 0, QStringLiteral("连接")),
            makeStringParam(QStringLiteral("host"), QStringLiteral("主机"), QStringLiteral("127.0.0.1"), 1, {},
                            QStringLiteral("连接")),
            makeIntParam(QStringLiteral("port"), QStringLiteral("端口"), 502, 1, 65535, 2, QStringLiteral("连接")),
            makeStringParam(QStringLiteral("portName"), QStringLiteral("串口"), QStringLiteral("COM1"), 3, {},
                            QStringLiteral("连接")),
            makeIntParam(QStringLiteral("unitId"), QStringLiteral("站号"), 1, 0, 255, 4, QStringLiteral("连接")),
            makeEnumParam(QStringLiteral("table"), QStringLiteral("功能区"),
                          {{QStringLiteral("coils"), QStringLiteral("线圈")},
                           {QStringLiteral("holding"), QStringLiteral("保持寄存器")}},
                          QStringLiteral("holding"), 5, QStringLiteral("访问")),
            makeIntParam(QStringLiteral("address"), QStringLiteral("地址(0基)"), 0, 0, 65535, 6,
                         QStringLiteral("访问"), QStringLiteral("协议零基地址")),
            makeEnumParam(QStringLiteral("valueKind"), QStringLiteral("值类型"),
                          {{QStringLiteral("uint16"), QStringLiteral("UInt16")},
                           {QStringLiteral("int16"), QStringLiteral("Int16")},
                           {QStringLiteral("uint32"), QStringLiteral("UInt32")},
                           {QStringLiteral("int32"), QStringLiteral("Int32")},
                           {QStringLiteral("float32"), QStringLiteral("Float32")},
                           {QStringLiteral("ascii"), QStringLiteral("ASCII字符串")},
                           {QStringLiteral("coil"), QStringLiteral("线圈布尔")}},
                          QStringLiteral("uint16"), 7, QStringLiteral("编解码")),
            makeEnumParam(QStringLiteral("wordOrder"), QStringLiteral("字序"),
                          {{QStringLiteral("ABCD"), QStringLiteral("ABCD")},
                           {QStringLiteral("CDAB"), QStringLiteral("CDAB")},
                           {QStringLiteral("BADC"), QStringLiteral("BADC")},
                           {QStringLiteral("DCBA"), QStringLiteral("DCBA")}},
                          QStringLiteral("ABCD"), 8, QStringLiteral("编解码")),
            makeIntParam(QStringLiteral("stringRegisters"), QStringLiteral("字符串寄存器数"), 4, 1, 60, 9,
                         QStringLiteral("编解码")),
            makeDoubleParam(QStringLiteral("value"), QStringLiteral("数值(无绑定时)"), 0.0, -1e18, 1e18, 10,
                            QStringLiteral("访问")),
            makeStringParam(QStringLiteral("text"), QStringLiteral("文本(无绑定时)"), {}, 11, {},
                            QStringLiteral("访问")),
            makeBoolParam(QStringLiteral("allowWrite"), QStringLiteral("允许写入"), false, 12,
                          QStringLiteral("安全"), QStringLiteral("默认关闭写保护，现场需显式打开")),
            makeStringParam(QStringLiteral("connectionId"), QStringLiteral("连接资源ID"), {}, 13,
                            QStringLiteral("填写后复用 CommunicationManager 连接"), QStringLiteral("连接")),
            makeBoolParam(QStringLiteral("useMock"), QStringLiteral("使用Mock传输"), true, 14,
                          QStringLiteral("安全"), QStringLiteral("测试/无硬件")),
            makeIntParam(QStringLiteral("timeoutMs"), QStringLiteral("超时ms"), 1000, 1, 60000, 15,
                         QStringLiteral("连接"))};
        d.inputKeys = {QStringLiteral("value"), QStringLiteral("okIn"), QStringLiteral("text")};
        d.outputKeys = {QStringLiteral("ok"), QStringLiteral("error")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(CommunicationNodeIds::encode(), QStringLiteral("编码"),
                                      QStringLiteral("数据"), QStringLiteral("数值/字符串→寄存器或字节"));
        d.ports = {
            {QStringLiteral("value"), QStringLiteral("数值"), true, Selt::DataTypeId::Real, false, {}},
            {QStringLiteral("text"), QStringLiteral("文本"), true, Selt::DataTypeId::String, false, {}},
            {QStringLiteral("bytes"), QStringLiteral("字节"), false, Selt::DataTypeId::ByteArray, false, {}},
            {QStringLiteral("ok"), QStringLiteral("成功"), false, Selt::DataTypeId::Bool, false, {}}};
        d.parameters = {
            makeEnumParam(QStringLiteral("valueKind"), QStringLiteral("类型"),
                          {{QStringLiteral("uint16"), QStringLiteral("UInt16")},
                           {QStringLiteral("int32"), QStringLiteral("Int32")},
                           {QStringLiteral("float32"), QStringLiteral("Float32")},
                           {QStringLiteral("ascii"), QStringLiteral("ASCII")},
                           {QStringLiteral("hex"), QStringLiteral("HEX")},
                           {QStringLiteral("decimal_ascii"), QStringLiteral("十进制ASCII")}},
                          QStringLiteral("float32"), 0),
            makeEnumParam(QStringLiteral("wordOrder"), QStringLiteral("字序"),
                          {{QStringLiteral("ABCD"), QStringLiteral("ABCD")},
                           {QStringLiteral("CDAB"), QStringLiteral("CDAB")},
                           {QStringLiteral("BADC"), QStringLiteral("BADC")},
                           {QStringLiteral("DCBA"), QStringLiteral("DCBA")}},
                          QStringLiteral("ABCD"), 1),
            makeIntParam(QStringLiteral("stringRegisters"), QStringLiteral("字符串寄存器数"), 4, 1, 60, 2),
            makeDoubleParam(QStringLiteral("value"), QStringLiteral("数值"), 0.0, -1e18, 1e18, 3),
            makeStringParam(QStringLiteral("text"), QStringLiteral("文本"), {}, 4)};
        d.inputKeys = {QStringLiteral("value"), QStringLiteral("text")};
        d.outputKeys = {QStringLiteral("bytes"), QStringLiteral("ok")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(CommunicationNodeIds::decode(), QStringLiteral("解码"),
                                      QStringLiteral("数据"), QStringLiteral("字节/寄存器→数值或字符串"));
        d.ports = {
            {QStringLiteral("bytes"), QStringLiteral("字节"), true, Selt::DataTypeId::ByteArray, false, {}},
            {QStringLiteral("value"), QStringLiteral("数值"), false, Selt::DataTypeId::Real, false, {}},
            {QStringLiteral("text"), QStringLiteral("文本"), false, Selt::DataTypeId::String, false, {}},
            {QStringLiteral("ok"), QStringLiteral("成功"), false, Selt::DataTypeId::Bool, false, {}}};
        d.parameters = {
            makeEnumParam(QStringLiteral("valueKind"), QStringLiteral("类型"),
                          {{QStringLiteral("uint16"), QStringLiteral("UInt16")},
                           {QStringLiteral("int32"), QStringLiteral("Int32")},
                           {QStringLiteral("float32"), QStringLiteral("Float32")},
                           {QStringLiteral("ascii"), QStringLiteral("ASCII")},
                           {QStringLiteral("hex"), QStringLiteral("HEX")},
                           {QStringLiteral("decimal_ascii"), QStringLiteral("十进制ASCII")}},
                          QStringLiteral("float32"), 0),
            makeEnumParam(QStringLiteral("wordOrder"), QStringLiteral("字序"),
                          {{QStringLiteral("ABCD"), QStringLiteral("ABCD")},
                           {QStringLiteral("CDAB"), QStringLiteral("CDAB")},
                           {QStringLiteral("BADC"), QStringLiteral("BADC")},
                           {QStringLiteral("DCBA"), QStringLiteral("DCBA")}},
                          QStringLiteral("ABCD"), 1),
            makeStringParam(QStringLiteral("hexText"), QStringLiteral("HEX文本(无输入)"), {}, 2)};
        d.inputKeys = {QStringLiteral("bytes")};
        d.outputKeys = {QStringLiteral("value"), QStringLiteral("text"), QStringLiteral("ok")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(CommunicationNodeIds::numeric(), QStringLiteral("数值运算"),
                                      QStringLiteral("数据"), QStringLiteral("加减乘除与缩放"));
        d.ports = {
            {QStringLiteral("a"), QStringLiteral("A"), true, Selt::DataTypeId::Real, false, {}},
            {QStringLiteral("b"), QStringLiteral("B"), true, Selt::DataTypeId::Real, false, {}},
            {QStringLiteral("out"), QStringLiteral("结果"), false, Selt::DataTypeId::Real, false, {}},
            {QStringLiteral("ok"), QStringLiteral("成功"), false, Selt::DataTypeId::Bool, false, {}}};
        d.parameters = {
            makeEnumParam(QStringLiteral("op"), QStringLiteral("运算"),
                          {{QStringLiteral("add"), QStringLiteral("加")},
                           {QStringLiteral("sub"), QStringLiteral("减")},
                           {QStringLiteral("mul"), QStringLiteral("乘")},
                           {QStringLiteral("div"), QStringLiteral("除")},
                           {QStringLiteral("scale"), QStringLiteral("缩放 a*scale+offset")}},
                          QStringLiteral("add"), 0),
            makeDoubleParam(QStringLiteral("a"), QStringLiteral("A"), 0.0, -1e18, 1e18, 1),
            makeDoubleParam(QStringLiteral("b"), QStringLiteral("B"), 0.0, -1e18, 1e18, 2),
            makeDoubleParam(QStringLiteral("scale"), QStringLiteral("比例"), 1.0, -1e9, 1e9, 3),
            makeDoubleParam(QStringLiteral("offset"), QStringLiteral("偏移"), 0.0, -1e9, 1e9, 4)};
        d.inputKeys = {QStringLiteral("a"), QStringLiteral("b")};
        d.outputKeys = {QStringLiteral("out"), QStringLiteral("ok")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(CommunicationNodeIds::stringOp(), QStringLiteral("字符串处理"),
                                      QStringLiteral("数据"), QStringLiteral("拼接/格式化/十进制ASCII"));
        d.ports = {
            {QStringLiteral("a"), QStringLiteral("A"), true, Selt::DataTypeId::String, false, {}},
            {QStringLiteral("b"), QStringLiteral("B"), true, Selt::DataTypeId::String, false, {}},
            {QStringLiteral("out"), QStringLiteral("结果"), false, Selt::DataTypeId::String, false, {}},
            {QStringLiteral("bytes"), QStringLiteral("字节"), false, Selt::DataTypeId::ByteArray, false, {}},
            {QStringLiteral("ok"), QStringLiteral("成功"), false, Selt::DataTypeId::Bool, false, {}}};
        d.parameters = {
            makeEnumParam(QStringLiteral("op"), QStringLiteral("操作"),
                          {{QStringLiteral("concat"), QStringLiteral("拼接")},
                           {QStringLiteral("format"), QStringLiteral("格式化")},
                           {QStringLiteral("decimal_to_ascii"), QStringLiteral("十进制→ASCII")},
                           {QStringLiteral("ascii_to_decimal"), QStringLiteral("ASCII→十进制")},
                           {QStringLiteral("to_hex"), QStringLiteral("转HEX")},
                           {QStringLiteral("from_hex"), QStringLiteral("从HEX")}},
                          QStringLiteral("concat"), 0),
            makeStringParam(QStringLiteral("a"), QStringLiteral("A"), {}, 1),
            makeStringParam(QStringLiteral("b"), QStringLiteral("B"), {}, 2),
            makeStringParam(QStringLiteral("format"), QStringLiteral("格式"), QStringLiteral("{0}{1}"), 3),
            makeDoubleParam(QStringLiteral("number"), QStringLiteral("数值"), 0.0, -1e18, 1e18, 4)};
        d.inputKeys = {QStringLiteral("a"), QStringLiteral("b")};
        d.outputKeys = {QStringLiteral("out"), QStringLiteral("bytes"), QStringLiteral("ok")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(CommunicationNodeIds::checksum(), QStringLiteral("校验和"),
                                      QStringLiteral("数据"), QStringLiteral("CRC16/LRC/XOR/Sum16"));
        d.ports = {
            {QStringLiteral("bytes"), QStringLiteral("字节"), true, Selt::DataTypeId::ByteArray, false, {}},
            {QStringLiteral("value"), QStringLiteral("校验值"), false, Selt::DataTypeId::Int, false, {}},
            {QStringLiteral("bytesOut"), QStringLiteral("附加后"), false, Selt::DataTypeId::ByteArray, false,
             {}},
            {QStringLiteral("ok"), QStringLiteral("成功"), false, Selt::DataTypeId::Bool, false, {}}};
        // keep output key "bytes" for appended payload
        d.ports[2].id = QStringLiteral("bytes");
        d.parameters = {
            makeEnumParam(QStringLiteral("algo"), QStringLiteral("算法"),
                          {{QStringLiteral("crc16"), QStringLiteral("CRC16-Modbus")},
                           {QStringLiteral("lrc"), QStringLiteral("LRC")},
                           {QStringLiteral("xor"), QStringLiteral("异或")},
                           {QStringLiteral("sum16"), QStringLiteral("16位累加")}},
                          QStringLiteral("crc16"), 0),
            makeStringParam(QStringLiteral("hexText"), QStringLiteral("HEX文本(无输入)"), {}, 1)};
        d.inputKeys = {QStringLiteral("bytes")};
        d.outputKeys = {QStringLiteral("value"), QStringLiteral("bytes"), QStringLiteral("ok")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(CommunicationNodeIds::frameWrap(), QStringLiteral("帧封装"),
                                      QStringLiteral("数据"), QStringLiteral("追加帧头/帧尾/CRC"));
        d.ports = {
            {QStringLiteral("bytes"), QStringLiteral("报文体"), true, Selt::DataTypeId::ByteArray, false, {}},
            {QStringLiteral("bytes"), QStringLiteral("完整帧"), false, Selt::DataTypeId::ByteArray, false, {}},
            {QStringLiteral("length"), QStringLiteral("长度"), false, Selt::DataTypeId::Int, false, {}},
            {QStringLiteral("ok"), QStringLiteral("成功"), false, Selt::DataTypeId::Bool, false, {}}};
        // Fix duplicate port id - use out for output
        d.ports[1].id = QStringLiteral("out");
        d.ports[1].name = QStringLiteral("完整帧");
        d.parameters = {
            makeStringParam(QStringLiteral("headHex"), QStringLiteral("帧头HEX"), {}, 0),
            makeStringParam(QStringLiteral("tailHex"), QStringLiteral("帧尾HEX"), {}, 1),
            makeBoolParam(QStringLiteral("appendCrc16"), QStringLiteral("附加CRC16"), false, 2,
                          QStringLiteral("报文")),
            makeEnumParam(QStringLiteral("encoding"), QStringLiteral("正文编码"),
                          {{QStringLiteral("hex"), QStringLiteral("HEX")},
                           {QStringLiteral("ascii"), QStringLiteral("ASCII")},
                           {QStringLiteral("raw"), QStringLiteral("UTF-8")}},
                          QStringLiteral("hex"), 3),
            makeStringParam(QStringLiteral("payloadText"), QStringLiteral("正文(无输入)"), {}, 4)};
        d.inputKeys = {QStringLiteral("bytes")};
        d.outputKeys = {QStringLiteral("out"), QStringLiteral("length"), QStringLiteral("ok")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(CommunicationNodeIds::clampScale(), QStringLiteral("限幅缩放"),
                                      QStringLiteral("数据"), QStringLiteral("scale/offset 后限幅"));
        d.ports = {
            {QStringLiteral("value"), QStringLiteral("输入"), true, Selt::DataTypeId::Real, false, {}},
            {QStringLiteral("out"), QStringLiteral("输出"), false, Selt::DataTypeId::Real, false, {}},
            {QStringLiteral("ok"), QStringLiteral("成功"), false, Selt::DataTypeId::Bool, false, {}}};
        d.parameters = {
            makeDoubleParam(QStringLiteral("scale"), QStringLiteral("比例"), 1.0, -1e9, 1e9, 0),
            makeDoubleParam(QStringLiteral("offset"), QStringLiteral("偏移"), 0.0, -1e9, 1e9, 1),
            makeDoubleParam(QStringLiteral("min"), QStringLiteral("下限"), -1e18, -1e18, 1e18, 2),
            makeDoubleParam(QStringLiteral("max"), QStringLiteral("上限"), 1e18, -1e18, 1e18, 3),
            makeDoubleParam(QStringLiteral("value"), QStringLiteral("值(无绑定时)"), 0.0, -1e18, 1e18, 4)};
        d.inputKeys = {QStringLiteral("value")};
        d.outputKeys = {QStringLiteral("out"), QStringLiteral("ok")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(CommunicationNodeIds::registerTable(), QStringLiteral("寄存器表"),
                                      QStringLiteral("数据"), QStringLiteral("字节→寄存器 Table"));
        d.ports = {
            {QStringLiteral("bytes"), QStringLiteral("字节"), true, Selt::DataTypeId::ByteArray, false, {}},
            {QStringLiteral("table"), QStringLiteral("表格"), false, Selt::DataTypeId::Table, false, {}},
            {QStringLiteral("count"), QStringLiteral("数量"), false, Selt::DataTypeId::Int, false, {}},
            {QStringLiteral("ok"), QStringLiteral("成功"), false, Selt::DataTypeId::Bool, false, {}}};
        d.parameters = {makeStringParam(QStringLiteral("hexText"), QStringLiteral("HEX文本(无输入)"), {}, 0)};
        d.inputKeys = {QStringLiteral("bytes")};
        d.outputKeys = {QStringLiteral("table"), QStringLiteral("count"), QStringLiteral("ok")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::morphGradient(), QStringLiteral("形态学梯度"),
                                      QStringLiteral("预处理"), QStringLiteral("膨胀减腐蚀突出边缘"));
        d.ports = {
            {QStringLiteral("image"), QStringLiteral("图像"), true, Selt::DataTypeId::Image, true, {}},
            {QStringLiteral("image"), QStringLiteral("输出"), false, Selt::DataTypeId::Image, false, {}}};
        d.ports[1].id = QStringLiteral("out");
        d.parameters = {
            makeIntParam(QStringLiteral("ksize"), QStringLiteral("核大小"), 3, 1, 31, 0),
            makeRoiParam(QStringLiteral("roi"), QStringLiteral("ROI"), 1)};
        d.inputKeys = {QStringLiteral("image")};
        d.outputKeys = {QStringLiteral("out"), QStringLiteral("image")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::pointLineDistance(), QStringLiteral("点线距离"),
                                      QStringLiteral("测量"), QStringLiteral("点到直线距离"));
        d.ports = {
            {QStringLiteral("point"), QStringLiteral("点"), true, Selt::DataTypeId::Point2D, true, {}},
            {QStringLiteral("line"), QStringLiteral("直线"), true, Selt::DataTypeId::Line, true, {}},
            {QStringLiteral("distance"), QStringLiteral("距离"), false, Selt::DataTypeId::Real, false, {}},
            {QStringLiteral("ok"), QStringLiteral("成功"), false, Selt::DataTypeId::Bool, false, {}}};
        d.inputKeys = {QStringLiteral("point"), QStringLiteral("line")};
        d.outputKeys = {QStringLiteral("distance"), QStringLiteral("ok")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::circularityMeasure(), QStringLiteral("圆度测量"),
                                      QStringLiteral("测量"), QStringLiteral("4πA/P² 圆度指标"));
        d.ports = {
            {QStringLiteral("contour"), QStringLiteral("轮廓"), true, Selt::DataTypeId::Contour, false, {}},
            {QStringLiteral("image"), QStringLiteral("图像"), true, Selt::DataTypeId::Image, false, {}},
            {QStringLiteral("circularity"), QStringLiteral("圆度"), false, Selt::DataTypeId::Real, false, {}},
            {QStringLiteral("ok"), QStringLiteral("成功"), false, Selt::DataTypeId::Bool, false, {}}};
        d.parameters = {
            makeIntParam(QStringLiteral("minArea"), QStringLiteral("最小面积"), 20, 1, 1000000, 0),
            makeRoiParam(QStringLiteral("roi"), QStringLiteral("ROI"), 1)};
        d.inputKeys = {QStringLiteral("contour"), QStringLiteral("image")};
        d.outputKeys = {QStringLiteral("circularity"), QStringLiteral("ok")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::pointPointDistance(), QStringLiteral("点到点距离"),
                                      QStringLiteral("测量"), QStringLiteral("由 Point2D 端口计算两点距离"));
        d.ports = {
            {QStringLiteral("point1"), QStringLiteral("点1"), true, Selt::DataTypeId::Point2D, true, {}},
            {QStringLiteral("point2"), QStringLiteral("点2"), true, Selt::DataTypeId::Point2D, true, {}},
            {QStringLiteral("distance"), QStringLiteral("距离"), false, Selt::DataTypeId::Real, false, {}},
            {QStringLiteral("measurement"), QStringLiteral("测量"), false, Selt::DataTypeId::Measurement, false, {}}};
        d.parameters = {
            makeDoubleParam(QStringLiteral("x1"), QStringLiteral("X1"), 0.0, -1e9, 1e9, 0),
            makeDoubleParam(QStringLiteral("y1"), QStringLiteral("Y1"), 0.0, -1e9, 1e9, 1),
            makeDoubleParam(QStringLiteral("x2"), QStringLiteral("X2"), 100.0, -1e9, 1e9, 2),
            makeDoubleParam(QStringLiteral("y2"), QStringLiteral("Y2"), 0.0, -1e9, 1e9, 3)};
        d.inputKeys = {QStringLiteral("point1"), QStringLiteral("point2")};
        d.outputKeys = {QStringLiteral("distance"), QStringLiteral("measurement")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::perpendicularityMeasure(), QStringLiteral("垂直度"),
                                      QStringLiteral("测量"), QStringLiteral("两直线相对 90° 的偏差"));
        d.ports = {
            {QStringLiteral("line1"), QStringLiteral("直线1"), true, Selt::DataTypeId::Line, true, {}},
            {QStringLiteral("line2"), QStringLiteral("直线2"), true, Selt::DataTypeId::Line, true, {}},
            {QStringLiteral("angle"), QStringLiteral("夹角"), false, Selt::DataTypeId::Real, false, {}},
            {QStringLiteral("deviation"), QStringLiteral("偏差"), false, Selt::DataTypeId::Real, false, {}},
            {QStringLiteral("measurement"), QStringLiteral("测量"), false, Selt::DataTypeId::Measurement, false, {}}};
        d.inputKeys = {QStringLiteral("line1"), QStringLiteral("line2")};
        d.outputKeys = {QStringLiteral("angle"), QStringLiteral("deviation"), QStringLiteral("measurement")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::positionDeviation(), QStringLiteral("位置偏差"),
                                      QStringLiteral("测量"), QStringLiteral("相对基准点的位置偏差"));
        d.ports = {
            {QStringLiteral("point"), QStringLiteral("实际点"), true, Selt::DataTypeId::Point2D, true, {}},
            {QStringLiteral("reference"), QStringLiteral("基准点"), true, Selt::DataTypeId::Point2D, false, {}},
            {QStringLiteral("dx"), QStringLiteral("ΔX"), false, Selt::DataTypeId::Real, false, {}},
            {QStringLiteral("dy"), QStringLiteral("ΔY"), false, Selt::DataTypeId::Real, false, {}},
            {QStringLiteral("distance"), QStringLiteral("距离"), false, Selt::DataTypeId::Real, false, {}},
            {QStringLiteral("measurement"), QStringLiteral("测量"), false, Selt::DataTypeId::Measurement, false, {}}};
        d.parameters = {
            makeDoubleParam(QStringLiteral("refX"), QStringLiteral("基准X"), 0.0, -1e9, 1e9, 0),
            makeDoubleParam(QStringLiteral("refY"), QStringLiteral("基准Y"), 0.0, -1e9, 1e9, 1)};
        d.inputKeys = {QStringLiteral("point"), QStringLiteral("reference")};
        d.outputKeys = {QStringLiteral("dx"), QStringLiteral("dy"), QStringLiteral("distance"),
                        QStringLiteral("measurement")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::referencePoint(), QStringLiteral("基准点"),
                                      QStringLiteral("测量"), QStringLiteral("定义或透传基准点"));
        d.ports = {
            {QStringLiteral("in"), QStringLiteral("输入点"), true, Selt::DataTypeId::Point2D, false, {}},
            {QStringLiteral("point"), QStringLiteral("基准"), false, Selt::DataTypeId::Point2D, false, {}},
            {QStringLiteral("x"), QStringLiteral("X"), false, Selt::DataTypeId::Real, false, {}},
            {QStringLiteral("y"), QStringLiteral("Y"), false, Selt::DataTypeId::Real, false, {}}};
        d.parameters = {
            makeDoubleParam(QStringLiteral("x"), QStringLiteral("X"), 0.0, -1e9, 1e9, 0),
            makeDoubleParam(QStringLiteral("y"), QStringLiteral("Y"), 0.0, -1e9, 1e9, 1)};
        d.inputKeys = {QStringLiteral("in")};
        d.outputKeys = {QStringLiteral("point"), QStringLiteral("x"), QStringLiteral("y")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::variableWrite(), QStringLiteral("变量写入"),
                                      QStringLiteral("逻辑"), QStringLiteral("将值写入运行时变量（不落盘）"));
        d.ports = {
            {QStringLiteral("value"), QStringLiteral("值"), true, Selt::DataTypeId::Real, false, {}},
            {QStringLiteral("out"), QStringLiteral("写出"), false, Selt::DataTypeId::Real, false,
             QStringLiteral("透传写出值（与输入 value 相同）")},
            {QStringLiteral("key"), QStringLiteral("键"), false, Selt::DataTypeId::String, false, {}},
            {QStringLiteral("ok"), QStringLiteral("成功"), false, Selt::DataTypeId::Bool, false, {}}};
        d.parameters = {
            makeStringParam(QStringLiteral("key"), QStringLiteral("变量键"), QStringLiteral("result"), 0)};
        d.inputKeys = {QStringLiteral("value")};
        d.outputKeys = {QStringLiteral("out"), QStringLiteral("key"), QStringLiteral("ok")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::inspectionPack(), QStringLiteral("检测结果打包"),
                                      QStringLiteral("判定"), QStringLiteral("将 OK/NG 与数值打包为检测结果"));
        d.ports = {
            {QStringLiteral("ok"), QStringLiteral("OK输入"), true, Selt::DataTypeId::Bool, true, {}},
            {QStringLiteral("value"), QStringLiteral("值输入"), true, Selt::DataTypeId::Real, false, {}},
            {QStringLiteral("pass"), QStringLiteral("OK"), false, Selt::DataTypeId::Bool, false,
             QStringLiteral("打包后的判定结果")},
            {QStringLiteral("resultValue"), QStringLiteral("结果值"), false, Selt::DataTypeId::Real, false, {}},
            {QStringLiteral("state"), QStringLiteral("状态"), false, Selt::DataTypeId::String, false, {}},
            {QStringLiteral("message"), QStringLiteral("消息"), false, Selt::DataTypeId::String, false, {}},
            {QStringLiteral("measurement"), QStringLiteral("测量"), false, Selt::DataTypeId::Measurement, false, {}}};
        d.parameters = {
            makeStringParam(QStringLiteral("message"), QStringLiteral("消息"), QString(), 0)};
        d.inputKeys = {QStringLiteral("ok"), QStringLiteral("value")};
        d.outputKeys = {QStringLiteral("pass"), QStringLiteral("resultValue"), QStringLiteral("state"),
                        QStringLiteral("message"), QStringLiteral("measurement")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::subflow(), QStringLiteral("子流程"),
                                      QStringLiteral("流程"),
                                      QStringLiteral("对外 Terminal 节点；内部流程通过接口映射连接"));
        d.helpText = QStringLiteral("仅显示对外端口。完整契约保存在 subflowInterface 参数中。");
        d.capabilityVersion = 1;
        d.ports = {
            {QStringLiteral("in"), QStringLiteral("输入"), true, Selt::DataTypeId::Image, true,
             QStringLiteral("默认图像输入 Terminal")},
            {QStringLiteral("out"), QStringLiteral("输出"), false, Selt::DataTypeId::Image, false,
             QStringLiteral("默认图像输出 Terminal")}};
        d.parameters = {
            makeStringParam(QStringLiteral("flowId"), QStringLiteral("内部流程ID"), QString(), 0),
            makeStringParam(QStringLiteral("displayName"), QStringLiteral("显示名"),
                            QStringLiteral("子流程"), 1)};
        d.inputKeys = {QStringLiteral("in")};
        d.outputKeys = {QStringLiteral("out")};
        list.append(d);
    }

    for (ModuleDescriptor &descriptor : list) {
        // Optional ROI input for nodes that already expose an ROI parameter.
        bool hasRoiParam = false;
        for (const ModuleParamDef &param : descriptor.parameters) {
            if (param.key == QLatin1String("roi")
                || param.type == ModuleParamType::RoiRect) {
                hasRoiParam = true;
                break;
            }
        }
        if (hasRoiParam && !descriptor.findInputPort(QStringLiteral("roi"))) {
            ModulePortDef roiPort;
            roiPort.id = QStringLiteral("roi");
            roiPort.name = QStringLiteral("ROI");
            roiPort.isInput = true;
            roiPort.dataType = Selt::DataTypeId::Roi;
            roiPort.required = false;
            roiPort.description = QStringLiteral("可选上游 ROI；未连接时使用参数面板 ROI");
            roiPort.group = QStringLiteral("ROI");
            roiPort.role = Selt::PortRole::Primary;
            roiPort.defaultExposed = false;
            descriptor.ports.append(roiPort);
        }
        Selt::applyBuiltinPortExposureHints(descriptor);
    }

    return list;
}

static QVector<ModuleDescriptor> &mutableDescriptorCache()
{
    static QVector<ModuleDescriptor> cache = buildDescriptors();
    return cache;
}

static const QVector<ModuleDescriptor> &descriptorCache()
{
    return mutableDescriptorCache();
}

static QHash<QString, QString> &pluginIdMap()
{
    static QHash<QString, QString> map;
    return map;
}

static const ModuleDescriptor *findDescriptor(const QString &type)
{
    for (const ModuleDescriptor &d : descriptorCache()) {
        if (d.typeId == type)
            return &d;
    }
    return nullptr;
}

QStringList VisionNodeRegistry::supportedTypes()
{
    QStringList types;
    for (const ModuleDescriptor &d : descriptorCache())
        types.append(d.typeId);
    return types;
}

QStringList VisionNodeRegistry::categories()
{
    QStringList cats;
    for (const ModuleDescriptor &d : descriptorCache()) {
        if (!cats.contains(d.category))
            cats.append(d.category);
    }
    std::sort(cats.begin(), cats.end(), [](const QString &a, const QString &b) {
        const int ra = Selt::WorkflowTemplates::categoryStageRank(a);
        const int rb = Selt::WorkflowTemplates::categoryStageRank(b);
        if (ra != rb)
            return ra < rb;
        return a < b;
    });
    return cats;
}

QStringList VisionNodeRegistry::typesInCategory(const QString &category)
{
    QStringList types;
    for (const ModuleDescriptor &d : descriptorCache()) {
        if (d.category == category)
            types.append(d.typeId);
    }
    return types;
}

QString VisionNodeRegistry::displayName(const QString &type)
{
    if (const ModuleDescriptor *d = findDescriptor(type))
        return d->displayName;
    return type;
}

QString VisionNodeRegistry::category(const QString &type)
{
    if (const ModuleDescriptor *d = findDescriptor(type))
        return d->category;
    return QStringLiteral("视觉");
}

ModuleDescriptor VisionNodeRegistry::descriptor(const QString &type)
{
    if (const ModuleDescriptor *d = findDescriptor(type))
        return *d;
    return {};
}

QVector<ModuleDescriptor> VisionNodeRegistry::allDescriptors()
{
    return descriptorCache();
}

QSizeF VisionNodeRegistry::fixedNodeSize()
{
    return QSizeF(160, 72);
}

QJsonObject VisionNodeRegistry::defaultParameters(const QString &type)
{
    if (const ModuleDescriptor *d = findDescriptor(type))
        return d->defaultParameters();
    return {};
}

bool VisionNodeRegistry::validateParameters(const QString &type, const QJsonObject &params, QString *error)
{
    if (const ModuleDescriptor *d = findDescriptor(type))
        return d->validateParameters(params, error);
    if (error)
        *error = QStringLiteral("未知视觉模块: %1").arg(type);
    return false;
}

bool VisionNodeRegistry::registerExternalDescriptor(const ModuleDescriptor &desc, QString *error)
{
    if (desc.typeId.isEmpty()) {
        if (error)
            *error = QStringLiteral("ModuleDescriptor.typeId 为空");
        return false;
    }
    if (findDescriptor(desc.typeId)) {
        if (error)
            *error = QStringLiteral("节点类型已存在: %1").arg(desc.typeId);
        return false;
    }
    ModuleDescriptor copy = desc;
    if (copy.fixedSize.isEmpty())
        copy.fixedSize = fixedNodeSize();
    if (copy.iconKey.isEmpty())
        copy.iconKey = copy.category.isEmpty() ? QStringLiteral("插件") : copy.category;
    if (copy.helpText.isEmpty())
        copy.helpText = copy.description;
    if (!copy.accentColor.isValid())
        copy.accentColor = Selt::accentColorForCategory(copy.category);
    if (!copy.fillColor.isValid())
        copy.fillColor = Selt::fillColorForCategory(copy.category);
    if (!copy.borderColor.isValid())
        copy.borderColor = copy.accentColor;
    Selt::prepareExternalDescriptor(copy);
    mutableDescriptorCache().append(copy);
    return true;
}

bool VisionNodeRegistry::registerExternalDescriptors(const QVector<ModuleDescriptor> &descriptors,
                                                     QString *error)
{
    for (const ModuleDescriptor &desc : descriptors) {
        if (!registerExternalDescriptor(desc, error))
            return false;
    }
    return true;
}

bool VisionNodeRegistry::isExternalType(const QString &typeId)
{
    return pluginIdMap().contains(typeId);
}

QString VisionNodeRegistry::pluginIdForType(const QString &typeId)
{
    return pluginIdMap().value(typeId);
}

void VisionNodeRegistry::setPluginIdForType(const QString &typeId, const QString &pluginId)
{
    pluginIdMap().insert(typeId, pluginId);
}

NodeModel VisionNodeRegistry::create(const QString &type, const QPointF &position)
{
    const ModuleDescriptor desc = descriptor(supportedTypes().contains(type) ? type : VisionNodeIds::imageLoader());
    NodeModel node;
    node.id = Document::createId(QStringLiteral("node"));
    node.type = desc.typeId;
    node.position = position;
    node.text = desc.displayName;
    node.size = desc.fixedSize;
    node.style.fillColor = desc.fillColor;
    node.style.borderColor = desc.borderColor;
    node.style.textColor = QColor(220, 224, 230);
    node.style.borderWidth = 1.5;
    node.style.cornerRadius = 6.0;
    node.parameters = desc.defaultParameters();

    QList<PortModel> ports;
    for (const ModulePortDef &portDef : desc.ports) {
        PortModel port;
        port.id = portDef.id;
        port.name = portDef.name;
        port.direction = portDef.isInput ? PortDirection::Input : PortDirection::Output;
        port.dataType = Selt::dataTypeIdToString(portDef.dataType);
        port.relativeX = portDef.isInput ? 0.0 : 1.0;
        port.relativeY = 0.5;
        ports.append(port);
    }
    if (ports.isEmpty())
        ports = NodeModel::defaultPortsForType(node.type);
    layoutPortsFromDescriptor(desc, ports);
    node.ports = ports;
    if (node.type == VisionNodeIds::subflow()) {
        const Selt::SubflowInterface iface =
            Selt::subflowInterfaceFromParameters(node.parameters);
        Selt::applySubflowInterfaceToNode(node, iface);
    } else {
        Selt::syncNodePortExposure(node, desc);
        Selt::layoutExposedPorts(desc, node);
    }
    // Grow node height when many ports are exposed so labels remain readable.
    const int visibleCount = qMax(node.exposedPortIds.size(), 2);
    const qreal neededHeight = 48.0 + visibleCount * 18.0;
    if (neededHeight > node.size.height())
        node.size.setHeight(neededHeight);
    return node;
}

bool VisionNodeRegistry::upgradeGraph(QVector<NodeModel> &nodes, QVector<ConnectionModel> &connections)
{
    bool changed = false;

    QHash<QString, NodeModel> nodeById;
    for (const NodeModel &node : nodes)
        nodeById.insert(node.id, node);

    for (NodeModel &node : nodes) {
        if (!VisionNodeIds::isVisionType(node.type))
            continue;
        const ModuleDescriptor desc = descriptor(node.type);
        const QList<PortModel> before = node.ports;
        // Old projects may contain only the legacy "in"/"out" ports. Rebuild
        // built-in vision ports from the descriptor so newly added ports such
        // as template/score/overlay are actually present in the scene.
        if (node.type == VisionNodeIds::subflow()) {
            const Selt::SubflowInterface iface =
                Selt::subflowInterfaceFromParameters(node.parameters);
            Selt::applySubflowInterfaceToNode(node, iface, connections);
        } else if (!desc.ports.isEmpty()) {
            QList<PortModel> rebuilt;
            for (const ModulePortDef &portDef : desc.ports) {
                PortModel port;
                port.id = portDef.id;
                port.name = portDef.name;
                port.direction = portDef.isInput ? PortDirection::Input
                                                 : PortDirection::Output;
                port.dataType = Selt::dataTypeIdToString(portDef.dataType);
                rebuilt.append(port);
            }
            // Keep unknown ports that still have connections (plugin migration).
            for (const PortModel &old : before) {
                bool known = false;
                for (const PortModel &p : rebuilt) {
                    if (p.id == old.id) {
                        known = true;
                        break;
                    }
                }
                if (known)
                    continue;
                bool wired = false;
                for (const ConnectionModel &conn : connections) {
                    if ((conn.sourceNodeId == node.id && conn.sourcePortId == old.id)
                        || (conn.targetNodeId == node.id && conn.targetPortId == old.id)) {
                        wired = true;
                        break;
                    }
                }
                if (wired)
                    rebuilt.append(old);
            }
            node.ports = rebuilt;
        }
        const QList<PortModel> mid = node.ports;
        if (node.type != VisionNodeIds::subflow()) {
            Selt::syncNodePortExposure(node, desc, connections);
            Selt::layoutExposedPorts(desc, node, connections);
        }
        if (portsLayoutChanged(before, node.ports)
            || mid.size() != node.ports.size()
            || before.size() != node.ports.size()) {
            changed = true;
        }
        if (!node.portExposureCustomized && node.exposedPortIds != Selt::defaultExposedPortIds(desc))
            changed = true;
    }

    for (ConnectionModel &conn : connections) {
        const NodeModel target = nodeById.value(conn.targetNodeId);
        if (target.type != VisionNodeIds::templateMatch())
            continue;
        if (conn.targetPortId != QLatin1String("template"))
            continue;
        if (inputPortWired(connections, target.id, QStringLiteral("in")))
            continue;

        const NodeModel source = nodeById.value(conn.sourceNodeId);
        if (source.type == VisionNodeIds::templateSource())
            continue;

        conn.targetPortId = QStringLiteral("in");
        changed = true;
    }

    return changed;
}
