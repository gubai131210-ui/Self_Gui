#include "nodemodel.h"

#include <QJsonArray>

QList<PortModel> NodeModel::defaultPortsForType(const QString &type)
{
    Q_UNUSED(type);
    PortModel left;
    left.id = QStringLiteral("in");
    left.name = QStringLiteral("In");
    left.direction = PortDirection::Input;
    left.relativeX = 0.0;
    left.relativeY = 0.5;

    PortModel right;
    right.id = QStringLiteral("out");
    right.name = QStringLiteral("Out");
    right.direction = PortDirection::Output;
    right.relativeX = 1.0;
    right.relativeY = 0.5;

    return {left, right};
}

QJsonObject NodeModel::toJson() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("id"), id);
    obj.insert(QStringLiteral("type"), type);
    obj.insert(QStringLiteral("text"), text);
    obj.insert(QStringLiteral("note"), note);
    obj.insert(QStringLiteral("url"), url);
    obj.insert(QStringLiteral("imagePath"), imagePath);
    obj.insert(QStringLiteral("x"), position.x());
    obj.insert(QStringLiteral("y"), position.y());
    obj.insert(QStringLiteral("width"), size.width());
    obj.insert(QStringLiteral("height"), size.height());
    obj.insert(QStringLiteral("style"), style.toJson());
    obj.insert(QStringLiteral("locked"), locked);
    obj.insert(QStringLiteral("enabled"), enabled);
    obj.insert(QStringLiteral("breakpoint"), breakpoint);
    obj.insert(QStringLiteral("collapsed"), collapsed);
    obj.insert(QStringLiteral("zValue"), zValue);
    obj.insert(QStringLiteral("groupId"), groupId);
    if (!parameters.isEmpty())
        obj.insert(QStringLiteral("parameters"), parameters);
    if (!parameterBindings.isEmpty())
        obj.insert(QStringLiteral("parameterBindings"), parameterBindings);
    if (portExposureCustomized || !exposedPortIds.isEmpty()) {
        QJsonArray exposed;
        for (const QString &id : exposedPortIds)
            exposed.append(id);
        obj.insert(QStringLiteral("exposedPortIds"), exposed);
        obj.insert(QStringLiteral("portExposureCustomized"), portExposureCustomized);
    }

    QJsonArray portsArray;
    for (const PortModel &port : ports)
        portsArray.append(port.toJson());
    obj.insert(QStringLiteral("ports"), portsArray);
    return obj;
}

const PortModel *NodeModel::findPort(const QString &portId) const
{
    for (const PortModel &port : ports) {
        if (port.id == portId)
            return &port;
    }
    return nullptr;
}

PortModel *NodeModel::findPort(const QString &portId)
{
    for (PortModel &port : ports) {
        if (port.id == portId)
            return &port;
    }
    return nullptr;
}

NodeModel NodeModel::fromJson(const QJsonObject &obj)
{
    NodeModel node;
    node.id = obj.value(QStringLiteral("id")).toString();
    node.type = obj.value(QStringLiteral("type")).toString(QStringLiteral("rectangle"));
    node.text = obj.value(QStringLiteral("text")).toString();
    node.note = obj.value(QStringLiteral("note")).toString();
    node.url = obj.value(QStringLiteral("url")).toString();
    node.imagePath = obj.value(QStringLiteral("imagePath")).toString();
    node.position = QPointF(obj.value(QStringLiteral("x")).toDouble(),
                            obj.value(QStringLiteral("y")).toDouble());
    node.size = QSizeF(obj.value(QStringLiteral("width")).toDouble(140),
                       obj.value(QStringLiteral("height")).toDouble(70));
    node.style = NodeStyle::fromJson(obj.value(QStringLiteral("style")).toObject());
    node.locked = obj.value(QStringLiteral("locked")).toBool(false);
    node.enabled = obj.value(QStringLiteral("enabled")).toBool(true);
    node.breakpoint = obj.value(QStringLiteral("breakpoint")).toBool(false);
    node.collapsed = obj.value(QStringLiteral("collapsed")).toBool(false);
    node.zValue = obj.value(QStringLiteral("zValue")).toInt(0);
    node.groupId = obj.value(QStringLiteral("groupId")).toString();
    node.parameters = obj.value(QStringLiteral("parameters")).toObject();
    node.parameterBindings = obj.value(QStringLiteral("parameterBindings")).toObject();
    node.portExposureCustomized = obj.value(QStringLiteral("portExposureCustomized")).toBool(false);
    const QJsonArray exposedArray = obj.value(QStringLiteral("exposedPortIds")).toArray();
    for (const QJsonValue &v : exposedArray)
        node.exposedPortIds.append(v.toString());

    const QJsonArray portsArray = obj.value(QStringLiteral("ports")).toArray();
    if (portsArray.isEmpty()) {
        node.ports = defaultPortsForType(node.type);
    } else {
        for (const QJsonValue &v : portsArray)
            node.ports.append(PortModel::fromJson(v.toObject()));
    }
    return node;
}
