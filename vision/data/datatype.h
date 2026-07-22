#ifndef DATATYPE_H
#define DATATYPE_H

#include "vision/model/measurementresult.h"
#include "vision/model/overlayitems.h"
#include "vision/model/roi.h"
#include "vision/model/visionimage.h"
#include "vision/model/contourdata.h"

#include <QByteArray>
#include <QJsonObject>
#include <QPointF>
#include <QString>
#include <QVariant>
#include <QVector>

class VisionFlow;
struct ModuleParamDef;

namespace Selt {

enum class DataTypeId {
    None,
    Bool,
    Int,
    Real,
    String,
    Image,
    Point2D,
    Line,
    Rect,
    Circle,
    Contour,
    Roi,
    Region,
    Measurement,
    Overlay,
    Table,
    ByteArray
};

QString dataTypeIdToString(DataTypeId id);
QString dataTypeIdDisplayName(DataTypeId id);
DataTypeId dataTypeIdFromString(const QString &text, bool *ok = nullptr);

/// Compatibility for wiring and binding: exact match, or Int → Real.
bool dataTypesCompatible(DataTypeId from, DataTypeId to);

struct Line2D {
    QPointF start;
    QPointF end;
};

struct Circle2D {
    QPointF center;
    double radius{0.0};
};

class DataValue
{
public:
    DataValue() = default;
    explicit DataValue(bool v);
    explicit DataValue(int v);
    explicit DataValue(double v);
    explicit DataValue(const QString &v);
    explicit DataValue(const QByteArray &v);
    explicit DataValue(const VisionImage &v);
    explicit DataValue(const RoiRect &v);
    explicit DataValue(const MeasurementResult &v);
    explicit DataValue(const OverlayList &v);
    explicit DataValue(const QPointF &v);
    explicit DataValue(const Line2D &v);
    explicit DataValue(const Circle2D &v);
    explicit DataValue(const ContourList &v);
    explicit DataValue(const RegionData &v);
    explicit DataValue(const TableData &v);

    DataTypeId type() const { return m_type; }
    bool isNull() const { return m_type == DataTypeId::None; }

    bool toBool(bool def = false) const;
    int toInt(int def = 0) const;
    double toReal(double def = 0.0) const;
    QString toString() const;
    QByteArray toByteArray() const;
    VisionImage toImage() const;
    RoiRect toRoi() const;
    MeasurementResult toMeasurement() const;
    OverlayList toOverlays() const;
    QPointF toPoint2D(const QPointF &def = {}) const;
    Line2D toLine() const;
    Circle2D toCircle() const;
    ContourList toContours() const;
    RegionData toRegion() const;
    TableData toTable() const;

    bool isValidFor(DataTypeId expected) const;
    bool convertTo(DataTypeId target, DataValue *out, QString *error = nullptr) const;
    QString debugString() const;
    /// 结果面板用的可读摘要（不含 Type(...) 包装），复杂类型给短摘要。
    QString displaySummary() const;
    /// 详情对话框 / 复制整值用的完整文本。
    QString displayDetail() const;

    QJsonObject toJson() const;
    static DataValue fromJson(const QJsonObject &obj, QString *error = nullptr);

    /// Build from a raw JSON parameter value using the expected type.
    static DataValue fromParameterJson(const QJsonValue &value, DataTypeId expected, QString *error = nullptr);
    QJsonValue toParameterJson() const;

private:
    DataTypeId m_type{DataTypeId::None};
    QVariant m_variant;
    VisionImage m_image;
    RoiRect m_roi;
    MeasurementResult m_measurement;
    OverlayList m_overlays;
    Line2D m_line;
    Circle2D m_circle;
    ContourList m_contours;
    RegionData m_region;
    TableData m_table;
};

enum class ParameterSourceKind {
    Constant,
    UpstreamBinding,
    ProjectVariable
};

QString parameterSourceKindToString(ParameterSourceKind kind);
ParameterSourceKind parameterSourceKindFromString(const QString &text, bool *ok = nullptr);

struct ParameterBinding
{
    int version{1};
    ParameterSourceKind kind{ParameterSourceKind::Constant};
    DataTypeId targetType{DataTypeId::None};
    QString upstreamNodeId;
    QString upstreamPortId;
    QString projectVariableId;
    QJsonObject constantValue;

    static ParameterBinding constant(const DataValue &value, DataTypeId targetType = DataTypeId::None);
    static ParameterBinding constantFromJson(const QJsonValue &value, DataTypeId targetType);
    static ParameterBinding upstream(const QString &nodeId, const QString &portId,
                                     DataTypeId targetType = DataTypeId::None);
    static ParameterBinding projectVariable(const QString &variableId,
                                            DataTypeId targetType = DataTypeId::None);

    bool isValid() const;
    bool validateAgainst(const VisionFlow &flow, const ModuleParamDef &param, QString *error = nullptr) const;

    QJsonObject toJson() const;
    static ParameterBinding fromJson(const QJsonObject &obj, QString *error = nullptr);

    /// Treat a plain NodeModel.parameters entry as a Constant binding.
    static ParameterBinding legacyConstantBinding(const QJsonValue &parameterValue, DataTypeId targetType);
};

struct TypedPortDef
{
    QString id;
    QString name;
    bool isInput{true};
    DataTypeId dataType{DataTypeId::Image};
    bool required{true};
    QString description;
};

DataTypeId dataTypeIdFromModuleParamType(int moduleParamType);

} // namespace Selt

#endif // DATATYPE_H
