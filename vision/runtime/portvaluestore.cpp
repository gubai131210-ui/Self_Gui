#include "vision/runtime/portvaluestore.h"

namespace Selt {

QString PortValueStore::key(const QString &nodeId, const QString &portId)
{
    return nodeId + QLatin1Char('\x1f') + portId;
}

QStringList PortValueStore::aliasKeys(const QString &nodeId, const QString &portId)
{
    QStringList keys;
    keys.append(key(nodeId, portId));
    if (portId == QLatin1String("image")) {
        keys.append(key(nodeId, QStringLiteral("out")));
        keys.append(key(nodeId, QStringLiteral("in")));
    } else if (portId == QLatin1String("out") || portId == QLatin1String("in")) {
        keys.append(key(nodeId, QStringLiteral("image")));
    }
    return keys;
}

void PortValueStore::clear()
{
    m_values.clear();
    m_types.clear();
}

void PortValueStore::declareType(const QString &nodeId, const QString &portId, DataTypeId type)
{
    m_types.insert(key(nodeId, portId), type);
}

DataTypeId PortValueStore::declaredType(const QString &nodeId, const QString &portId) const
{
    return m_types.value(key(nodeId, portId), DataTypeId::None);
}

bool PortValueStore::set(const QString &nodeId, const QString &portId, const DataValue &value, QString *error)
{
    const QString k = key(nodeId, portId);
    const DataTypeId expected = m_types.value(k, DataTypeId::None);
    if (expected != DataTypeId::None && !value.isNull()
        && !value.isValidFor(expected) && !dataTypesCompatible(value.type(), expected)) {
        if (error) {
            *error = QStringLiteral("端口 %1.%2 拒绝写入类型 %3（期望 %4）")
                         .arg(nodeId, portId, dataTypeIdDisplayName(value.type()),
                              dataTypeIdDisplayName(expected));
        }
        return false;
    }
    m_values.insert(k, value);
    if (expected == DataTypeId::None && !value.isNull())
        m_types.insert(k, value.type());
    return true;
}

bool PortValueStore::contains(const QString &nodeId, const QString &portId) const
{
    return m_values.contains(key(nodeId, portId));
}

DataValue PortValueStore::value(const QString &nodeId, const QString &portId) const
{
    return m_values.value(key(nodeId, portId));
}

bool PortValueStore::containsAliased(const QString &nodeId, const QString &portId) const
{
    for (const QString &k : aliasKeys(nodeId, portId)) {
        if (m_values.contains(k))
            return true;
    }
    return false;
}

DataValue PortValueStore::valueAliased(const QString &nodeId, const QString &portId) const
{
    for (const QString &k : aliasKeys(nodeId, portId)) {
        if (m_values.contains(k))
            return m_values.value(k);
    }
    return {};
}

} // namespace Selt
