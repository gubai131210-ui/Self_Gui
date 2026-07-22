#include "vision/data/datatype.h"

#include "vision/domain/visionflow.h"
#include "vision/model/moduleparamdef.h"

#include <QJsonArray>

namespace Selt {

QString dataTypeIdToString(DataTypeId id)
{
    switch (id) {
    case DataTypeId::Bool: return QStringLiteral("bool");
    case DataTypeId::Int: return QStringLiteral("int");
    case DataTypeId::Real: return QStringLiteral("real");
    case DataTypeId::String: return QStringLiteral("string");
    case DataTypeId::Image: return QStringLiteral("image");
    case DataTypeId::Point2D: return QStringLiteral("point2d");
    case DataTypeId::Line: return QStringLiteral("line");
    case DataTypeId::Rect: return QStringLiteral("rect");
    case DataTypeId::Circle: return QStringLiteral("circle");
    case DataTypeId::Contour: return QStringLiteral("contour");
    case DataTypeId::Roi: return QStringLiteral("roi");
    case DataTypeId::Region: return QStringLiteral("region");
    case DataTypeId::Measurement: return QStringLiteral("measurement");
    case DataTypeId::Overlay: return QStringLiteral("overlay");
    case DataTypeId::Table: return QStringLiteral("table");
    case DataTypeId::ByteArray: return QStringLiteral("bytearray");
    case DataTypeId::None:
    default: return QStringLiteral("none");
    }
}

QString dataTypeIdDisplayName(DataTypeId id)
{
    switch (id) {
    case DataTypeId::Bool: return QStringLiteral("布尔");
    case DataTypeId::Int: return QStringLiteral("整数");
    case DataTypeId::Real: return QStringLiteral("实数");
    case DataTypeId::String: return QStringLiteral("字符串");
    case DataTypeId::Image: return QStringLiteral("图像");
    case DataTypeId::Point2D: return QStringLiteral("点");
    case DataTypeId::Line: return QStringLiteral("直线");
    case DataTypeId::Rect: return QStringLiteral("矩形");
    case DataTypeId::Circle: return QStringLiteral("圆");
    case DataTypeId::Contour: return QStringLiteral("轮廓");
    case DataTypeId::Roi: return QStringLiteral("ROI");
    case DataTypeId::Region: return QStringLiteral("区域");
    case DataTypeId::Measurement: return QStringLiteral("测量结果");
    case DataTypeId::Overlay: return QStringLiteral("叠加层");
    case DataTypeId::Table: return QStringLiteral("表格");
    case DataTypeId::ByteArray: return QStringLiteral("字节数组");
    case DataTypeId::None:
    default: return QStringLiteral("无");
    }
}

DataTypeId dataTypeIdFromString(const QString &text, bool *ok)
{
    const QString t = text.trimmed().toLower();
    auto setOk = [ok](bool v) {
        if (ok)
            *ok = v;
    };
    if (t == QLatin1String("bool")) { setOk(true); return DataTypeId::Bool; }
    if (t == QLatin1String("int")) { setOk(true); return DataTypeId::Int; }
    if (t == QLatin1String("real") || t == QLatin1String("double") || t == QLatin1String("float")) {
        setOk(true);
        return DataTypeId::Real;
    }
    if (t == QLatin1String("string")) { setOk(true); return DataTypeId::String; }
    if (t == QLatin1String("image")) { setOk(true); return DataTypeId::Image; }
    if (t == QLatin1String("point2d") || t == QLatin1String("point")) { setOk(true); return DataTypeId::Point2D; }
    if (t == QLatin1String("line")) { setOk(true); return DataTypeId::Line; }
    if (t == QLatin1String("rect")) { setOk(true); return DataTypeId::Rect; }
    if (t == QLatin1String("circle")) { setOk(true); return DataTypeId::Circle; }
    if (t == QLatin1String("contour")) { setOk(true); return DataTypeId::Contour; }
    if (t == QLatin1String("roi")) { setOk(true); return DataTypeId::Roi; }
    if (t == QLatin1String("region")) { setOk(true); return DataTypeId::Region; }
    if (t == QLatin1String("measurement")) { setOk(true); return DataTypeId::Measurement; }
    if (t == QLatin1String("overlay")) { setOk(true); return DataTypeId::Overlay; }
    if (t == QLatin1String("table")) { setOk(true); return DataTypeId::Table; }
    if (t == QLatin1String("bytearray") || t == QLatin1String("bytes")) {
        setOk(true);
        return DataTypeId::ByteArray;
    }
    if (t == QLatin1String("none") || t.isEmpty()) { setOk(true); return DataTypeId::None; }
    setOk(false);
    return DataTypeId::None;
}

bool dataTypesCompatible(DataTypeId from, DataTypeId to)
{
    if (from == DataTypeId::None || to == DataTypeId::None)
        return false;
    if (from == to)
        return true;
    // Explicit numeric promotion only.
    if (from == DataTypeId::Int && to == DataTypeId::Real)
        return true;
    // OK/NG 布尔可写入数值端口（线圈 0/1、寄存器门控）。
    if (from == DataTypeId::Bool
        && (to == DataTypeId::Real || to == DataTypeId::Int))
        return true;
    if ((from == DataTypeId::String && to == DataTypeId::ByteArray)
        || (from == DataTypeId::ByteArray && to == DataTypeId::String))
        return true;
    return false;
}

DataTypeId dataTypeIdFromModuleParamType(int moduleParamType)
{
    switch (static_cast<ModuleParamType>(moduleParamType)) {
    case ModuleParamType::Bool: return DataTypeId::Bool;
    case ModuleParamType::Int: return DataTypeId::Int;
    case ModuleParamType::Double: return DataTypeId::Real;
    case ModuleParamType::RoiRect: return DataTypeId::Roi;
    case ModuleParamType::String:
    case ModuleParamType::FilePath:
    case ModuleParamType::Enum:
    case ModuleParamType::Color:
    default:
        return DataTypeId::String;
    }
}

DataValue::DataValue(bool v) : m_type(DataTypeId::Bool), m_variant(v) {}
DataValue::DataValue(int v) : m_type(DataTypeId::Int), m_variant(v) {}
DataValue::DataValue(double v) : m_type(DataTypeId::Real), m_variant(v) {}
DataValue::DataValue(const QString &v) : m_type(DataTypeId::String), m_variant(v) {}
DataValue::DataValue(const QByteArray &v) : m_type(DataTypeId::ByteArray), m_variant(v) {}
DataValue::DataValue(const VisionImage &v) : m_type(DataTypeId::Image), m_image(v) {}
DataValue::DataValue(const RoiRect &v) : m_type(DataTypeId::Roi), m_roi(v) {}
DataValue::DataValue(const MeasurementResult &v) : m_type(DataTypeId::Measurement), m_measurement(v) {}
DataValue::DataValue(const OverlayList &v) : m_type(DataTypeId::Overlay), m_overlays(v) {}
DataValue::DataValue(const QPointF &v) : m_type(DataTypeId::Point2D), m_variant(v) {}
DataValue::DataValue(const Line2D &v) : m_type(DataTypeId::Line), m_line(v) {}
DataValue::DataValue(const Circle2D &v) : m_type(DataTypeId::Circle), m_circle(v) {}
DataValue::DataValue(const ContourList &v) : m_type(DataTypeId::Contour), m_contours(v) {}
DataValue::DataValue(const RegionData &v) : m_type(DataTypeId::Region), m_region(v) {}
DataValue::DataValue(const TableData &v) : m_type(DataTypeId::Table), m_table(v) {}

bool DataValue::toBool(bool def) const
{
    return m_type == DataTypeId::Bool ? m_variant.toBool() : def;
}

int DataValue::toInt(int def) const
{
    if (m_type != DataTypeId::Int && m_type != DataTypeId::Real)
        return def;
    bool ok = false;
    const int value = m_variant.toInt(&ok);
    return ok ? value : def;
}

double DataValue::toReal(double def) const
{
    if (m_type != DataTypeId::Int && m_type != DataTypeId::Real)
        return def;
    bool ok = false;
    const double value = m_variant.toDouble(&ok);
    return ok ? value : def;
}

QString DataValue::toString() const
{
    if (m_type == DataTypeId::ByteArray)
        return QString::fromUtf8(m_variant.toByteArray());
    return m_variant.toString();
}

QByteArray DataValue::toByteArray() const
{
    if (m_type == DataTypeId::ByteArray)
        return m_variant.toByteArray();
    if (m_type == DataTypeId::String)
        return m_variant.toString().toUtf8();
    return {};
}

VisionImage DataValue::toImage() const
{
    return m_image;
}

RoiRect DataValue::toRoi() const
{
    return m_roi;
}

MeasurementResult DataValue::toMeasurement() const
{
    return m_measurement;
}

OverlayList DataValue::toOverlays() const
{
    return m_overlays;
}

QPointF DataValue::toPoint2D(const QPointF &def) const
{
    return m_type == DataTypeId::Point2D ? m_variant.toPointF() : def;
}

Line2D DataValue::toLine() const
{
    return m_line;
}

Circle2D DataValue::toCircle() const
{
    return m_circle;
}

ContourList DataValue::toContours() const
{
    return m_contours;
}

RegionData DataValue::toRegion() const
{
    return m_region;
}

TableData DataValue::toTable() const
{
    return m_table;
}

bool DataValue::isValidFor(DataTypeId expected) const
{
    if (expected == DataTypeId::None)
        return !isNull();
    if (m_type == expected)
        return true;
    return dataTypesCompatible(m_type, expected);
}

bool DataValue::convertTo(DataTypeId target, DataValue *out, QString *error) const
{
    if (!out) {
        if (error)
            *error = QStringLiteral("转换输出指针为空");
        return false;
    }
    if (isNull()) {
        if (error)
            *error = QStringLiteral("空值无法转换为 %1").arg(dataTypeIdDisplayName(target));
        return false;
    }
    if (m_type == target) {
        *out = *this;
        return true;
    }
    if (m_type == DataTypeId::Int && target == DataTypeId::Real) {
        *out = DataValue(static_cast<double>(toInt()));
        return true;
    }
    if (m_type == DataTypeId::Bool && target == DataTypeId::Real) {
        *out = DataValue(toBool() ? 1.0 : 0.0);
        return true;
    }
    if (m_type == DataTypeId::Bool && target == DataTypeId::Int) {
        *out = DataValue(toBool() ? 1 : 0);
        return true;
    }
    if (m_type == DataTypeId::String && target == DataTypeId::ByteArray) {
        *out = DataValue(toString().toUtf8());
        return true;
    }
    if (m_type == DataTypeId::ByteArray && target == DataTypeId::String) {
        *out = DataValue(QString::fromUtf8(toByteArray()));
        return true;
    }
    if (error) {
        *error = QStringLiteral("不支持从 %1 转换为 %2")
                     .arg(dataTypeIdDisplayName(m_type), dataTypeIdDisplayName(target));
    }
    return false;
}

QString DataValue::debugString() const
{
    switch (m_type) {
    case DataTypeId::None:
        return QStringLiteral("None");
    case DataTypeId::Bool:
        return QStringLiteral("Bool(%1)")
            .arg(toBool() ? QStringLiteral("true") : QStringLiteral("false"));
    case DataTypeId::Int:
        return QStringLiteral("Int(%1)").arg(toInt());
    case DataTypeId::Real:
        return QStringLiteral("Real(%1)").arg(toReal(), 0, 'f', 3);
    case DataTypeId::String:
        return QStringLiteral("String(%1)").arg(toString());
    case DataTypeId::ByteArray:
        return QStringLiteral("ByteArray(len=%1)").arg(toByteArray().size());
    case DataTypeId::Image:
        return QStringLiteral("Image(%1x%2, ch=%3, src=%4)")
            .arg(m_image.isEmpty() ? 0 : m_image.width())
            .arg(m_image.isEmpty() ? 0 : m_image.height())
            .arg(m_image.isEmpty() ? 0 : m_image.channels())
            .arg(m_image.sourcePath().isEmpty() ? QStringLiteral("-")
                                               : m_image.sourcePath());
    case DataTypeId::Roi:
        return QStringLiteral("Roi(enabled=%1)")
            .arg(m_roi.enabled ? QStringLiteral("true") : QStringLiteral("false"));
    case DataTypeId::Measurement:
        return QStringLiteral(
                   "Measurement(valid=%1,w=%2,h=%3,area=%4,angle=%5,unit=%6,conf=%7,"
                   "decision=%8)")
            .arg(m_measurement.valid ? QStringLiteral("true") : QStringLiteral("false"))
            .arg(m_measurement.width, 0, 'f', 1)
            .arg(m_measurement.height, 0, 'f', 1)
            .arg(m_measurement.area, 0, 'f', 1)
            .arg(m_measurement.angle, 0, 'f', 1)
            .arg(m_measurement.unit)
            .arg(m_measurement.confidence, 0, 'f', 2)
            .arg(m_measurement.decisionState);
    case DataTypeId::Overlay:
        return QStringLiteral("Overlay(count=%1)").arg(m_overlays.size());
    case DataTypeId::Point2D: {
        const QPointF p = toPoint2D();
        return QStringLiteral("Point2D(%1,%2)").arg(p.x()).arg(p.y());
    }
    case DataTypeId::Line:
        return QStringLiteral("Line((%1,%2)->(%3,%4))")
            .arg(m_line.start.x())
            .arg(m_line.start.y())
            .arg(m_line.end.x())
            .arg(m_line.end.y());
    case DataTypeId::Circle:
        return QStringLiteral("Circle(c=%1,%2 r=%3)")
            .arg(m_circle.center.x())
            .arg(m_circle.center.y())
            .arg(m_circle.radius);
    case DataTypeId::Contour:
        return QStringLiteral("Contour(count=%1)").arg(m_contours.contours.size());
    case DataTypeId::Region:
        return QStringLiteral("Region(labels=%1)").arg(m_region.labelCount);
    case DataTypeId::Table:
        return QStringLiteral("Table(cols=%1,rows=%2)")
            .arg(m_table.columns.size())
            .arg(m_table.rows.size());
    default:
        return QStringLiteral("%1").arg(dataTypeIdToString(m_type));
    }
}

QJsonObject DataValue::toJson() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("type"), dataTypeIdToString(m_type));
    switch (m_type) {
    case DataTypeId::Bool:
        obj.insert(QStringLiteral("value"), toBool());
        break;
    case DataTypeId::Int:
        obj.insert(QStringLiteral("value"), toInt());
        break;
    case DataTypeId::Real:
        obj.insert(QStringLiteral("value"), toReal());
        break;
    case DataTypeId::String:
        obj.insert(QStringLiteral("value"), toString());
        break;
    case DataTypeId::ByteArray:
        obj.insert(QStringLiteral("value"), QString::fromLatin1(toByteArray().toHex()));
        break;
    case DataTypeId::Image: {
        QJsonObject image;
        image.insert(QStringLiteral("empty"), m_image.isEmpty());
        image.insert(QStringLiteral("sourcePath"), m_image.sourcePath());
        image.insert(QStringLiteral("width"), m_image.isEmpty() ? 0 : m_image.width());
        image.insert(QStringLiteral("height"), m_image.isEmpty() ? 0 : m_image.height());
        obj.insert(QStringLiteral("value"), image);
        break;
    }
    case DataTypeId::Roi:
        obj.insert(QStringLiteral("value"), m_roi.toJson());
        break;
    case DataTypeId::Measurement:
        obj.insert(QStringLiteral("value"), m_measurement.toJson());
        break;
    case DataTypeId::Overlay: {
        QJsonArray arr;
        for (const OverlayItem &item : m_overlays)
            arr.append(item.toJson());
        obj.insert(QStringLiteral("value"), arr);
        break;
    }
    case DataTypeId::Point2D: {
        const QPointF p = toPoint2D();
        obj.insert(QStringLiteral("value"),
                   QJsonObject{{QStringLiteral("x"), p.x()}, {QStringLiteral("y"), p.y()}});
        break;
    }
    case DataTypeId::Line:
        obj.insert(QStringLiteral("value"),
                   QJsonObject{{QStringLiteral("x1"), m_line.start.x()},
                               {QStringLiteral("y1"), m_line.start.y()},
                               {QStringLiteral("x2"), m_line.end.x()},
                               {QStringLiteral("y2"), m_line.end.y()}});
        break;
    case DataTypeId::Circle:
        obj.insert(QStringLiteral("value"),
                   QJsonObject{{QStringLiteral("x"), m_circle.center.x()},
                               {QStringLiteral("y"), m_circle.center.y()},
                               {QStringLiteral("radius"), m_circle.radius}});
        break;
    case DataTypeId::Contour:
        obj.insert(QStringLiteral("value"), m_contours.toJson());
        break;
    case DataTypeId::Region:
        obj.insert(QStringLiteral("value"), m_region.toJson());
        break;
    case DataTypeId::Table:
        obj.insert(QStringLiteral("value"), m_table.toJson());
        break;
    case DataTypeId::None:
    default:
        obj.insert(QStringLiteral("value"), QJsonValue());
        break;
    }
    return obj;
}

DataValue DataValue::fromJson(const QJsonObject &obj, QString *error)
{
    bool ok = false;
    const DataTypeId type = dataTypeIdFromString(obj.value(QStringLiteral("type")).toString(), &ok);
    if (!ok) {
        if (error)
            *error = QStringLiteral("未知 DataValue 类型: %1").arg(obj.value(QStringLiteral("type")).toString());
        return {};
    }
    const QJsonValue value = obj.value(QStringLiteral("value"));
    switch (type) {
    case DataTypeId::None:
        return {};
    case DataTypeId::Bool:
        return DataValue(value.toBool());
    case DataTypeId::Int:
        return DataValue(value.toInt());
    case DataTypeId::Real:
        return DataValue(value.toDouble());
    case DataTypeId::String:
        return DataValue(value.toString());
    case DataTypeId::ByteArray:
        return DataValue(QByteArray::fromHex(value.toString().toLatin1()));
    case DataTypeId::Image: {
        // Pixel data is runtime-only; JSON only restores an empty placeholder with metadata.
        VisionImage image;
        Q_UNUSED(value);
        return DataValue(image);
    }
    case DataTypeId::Roi:
        return DataValue(RoiRect::fromJson(value.toObject()));
    case DataTypeId::Measurement:
        return DataValue(MeasurementResult::fromJson(value.toObject()));
    case DataTypeId::Overlay: {
        OverlayList list;
        for (const QJsonValue &v : value.toArray())
            list.append(OverlayItem::fromJson(v.toObject()));
        return DataValue(list);
    }
    case DataTypeId::Point2D: {
        const QJsonObject p = value.toObject();
        return DataValue(QPointF(p.value(QStringLiteral("x")).toDouble(),
                                 p.value(QStringLiteral("y")).toDouble()));
    }
    case DataTypeId::Line: {
        const QJsonObject p = value.toObject();
        Line2D line;
        line.start = QPointF(p.value(QStringLiteral("x1")).toDouble(), p.value(QStringLiteral("y1")).toDouble());
        line.end = QPointF(p.value(QStringLiteral("x2")).toDouble(), p.value(QStringLiteral("y2")).toDouble());
        return DataValue(line);
    }
    case DataTypeId::Circle: {
        const QJsonObject p = value.toObject();
        Circle2D c;
        c.center = QPointF(p.value(QStringLiteral("x")).toDouble(), p.value(QStringLiteral("y")).toDouble());
        c.radius = p.value(QStringLiteral("radius")).toDouble();
        return DataValue(c);
    }
    case DataTypeId::Contour:
        return DataValue(ContourList::fromJson(value.toObject()));
    case DataTypeId::Region:
        return DataValue(RegionData::fromJson(value.toObject()));
    case DataTypeId::Table:
        return DataValue(TableData::fromJson(value.toObject()));
    default:
        if (error)
            *error = QStringLiteral("类型 %1 暂不支持反序列化").arg(dataTypeIdToString(type));
        return {};
    }
}

DataValue DataValue::fromParameterJson(const QJsonValue &value, DataTypeId expected, QString *error)
{
    switch (expected) {
    case DataTypeId::Bool:
        return DataValue(value.toBool());
    case DataTypeId::Int:
        return DataValue(value.toInt());
    case DataTypeId::Real:
        return DataValue(value.toDouble());
    case DataTypeId::String:
        return DataValue(value.toString());
    case DataTypeId::Roi:
        return DataValue(RoiRect::fromJson(value.toObject()));
    case DataTypeId::Measurement:
        return DataValue(MeasurementResult::fromJson(value.toObject()));
    case DataTypeId::None:
        if (error)
            *error = QStringLiteral("目标类型为空");
        return {};
    default:
        if (error)
            *error = QStringLiteral("参数类型 %1 无法从 JSON 构造").arg(dataTypeIdDisplayName(expected));
        return {};
    }
}

QJsonValue DataValue::toParameterJson() const
{
    switch (m_type) {
    case DataTypeId::Bool:
        return toBool();
    case DataTypeId::Int:
        return toInt();
    case DataTypeId::Real:
        return toReal();
    case DataTypeId::String:
        return toString();
    case DataTypeId::Roi:
        return m_roi.toJson();
    case DataTypeId::Measurement:
        return m_measurement.toJson();
    default:
        return QJsonValue();
    }
}

QString parameterSourceKindToString(ParameterSourceKind kind)
{
    switch (kind) {
    case ParameterSourceKind::UpstreamBinding:
        return QStringLiteral("upstream");
    case ParameterSourceKind::ProjectVariable:
        return QStringLiteral("projectVariable");
    case ParameterSourceKind::Constant:
    default:
        return QStringLiteral("constant");
    }
}

ParameterSourceKind parameterSourceKindFromString(const QString &text, bool *ok)
{
    const QString t = text.trimmed().toLower();
    auto setOk = [ok](bool v) {
        if (ok)
            *ok = v;
    };
    if (t == QLatin1String("constant") || t == QLatin1String("0")) {
        setOk(true);
        return ParameterSourceKind::Constant;
    }
    if (t == QLatin1String("upstream") || t == QLatin1String("upstreambinding") || t == QLatin1String("1")) {
        setOk(true);
        return ParameterSourceKind::UpstreamBinding;
    }
    if (t == QLatin1String("projectvariable") || t == QLatin1String("variable") || t == QLatin1String("2")) {
        setOk(true);
        return ParameterSourceKind::ProjectVariable;
    }
    setOk(false);
    return ParameterSourceKind::Constant;
}

ParameterBinding ParameterBinding::constant(const DataValue &value, DataTypeId targetType)
{
    ParameterBinding b;
    b.kind = ParameterSourceKind::Constant;
    b.targetType = targetType == DataTypeId::None ? value.type() : targetType;
    b.constantValue = value.toJson();
    return b;
}

ParameterBinding ParameterBinding::constantFromJson(const QJsonValue &value, DataTypeId targetType)
{
    ParameterBinding b;
    b.kind = ParameterSourceKind::Constant;
    b.targetType = targetType;
    if (value.isObject()) {
        b.constantValue = value.toObject();
        if (!b.constantValue.contains(QStringLiteral("type"))) {
            DataValue dv = DataValue::fromParameterJson(value, targetType);
            b.constantValue = dv.toJson();
        }
    } else {
        DataValue dv = DataValue::fromParameterJson(value, targetType);
        b.constantValue = dv.toJson();
    }
    return b;
}

ParameterBinding ParameterBinding::upstream(const QString &nodeId, const QString &portId, DataTypeId targetType)
{
    ParameterBinding b;
    b.kind = ParameterSourceKind::UpstreamBinding;
    b.upstreamNodeId = nodeId;
    b.upstreamPortId = portId;
    b.targetType = targetType;
    return b;
}

ParameterBinding ParameterBinding::projectVariable(const QString &variableId, DataTypeId targetType)
{
    ParameterBinding b;
    b.kind = ParameterSourceKind::ProjectVariable;
    b.projectVariableId = variableId;
    b.targetType = targetType;
    return b;
}

bool ParameterBinding::isValid() const
{
    switch (kind) {
    case ParameterSourceKind::Constant:
        return !constantValue.isEmpty() || targetType != DataTypeId::None;
    case ParameterSourceKind::UpstreamBinding:
        return !upstreamNodeId.isEmpty() && !upstreamPortId.isEmpty();
    case ParameterSourceKind::ProjectVariable:
        return !projectVariableId.isEmpty();
    }
    return false;
}

bool ParameterBinding::validateAgainst(const VisionFlow &flow, const ModuleParamDef &param, QString *error) const
{
    const DataTypeId expected = targetType != DataTypeId::None
        ? targetType
        : dataTypeIdFromModuleParamType(static_cast<int>(param.type));

    switch (kind) {
    case ParameterSourceKind::Constant: {
        QString err;
        const DataValue value = constantValue.contains(QStringLiteral("type"))
            ? DataValue::fromJson(constantValue, &err)
            : DataValue::fromParameterJson(constantValue, expected, &err);
        if (value.isNull() && !err.isEmpty()) {
            if (error)
                *error = QStringLiteral("参数 %1 常量无效: %2").arg(param.label, err);
            return false;
        }
        if (!value.isNull() && !value.isValidFor(expected) && !dataTypesCompatible(value.type(), expected)) {
            if (error)
                *error = QStringLiteral("参数 %1 常量类型不兼容目标 %2")
                             .arg(param.label, dataTypeIdDisplayName(expected));
            return false;
        }
        return param.validate(value.isNull() ? QJsonValue(constantValue) : value.toParameterJson(), error);
    }
    case ParameterSourceKind::UpstreamBinding: {
        if (upstreamNodeId.isEmpty() || upstreamPortId.isEmpty()) {
            if (error)
                *error = QStringLiteral("参数 %1 上游绑定缺少节点或端口").arg(param.label);
            return false;
        }
        if (!flow.hasNode(upstreamNodeId)) {
            if (error)
                *error = QStringLiteral("参数 %1 上游节点不存在: %2").arg(param.label, upstreamNodeId);
            return false;
        }
        const NodeModel up = flow.node(upstreamNodeId);
        bool found = false;
        for (const PortModel &port : up.ports) {
            if (port.id == upstreamPortId) {
                found = true;
                break;
            }
        }
        if (!found) {
            if (error)
                *error = QStringLiteral("参数 %1 上游端口不存在: %2.%3")
                             .arg(param.label, upstreamNodeId, upstreamPortId);
            return false;
        }
        return true;
    }
    case ParameterSourceKind::ProjectVariable:
        if (projectVariableId.isEmpty()) {
            if (error)
                *error = QStringLiteral("参数 %1 未选择项目变量").arg(param.label);
            return false;
        }
        return true;
    }
    return false;
}

QJsonObject ParameterBinding::toJson() const
{
    return QJsonObject{
        {QStringLiteral("version"), version},
        {QStringLiteral("kind"), parameterSourceKindToString(kind)},
        {QStringLiteral("targetType"), dataTypeIdToString(targetType)},
        {QStringLiteral("upstreamNodeId"), upstreamNodeId},
        {QStringLiteral("upstreamPortId"), upstreamPortId},
        {QStringLiteral("projectVariableId"), projectVariableId},
        {QStringLiteral("constantValue"), constantValue}};
}

ParameterBinding ParameterBinding::fromJson(const QJsonObject &obj, QString *error)
{
    ParameterBinding b;
    b.version = obj.value(QStringLiteral("version")).toInt(1);

    const QJsonValue kindValue = obj.value(QStringLiteral("kind"));
    bool kindOk = false;
    if (kindValue.isString()) {
        b.kind = parameterSourceKindFromString(kindValue.toString(), &kindOk);
    } else if (kindValue.isDouble()) {
        const int k = kindValue.toInt();
        if (k >= 0 && k <= 2) {
            b.kind = static_cast<ParameterSourceKind>(k);
            kindOk = true;
        }
    }
    if (!kindOk && obj.contains(QStringLiteral("kind"))) {
        if (error)
            *error = QStringLiteral("未知绑定 kind");
        return {};
    }

    bool typeOk = true;
    if (obj.contains(QStringLiteral("targetType")))
        b.targetType = dataTypeIdFromString(obj.value(QStringLiteral("targetType")).toString(), &typeOk);
    if (!typeOk) {
        if (error)
            *error = QStringLiteral("未知绑定目标类型: %1").arg(obj.value(QStringLiteral("targetType")).toString());
        return {};
    }

    b.upstreamNodeId = obj.value(QStringLiteral("upstreamNodeId")).toString();
    b.upstreamPortId = obj.value(QStringLiteral("upstreamPortId")).toString();
    b.projectVariableId = obj.value(QStringLiteral("projectVariableId")).toString();
    b.constantValue = obj.value(QStringLiteral("constantValue")).toObject();
    return b;
}

ParameterBinding ParameterBinding::legacyConstantBinding(const QJsonValue &parameterValue, DataTypeId targetType)
{
    return constantFromJson(parameterValue, targetType);
}

} // namespace Selt
