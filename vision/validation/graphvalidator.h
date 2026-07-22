#ifndef GRAPHVALIDATOR_H
#define GRAPHVALIDATOR_H

#include "core/model/connectionmodel.h"
#include "core/model/document.h"
#include "core/model/nodemodel.h"
#include "vision/domain/visionflow.h"
#include "vision/validation/graphdiagnostic.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace Selt {

class GraphValidator
{
public:
    /// Structure-only checks used by VisionFlow::replaceGraph callers.
    static QVector<GraphDiagnostic> validateStructure(const QVector<NodeModel> &nodes,
                                                      const QVector<ConnectionModel> &connections);

    /// Full static validation including typed ports and parameter bindings.
    static QVector<GraphDiagnostic> validateFlow(const VisionFlow &flow);
    static QVector<GraphDiagnostic> validateDocument(const Document &document);

    static bool hasErrors(const QVector<GraphDiagnostic> &diags);
    static QStringList toMessages(const QVector<GraphDiagnostic> &diags);

    /// Check a single prospective connection (canvas create).
    /// When \a allowReplaceOccupied is true, an occupied input is treated as replaceable.
    static bool canConnect(const Document &document,
                           const QString &sourceNodeId, const QString &sourcePortId,
                           const QString &targetNodeId, const QString &targetPortId,
                           QString *error = nullptr,
                           bool allowReplaceOccupied = false);

    /// Publish / export gate: structure + typed ports + resource missing diagnostics.
    static QVector<GraphDiagnostic> validateForPublish(const Document &document,
                                                       const QStringList &resourceMissingMessages = {});
};

} // namespace Selt

#endif // GRAPHVALIDATOR_H
