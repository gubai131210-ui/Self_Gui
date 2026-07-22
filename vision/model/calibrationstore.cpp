#include "calibrationstore.h"

CalibrationModel CalibrationStore::active() const
{
    for (const CalibrationModel &item : m_items) {
        if (item.calibrationId == m_activeId)
            return item;
    }
    return {};
}

void CalibrationStore::setItems(const QVector<CalibrationModel> &items)
{
    m_items = items;
    if (!active().valid && !m_items.isEmpty())
        m_activeId = m_items.first().calibrationId;
}

void CalibrationStore::upsert(const CalibrationModel &model)
{
    if (model.calibrationId.isEmpty())
        return;
    for (CalibrationModel &item : m_items) {
        if (item.calibrationId == model.calibrationId) {
            item = model;
            return;
        }
    }
    m_items.append(model);
    if (m_activeId.isEmpty())
        m_activeId = model.calibrationId;
}

bool CalibrationStore::remove(const QString &calibrationId)
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).calibrationId != calibrationId)
            continue;
        m_items.removeAt(i);
        if (m_activeId == calibrationId)
            m_activeId = m_items.isEmpty() ? QString() : m_items.first().calibrationId;
        return true;
    }
    return false;
}

bool CalibrationStore::setActiveId(const QString &calibrationId)
{
    for (const CalibrationModel &item : m_items) {
        if (item.calibrationId == calibrationId) {
            m_activeId = calibrationId;
            return true;
        }
    }
    return false;
}

void CalibrationStore::clear()
{
    m_items.clear();
    m_activeId.clear();
}

QJsonObject CalibrationStore::toJson() const
{
    QJsonArray arr;
    for (const CalibrationModel &item : m_items)
        arr.append(item.toJson());
    return QJsonObject{
        {QStringLiteral("activeId"), m_activeId},
        {QStringLiteral("items"), arr},
    };
}

CalibrationStore CalibrationStore::fromJson(const QJsonObject &obj)
{
    CalibrationStore store;
    const QJsonArray arr = obj.value(QStringLiteral("items")).toArray();
    for (const QJsonValue &v : arr)
        store.m_items.append(CalibrationModel::fromJson(v.toObject()));
    store.m_activeId = obj.value(QStringLiteral("activeId")).toString();
    if (!store.active().valid && !store.m_items.isEmpty())
        store.m_activeId = store.m_items.first().calibrationId;
    return store;
}
