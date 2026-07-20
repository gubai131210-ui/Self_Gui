#ifndef NODEFACTORY_H
#define NODEFACTORY_H

#include "core/model/nodemodel.h"

#include <QPointF>
#include <QString>
#include <QStringList>

class NodeFactory
{
public:
    static QStringList supportedTypes();
    static QString displayName(const QString &type);
    static NodeModel create(const QString &type, const QPointF &position = QPointF(0, 0));
};

#endif // NODEFACTORY_H
