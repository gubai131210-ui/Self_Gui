#include "visionnoderegistry.h"

#include "core/model/document.h"
#include "vision/registry/visionnodeids.h"

#include <QHash>

static ModuleParamDef makeIntParam(const QString &key, const QString &label, int def,
                                   int minV, int maxV, int order)
{
    ModuleParamDef p;
    p.key = key;
    p.label = label;
    p.type = ModuleParamType::Int;
    p.defaultValue = def;
    p.minimum = minV;
    p.maximum = maxV;
    p.displayOrder = order;
    return p;
}

static ModuleParamDef makeDoubleParam(const QString &key, const QString &label, double def,
                                      double minV, double maxV, int order)
{
    ModuleParamDef p;
    p.key = key;
    p.label = label;
    p.type = ModuleParamType::Double;
    p.defaultValue = def;
    p.minimum = minV;
    p.maximum = maxV;
    p.displayOrder = order;
    return p;
}

static ModuleParamDef makeBoolParam(const QString &key, const QString &label, bool def, int order)
{
    ModuleParamDef p;
    p.key = key;
    p.label = label;
    p.type = ModuleParamType::Bool;
    p.defaultValue = def;
    p.displayOrder = order;
    return p;
}

static ModuleParamDef makeFileParam(const QString &key, const QString &label, int order)
{
    ModuleParamDef p;
    p.key = key;
    p.label = label;
    p.type = ModuleParamType::FilePath;
    p.defaultValue = QString();
    p.fileFilter = QStringLiteral("图片 (*.png *.jpg *.jpeg *.bmp *.tif *.tiff);;所有文件 (*.*)");
    p.displayOrder = order;
    return p;
}

static ModuleParamDef makeEnumParam(const QString &key, const QString &label,
                                    const QStringList &values,
                                    const QStringList &labels,
                                    const QString &def, int order)
{
    ModuleParamDef p;
    p.key = key;
    p.label = label;
    p.type = ModuleParamType::Enum;
    p.enumValues = values;
    p.defaultValue = def;
    p.displayOrder = order;
    for (int i = 0; i < values.size(); ++i) {
        ModuleParamOption opt;
        opt.value = values.at(i);
        opt.label = (i < labels.size()) ? labels.at(i) : values.at(i);
        p.enumOptions.append(opt);
    }
    return p;
}

static ModuleParamDef makeRoiParam(const QString &key, const QString &label, int order)
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
    p.tooltip = QStringLiteral("矩形 ROI，可在图像预览中绘制");
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
    d.fixedSize = VisionNodeRegistry::fixedNodeSize();
    d.fillColor = QColor(235, 248, 240);
    d.borderColor = QColor(40, 140, 90);
    d.ports = {
        {QStringLiteral("in"), QStringLiteral("输入"), true},
        {QStringLiteral("out"), QStringLiteral("输出"), false}};
    return d;
}

static QVector<ModuleDescriptor> buildDescriptors()
{
    QVector<ModuleDescriptor> list;

    {
        ModuleDescriptor d = makeBase(VisionNodeIds::imageLoader(), QStringLiteral("图片输入"),
                                      QStringLiteral("输入"), QStringLiteral("从本地文件加载图像"));
        d.ports = {{QStringLiteral("out"), QStringLiteral("图像"), false}};
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
            makeIntParam(QStringLiteral("threshold"), QStringLiteral("阈值"), 128, 0, 255, 0),
            makeIntParam(QStringLiteral("maxValue"), QStringLiteral("最大值"), 255, 0, 255, 1),
            makeEnumParam(QStringLiteral("mode"), QStringLiteral("阈值模式"),
                          {QStringLiteral("binary"), QStringLiteral("binaryInv")},
                          {QStringLiteral("Binary"), QStringLiteral("BinaryInv")},
                          QStringLiteral("binary"), 2),
            makeRoiParam(QStringLiteral("roi"), QStringLiteral("ROI"), 3)};
        d.inputKeys = {QStringLiteral("image")};
        d.outputKeys = {QStringLiteral("image")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::rectangleMeasure(), QStringLiteral("矩形测量"),
                                      QStringLiteral("测量"), QStringLiteral("检测最大轮廓矩形并测量"));
        d.parameters = {
            makeDoubleParam(QStringLiteral("minArea"), QStringLiteral("最小面积"), 100.0, 0.0, 1e9, 0),
            makeBoolParam(QStringLiteral("drawOverlay"), QStringLiteral("绘制测量结果"), true, 1),
            makeRoiParam(QStringLiteral("roi"), QStringLiteral("ROI"), 2)};
        d.inputKeys = {QStringLiteral("image")};
        d.outputKeys = {QStringLiteral("image"), QStringLiteral("measurement")};
        list.append(d);
    }
    {
        ModuleDescriptor d = makeBase(VisionNodeIds::resultPreview(), QStringLiteral("结果预览"),
                                      QStringLiteral("输出"), QStringLiteral("显示上游处理结果"));
        d.ports = {{QStringLiteral("in"), QStringLiteral("图像"), true}};
        d.inputKeys = {QStringLiteral("image")};
        list.append(d);
    }
    return list;
}

static const QVector<ModuleDescriptor> &descriptorCache()
{
    static const QVector<ModuleDescriptor> cache = buildDescriptors();
    return cache;
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
    node.style.cornerRadius = 10.0;
    node.parameters = desc.defaultParameters();

    QList<PortModel> ports;
    for (const ModulePortDef &portDef : desc.ports) {
        PortModel port;
        port.id = portDef.id;
        port.name = portDef.name;
        port.direction = portDef.isInput ? PortDirection::Input : PortDirection::Output;
        port.relativeX = portDef.isInput ? 0.0 : 1.0;
        port.relativeY = 0.5;
        ports.append(port);
    }
    if (ports.isEmpty())
        ports = NodeModel::defaultPortsForType(node.type);
    node.ports = ports;
    return node;
}
