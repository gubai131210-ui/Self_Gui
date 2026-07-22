#ifndef PROJECTVARIABLES_H
#define PROJECTVARIABLES_H

#include "vision/data/datatype.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

class VisionFlow;
class VisionProject;

namespace Selt {

struct ProjectVariableRecord
{
    QString id;
    QString name;
    DataTypeId dataType{DataTypeId::Real};
    DataValue value;
    DataValue defaultValue;
    QString description;

    QJsonObject toJson() const;
    static ProjectVariableRecord fromJson(const QJsonObject &obj, QString *error = nullptr);
};

class ProjectVariableStore
{
public:
    QString add(const QString &name, DataTypeId type, const DataValue &value = {},
                const QString &description = {});
    bool remove(const QString &id);
    bool rename(const QString &id, const QString &name);
    bool setType(const QString &id, DataTypeId type, QString *error = nullptr);
    bool setValue(const QString &id, const DataValue &value, QString *error = nullptr);
    bool setDescription(const QString &id, const QString &description);

    bool contains(const QString &id) const;
    ProjectVariableRecord record(const QString &id) const;
    DataValue get(const QString &id) const;
    QStringList ids() const { return m_order; }
    QStringList keys() const { return m_order; } // ordered
    QVector<ProjectVariableRecord> records() const;

    void clear();
    QJsonArray toJson() const;
    bool fromJson(const QJsonArray &arr, QString *error = nullptr);

    /// Scan flows for ProjectVariable bindings referencing id.
    static QStringList findReferences(const VisionProject &project, const QString &variableId);

    // Backward-compatible helpers used by older call sites.
    void set(const QString &key, const DataValue &value);

private:
    QHash<QString, ProjectVariableRecord> m_values;
    QStringList m_order;
};

struct SubflowPortMapping
{
    QString outerPortId;
    QString innerNodeId;
    QString innerPortId;
};

struct SubflowInterface
{
    /// Version of the serialized interface contract. Missing legacy values read as 1.
    int schemaVersion{1};
    QString flowId;
    QString displayName;
    QString description;
    QVector<TypedPortDef> inputs;
    QVector<TypedPortDef> outputs;
    QVector<SubflowPortMapping> inputMappings;
    QVector<SubflowPortMapping> outputMappings;

    /// Bindings may only reference ports within the same project; never across projects.
    bool validateScope(QString *error = nullptr) const
    {
        if (schemaVersion < 1) {
            if (error)
                *error = QStringLiteral("子流程版本无效");
            return false;
        }
        if (flowId.isEmpty()) {
            if (error)
                *error = QStringLiteral("子流程缺少 flowId");
            return false;
        }
        auto validatePorts = [error](const QVector<TypedPortDef> &ports, bool input) {
            QSet<QString> ids;
            for (const TypedPortDef &port : ports) {
                if (port.id.isEmpty() || ids.contains(port.id) || port.isInput != input
                    || port.dataType == DataTypeId::None) {
                    if (error)
                        *error = QStringLiteral("子流程接口端口无效或重复");
                    return false;
                }
                ids.insert(port.id);
            }
            return true;
        };
        if (!validatePorts(inputs, true) || !validatePorts(outputs, false))
            return false;
        auto hasPort = [](const QVector<TypedPortDef> &ports, const QString &id) {
            for (const TypedPortDef &port : ports) {
                if (port.id == id)
                    return true;
            }
            return false;
        };
        for (const SubflowPortMapping &m : inputMappings) {
            if (m.outerPortId.isEmpty() || m.innerNodeId.isEmpty() || m.innerPortId.isEmpty()) {
                if (error)
                    *error = QStringLiteral("子流程输入映射不完整");
                return false;
            }
            if (!hasPort(inputs, m.outerPortId)) {
                if (error)
                    *error = QStringLiteral("子流程输入映射引用未知接口端口");
                return false;
            }
        }
        for (const SubflowPortMapping &m : outputMappings) {
            if (m.outerPortId.isEmpty() || m.innerNodeId.isEmpty() || m.innerPortId.isEmpty()) {
                if (error)
                    *error = QStringLiteral("子流程输出映射不完整");
                return false;
            }
            if (!hasPort(outputs, m.outerPortId)) {
                if (error)
                    *error = QStringLiteral("子流程输出映射引用未知接口端口");
                return false;
            }
        }
        return true;
    }

    QJsonObject toJson() const
    {
        QJsonObject obj{{QStringLiteral("schemaVersion"), schemaVersion},
                        {QStringLiteral("flowId"), flowId},
                        {QStringLiteral("displayName"), displayName},
                        {QStringLiteral("description"), description}};
        QJsonArray inPorts;
        for (const TypedPortDef &p : inputs) {
            inPorts.append(QJsonObject{{QStringLiteral("id"), p.id},
                                       {QStringLiteral("name"), p.name},
                                       {QStringLiteral("isInput"), p.isInput},
                                       {QStringLiteral("dataType"), dataTypeIdToString(p.dataType)},
                                       {QStringLiteral("required"), p.required}});
        }
        QJsonArray outPorts;
        for (const TypedPortDef &p : outputs) {
            outPorts.append(QJsonObject{{QStringLiteral("id"), p.id},
                                        {QStringLiteral("name"), p.name},
                                        {QStringLiteral("isInput"), p.isInput},
                                        {QStringLiteral("dataType"), dataTypeIdToString(p.dataType)},
                                        {QStringLiteral("required"), p.required}});
        }
        obj.insert(QStringLiteral("inputs"), inPorts);
        obj.insert(QStringLiteral("outputs"), outPorts);
        auto mappingsToJson = [](const QVector<SubflowPortMapping> &mappings) {
            QJsonArray array;
            for (const SubflowPortMapping &mapping : mappings) {
                array.append(QJsonObject{
                    {QStringLiteral("outerPortId"), mapping.outerPortId},
                    {QStringLiteral("innerNodeId"), mapping.innerNodeId},
                    {QStringLiteral("innerPortId"), mapping.innerPortId}});
            }
            return array;
        };
        obj.insert(QStringLiteral("inputMappings"), mappingsToJson(inputMappings));
        obj.insert(QStringLiteral("outputMappings"), mappingsToJson(outputMappings));
        return obj;
    }

    static SubflowInterface fromJson(const QJsonObject &obj, QString *error = nullptr)
    {
        SubflowInterface result;
        result.schemaVersion = obj.value(QStringLiteral("schemaVersion")).toInt(1);
        result.flowId = obj.value(QStringLiteral("flowId")).toString();
        result.displayName = obj.value(QStringLiteral("displayName")).toString();
        result.description = obj.value(QStringLiteral("description")).toString();
        auto readPorts = [](const QJsonValue &value, bool input) {
            QVector<TypedPortDef> ports;
            for (const QJsonValue &item : value.toArray()) {
                const QJsonObject portObject = item.toObject();
                TypedPortDef port;
                port.id = portObject.value(QStringLiteral("id")).toString();
                port.name = portObject.value(QStringLiteral("name")).toString();
                port.isInput = portObject.value(QStringLiteral("isInput")).toBool(input);
                bool ok = false;
                port.dataType = dataTypeIdFromString(
                    portObject.value(QStringLiteral("dataType")).toString(), &ok);
                port.required = portObject.value(QStringLiteral("required")).toBool(true);
                if (!ok)
                    port.dataType = DataTypeId::None;
                ports.append(port);
            }
            return ports;
        };
        result.inputs = readPorts(obj.value(QStringLiteral("inputs")), true);
        result.outputs = readPorts(obj.value(QStringLiteral("outputs")), false);
        auto readMappings = [](const QJsonValue &value) {
            QVector<SubflowPortMapping> mappings;
            for (const QJsonValue &item : value.toArray()) {
                const QJsonObject mapping = item.toObject();
                mappings.append({mapping.value(QStringLiteral("outerPortId")).toString(),
                                 mapping.value(QStringLiteral("innerNodeId")).toString(),
                                 mapping.value(QStringLiteral("innerPortId")).toString()});
            }
            return mappings;
        };
        result.inputMappings = readMappings(obj.value(QStringLiteral("inputMappings")));
        result.outputMappings = readMappings(obj.value(QStringLiteral("outputMappings")));
        result.validateScope(error);
        return result;
    }
};

} // namespace Selt

#endif
