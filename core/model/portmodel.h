#ifndef PORTMODEL_H
#define PORTMODEL_H

#include <QJsonObject>
#include <QString>

enum class PortDirection {
    Input,
    Output,
    Both
};

struct PortModel
{
    QString id;
    QString name;
    PortDirection direction{PortDirection::Both};
    /// Serialized DataTypeId name (e.g. "Image"); empty = unknown/legacy.
    QString dataType;
    qreal relativeX{0.5}; // 0..1 within node bounds
    qreal relativeY{0.5};

    QJsonObject toJson() const;
    static PortModel fromJson(const QJsonObject &obj);
};

#endif // PORTMODEL_H
