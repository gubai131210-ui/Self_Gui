#ifndef VISIONNODEREGISTRY_H
#define VISIONNODEREGISTRY_H

#include "core/model/nodemodel.h"
#include "vision/model/moduledescriptor.h"

#include <QString>
#include <QStringList>
#include <QVector>

class VisionNodeRegistry
{
public:
    static QStringList supportedTypes();
    static QStringList categories();
    static QStringList typesInCategory(const QString &category);
    static QString displayName(const QString &type);
    static QString category(const QString &type);
    static ModuleDescriptor descriptor(const QString &type);
    static QVector<ModuleDescriptor> allDescriptors();
    static NodeModel create(const QString &type, const QPointF &position = QPointF(0, 0));
    static QJsonObject defaultParameters(const QString &type);
    static bool validateParameters(const QString &type, const QJsonObject &params, QString *error = nullptr);
    static QSizeF fixedNodeSize();
};

#endif // VISIONNODEREGISTRY_H
