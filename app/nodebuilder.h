#ifndef NODEBUILDER_H
#define NODEBUILDER_H

#include "core/model/nodemodel.h"

#include <QPointF>
#include <QString>

class NodeBuilder
{
public:
    static QString displayName(const QString &type);
    static NodeModel create(const QString &type, const QPointF &position = QPointF(0, 0));
};

#endif // NODEBUILDER_H
