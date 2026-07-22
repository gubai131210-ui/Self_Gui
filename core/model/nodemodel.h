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
    bool enabled{true}; // domain enable flag; scheduling of disabled nodes is later phases
    /// Debug breakpoint: FlowRuntime pauses before executing this node.
    bool breakpoint{false};
    /// UI-only fold state; does not affect DAG execution or ports.
    bool collapsed{false};
    int zValue{0};
    QString groupId;
    QJsonObject parameters;
    /// Optional per-parameter bindings (key → ParameterBinding JSON). Absent keys are Constant.
    QJsonObject parameterBindings;
    /// Canvas-visible port ids. Empty + !customized → use descriptor defaults.
    QStringList exposedPortIds;
    /// When true, exposedPortIds is user-authored and must be preserved across upgrades.
    bool portExposureCustomized{false};

    QJsonObject toJson() const;
    static NodeModel fromJson(const QJsonObject &obj);

    const PortModel *findPort(const QString &portId) const;
    PortModel *findPort(const QString &portId);

    static QList<PortModel> defaultPortsForType(const QString &type);
};

#endif // NODEMODEL_H
