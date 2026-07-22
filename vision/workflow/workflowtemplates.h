#ifndef WORKFLOWTEMPLATES_H
#define WORKFLOWTEMPLATES_H

#include "core/model/connectionmodel.h"
#include "core/model/nodemodel.h"

#include <QPointF>
#include <QString>
#include <QStringList>
#include <QVector>

namespace Selt {

struct WorkflowTemplateSpec
{
    QString id;
    QString displayName;
    QString description;
    QVector<NodeModel> nodes;
    QVector<ConnectionModel> connections;
};

/// Factory for VisionMaster-style starter flows (additive; does not mutate Document).
class WorkflowTemplates
{
public:
    static QStringList templateIds();
    static WorkflowTemplateSpec build(const QString &id, const QPointF &origin = QPointF(40, 40));
    /// Preferred toolbox stage order for categories.
    static QStringList categoryStageOrder();
    static int categoryStageRank(const QString &category);
};

} // namespace Selt

#endif // WORKFLOWTEMPLATES_H
