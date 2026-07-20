#include "nodefactory.h"

#include "core/model/document.h"

QStringList NodeFactory::supportedTypes()
{
    return {
        QStringLiteral("rectangle"),
        QStringLiteral("roundedRectangle"),
        QStringLiteral("ellipse"),
        QStringLiteral("diamond"),
        QStringLiteral("text")
    };
}

QString NodeFactory::displayName(const QString &type)
{
    if (type == QLatin1String("rectangle"))
        return QStringLiteral("矩形");
    if (type == QLatin1String("roundedRectangle"))
        return QStringLiteral("圆角矩形");
    if (type == QLatin1String("ellipse"))
        return QStringLiteral("椭圆");
    if (type == QLatin1String("diamond"))
        return QStringLiteral("菱形");
    if (type == QLatin1String("text"))
        return QStringLiteral("文本");
    return type;
}

NodeModel NodeFactory::create(const QString &type, const QPointF &position)
{
    NodeModel node;
    node.id = Document::createId(QStringLiteral("node"));
    node.type = supportedTypes().contains(type) ? type : QStringLiteral("rectangle");
    node.position = position;
    node.text = displayName(node.type);
    node.ports = NodeModel::defaultPortsForType(node.type);

    if (node.type == QLatin1String("text")) {
        node.size = QSizeF(160, 40);
        node.style.fillColor = QColor(255, 255, 255, 0);
        node.style.borderColor = QColor(120, 120, 120);
        node.style.borderWidth = 1.0;
    } else if (node.type == QLatin1String("ellipse")) {
        node.size = QSizeF(120, 80);
    } else if (node.type == QLatin1String("diamond")) {
        node.size = QSizeF(120, 120);
        node.style.fillColor = QColor(255, 245, 220);
        node.style.borderColor = QColor(180, 120, 40);
    }

    return node;
}
