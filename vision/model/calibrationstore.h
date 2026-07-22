#ifndef CALIBRATIONSTORE_H
#define CALIBRATIONSTORE_H

#include "vision/model/calibrationmodel.h"

#include <QJsonArray>
#include <QString>
#include <QVector>

/// Project-scoped collection of calibrations with one active selection.
class CalibrationStore
{
public:
    QVector<CalibrationModel> items() const { return m_items; }
    QString activeId() const { return m_activeId; }
    CalibrationModel active() const;
    bool hasActive() const { return active().valid; }

    void setItems(const QVector<CalibrationModel> &items);
    void upsert(const CalibrationModel &model);
    bool remove(const QString &calibrationId);
    bool setActiveId(const QString &calibrationId);
    void clear();

    QJsonObject toJson() const;
    static CalibrationStore fromJson(const QJsonObject &obj);

private:
    QVector<CalibrationModel> m_items;
    QString m_activeId;
};

#endif // CALIBRATIONSTORE_H
