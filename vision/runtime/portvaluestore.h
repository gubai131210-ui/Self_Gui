#ifndef PORTVALUESTORE_H
#define PORTVALUESTORE_H

#include "vision/data/datatype.h"

#include <QHash>
#include <QString>

namespace Selt {

class PortValueStore
{
public:
    void clear();
    bool set(const QString &nodeId, const QString &portId, const DataValue &value, QString *error = nullptr);
    bool contains(const QString &nodeId, const QString &portId) const;
    DataValue value(const QString &nodeId, const QString &portId) const;
    DataTypeId declaredType(const QString &nodeId, const QString &portId) const;
    void declareType(const QString &nodeId, const QString &portId, DataTypeId type);

    /// Lookup with in/out ↔ image aliases for legacy graphs.
    bool containsAliased(const QString &nodeId, const QString &portId) const;
    DataValue valueAliased(const QString &nodeId, const QString &portId) const;

private:
    static QString key(const QString &nodeId, const QString &portId);
    static QStringList aliasKeys(const QString &nodeId, const QString &portId);

    QHash<QString, DataValue> m_values;
    QHash<QString, DataTypeId> m_types;
};

} // namespace Selt

#endif // PORTVALUESTORE_H
