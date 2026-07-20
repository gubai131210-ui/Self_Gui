#ifndef CONNECTIONMODEL_H
#define CONNECTIONMODEL_H

#include <QJsonObject>
#include <QString>

enum class ConnectionLineStyle {
    Straight,
    Orthogonal
};

struct ConnectionModel
{
    QString id;
    QString sourceNodeId;
    QString sourcePortId;
    QString targetNodeId;
    QString targetPortId;
    QString label;
    ConnectionLineStyle lineStyle{ConnectionLineStyle::Orthogonal};
    bool showArrow{true};

    QJsonObject toJson() const;
    static ConnectionModel fromJson(const QJsonObject &obj);
};

#endif // CONNECTIONMODEL_H
