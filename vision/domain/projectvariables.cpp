#include "vision/domain/projectvariables.h"

#include "foundation/seltid.h"
#include "vision/domain/visionflow.h"
#include "vision/domain/visionproject.h"

#include <QJsonArray>

namespace Selt {

QJsonObject ProjectVariableRecord::toJson() const
{
    return QJsonObject{
        {QStringLiteral("id"), id},
        {QStringLiteral("name"), name},
        {QStringLiteral("dataType"), dataTypeIdToString(dataType)},
        {QStringLiteral("value"), value.toJson()},
        {QStringLiteral("defaultValue"), defaultValue.toJson()},
        {QStringLiteral("description"), description}};
}

ProjectVariableRecord ProjectVariableRecord::fromJson(const QJsonObject &obj, QString *error)
{
    ProjectVariableRecord rec;
    rec.id = obj.value(QStringLiteral("id")).toString();
    rec.name = obj.value(QStringLiteral("name")).toString();
    bool ok = false;
    rec.dataType = dataTypeIdFromString(obj.value(QStringLiteral("dataType")).toString(), &ok);
    if (!ok) {
        if (error)
            *error = QStringLiteral("未知变量类型");
        return {};
    }
    QString err;
    if (obj.value(QStringLiteral("value")).isObject())
        rec.value = DataValue::fromJson(obj.value(QStringLiteral("value")).toObject(), &err);
    if (obj.value(QStringLiteral("defaultValue")).isObject()
        && !obj.value(QStringLiteral("defaultValue")).toObject().isEmpty()) {
        rec.defaultValue = DataValue::fromJson(obj.value(QStringLiteral("defaultValue")).toObject(), &err);
    } else {
        rec.defaultValue = rec.value;
    }
    rec.description = obj.value(QStringLiteral("description")).toString();
    if (rec.id.isEmpty())
        rec.id = IdGenerator::create(QStringLiteral("var"));
    if (rec.name.isEmpty())
        rec.name = rec.id;
    return rec;
}

QString ProjectVariableStore::add(const QString &name, DataTypeId type, const DataValue &value,
                                  const QString &description)
{
    ProjectVariableRecord rec;
    rec.id = IdGenerator::create(QStringLiteral("var"));
    rec.name = name.isEmpty() ? rec.id : name;
    rec.dataType = type;
    rec.value = value.isNull() ? DataValue() : value;
    if (rec.value.isNull()) {
        switch (type) {
        case DataTypeId::Bool: rec.value = DataValue(false); break;
        case DataTypeId::Int: rec.value = DataValue(0); break;
        case DataTypeId::Real: rec.value = DataValue(0.0); break;
        case DataTypeId::String: rec.value = DataValue(QString()); break;
        default: break;
        }
    }
    rec.defaultValue = rec.value;
    rec.description = description;
    m_values.insert(rec.id, rec);
    m_order.append(rec.id);
    return rec.id;
}

bool ProjectVariableStore::remove(const QString &id)
{
    if (!m_values.contains(id))
        return false;
    m_values.remove(id);
    m_order.removeAll(id);
    return true;
}

bool ProjectVariableStore::rename(const QString &id, const QString &name)
{
    if (!m_values.contains(id) || name.isEmpty())
        return false;
    m_values[id].name = name;
    return true;
}

bool ProjectVariableStore::setType(const QString &id, DataTypeId type, QString *error)
{
    if (!m_values.contains(id)) {
        if (error)
            *error = QStringLiteral("变量不存在");
        return false;
    }
    ProjectVariableRecord &rec = m_values[id];
    if (rec.dataType == type)
        return true;
    DataValue converted;
    QString err;
    if (!rec.value.isNull() && !rec.value.convertTo(type, &converted, &err)) {
        // Reset to typed default when conversion is impossible.
        switch (type) {
        case DataTypeId::Bool: converted = DataValue(false); break;
        case DataTypeId::Int: converted = DataValue(0); break;
        case DataTypeId::Real: converted = DataValue(0.0); break;
        case DataTypeId::String: converted = DataValue(QString()); break;
        default:
            if (error)
                *error = err.isEmpty() ? QStringLiteral("无法转换变量类型") : err;
            return false;
        }
    }
    rec.dataType = type;
    rec.value = converted;
    rec.defaultValue = converted;
    return true;
}

bool ProjectVariableStore::setValue(const QString &id, const DataValue &value, QString *error)
{
    if (!m_values.contains(id)) {
        if (error)
            *error = QStringLiteral("变量不存在");
        return false;
    }
    ProjectVariableRecord &rec = m_values[id];
    if (!value.isNull() && !value.isValidFor(rec.dataType)
        && !dataTypesCompatible(value.type(), rec.dataType)) {
        DataValue converted;
        QString err;
        if (!value.convertTo(rec.dataType, &converted, &err)) {
            if (error)
                *error = err;
            return false;
        }
        rec.value = converted;
        return true;
    }
    rec.value = value;
    return true;
}

bool ProjectVariableStore::setDescription(const QString &id, const QString &description)
{
    if (!m_values.contains(id))
        return false;
    m_values[id].description = description;
    return true;
}

bool ProjectVariableStore::contains(const QString &id) const
{
    return m_values.contains(id);
}

ProjectVariableRecord ProjectVariableStore::record(const QString &id) const
{
    return m_values.value(id);
}

DataValue ProjectVariableStore::get(const QString &id) const
{
    return m_values.value(id).value;
}

QVector<ProjectVariableRecord> ProjectVariableStore::records() const
{
    QVector<ProjectVariableRecord> list;
    for (const QString &id : m_order)
        list.append(m_values.value(id));
    return list;
}

void ProjectVariableStore::clear()
{
    m_values.clear();
    m_order.clear();
}

QJsonArray ProjectVariableStore::toJson() const
{
    QJsonArray arr;
    for (const QString &id : m_order)
        arr.append(m_values.value(id).toJson());
    return arr;
}

bool ProjectVariableStore::fromJson(const QJsonArray &arr, QString *error)
{
    clear();
    for (const QJsonValue &v : arr) {
        QString err;
        ProjectVariableRecord rec = ProjectVariableRecord::fromJson(v.toObject(), &err);
        if (rec.id.isEmpty()) {
            if (error)
                *error = err.isEmpty() ? QStringLiteral("变量记录无效") : err;
            return false;
        }
        if (m_values.contains(rec.id)) {
            if (error)
                *error = QStringLiteral("重复变量 ID: %1").arg(rec.id);
            return false;
        }
        m_values.insert(rec.id, rec);
        m_order.append(rec.id);
    }
    return true;
}

QStringList ProjectVariableStore::findReferences(const VisionProject &project, const QString &variableId)
{
    QStringList refs;
    for (VisionFlow *flow : project.flows()) {
        if (!flow)
            continue;
        for (const NodeModel &node : flow->nodes()) {
            for (auto it = node.parameterBindings.begin(); it != node.parameterBindings.end(); ++it) {
                const ParameterBinding b = ParameterBinding::fromJson(it.value().toObject());
                if (b.kind == ParameterSourceKind::ProjectVariable && b.projectVariableId == variableId) {
                    refs.append(QStringLiteral("%1/%2.%3").arg(flow->name(), node.id, it.key()));
                }
            }
        }
    }
    return refs;
}

void ProjectVariableStore::set(const QString &key, const DataValue &value)
{
    if (m_values.contains(key)) {
        setValue(key, value);
        return;
    }
    ProjectVariableRecord rec;
    rec.id = key;
    rec.name = key;
    rec.dataType = value.isNull() ? DataTypeId::Real : value.type();
    rec.value = value;
    rec.defaultValue = value;
    m_values.insert(key, rec);
    m_order.append(key);
}

} // namespace Selt
