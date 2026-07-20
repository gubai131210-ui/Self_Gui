#ifndef NODEMODEL_H
#define NODEMODEL_H

#include "nodestyle.h"
#include "portmodel.h"

#include <QJsonObject>
#include <QList>
#include <QPointF>
#include <QSizeF>
#include <QString>
#include <QStringList>

struct NodeModel
{
    QString id;
    QString type; // rectangle, roundedRectangle, ellipse, diamond, text
    QString text;
    QString note;
    QString url;
    QString imagePath;
    QPointF position{0, 0};
    QSizeF size{140, 70};
    NodeStyle style;
    QList<PortModel> ports;
    bool locked{false};
    int zValue{0};
    QString groupId;

    QJsonObject toJson() const;
    static NodeModel fromJson(const QJsonObject &obj);

    static QList<PortModel> defaultPortsForType(const QString &type);
};

#endif // NODEMODEL_H
