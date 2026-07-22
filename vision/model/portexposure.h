#ifndef PORTEXPOSURE_H
#define PORTEXPOSURE_H

#include "core/model/connectionmodel.h"
#include "core/model/nodemodel.h"
#include "vision/model/moduledescriptor.h"

#include <QStringList>
#include <QVector>

namespace Selt {

QString portRoleToString(PortRole role);
PortRole portRoleFromString(const QString &text, bool *ok = nullptr);
QString portRoleDisplayName(PortRole role);
QString defaultPortGroup(DataTypeId type, bool isInput = true);

/// Apply heuristic exposure metadata for built-in descriptors.
void applyBuiltinPortExposureHints(ModuleDescriptor &descriptor);

/// Default exposed ports for a descriptor (respects hasExposureHints).
QStringList defaultExposedPortIds(const ModuleDescriptor &descriptor);

/// Resolve canvas-visible ports: custom selection ∪ defaults ∪ connected ports.
QStringList resolveExposedPortIds(const NodeModel &node,
                                  const ModuleDescriptor &descriptor,
                                  const QVector<ConnectionModel> &connections = {});

/// True when the port should be rendered on the canvas.
bool isPortExposedOnCanvas(const NodeModel &node,
                           const ModuleDescriptor &descriptor,
                           const QString &portId,
                           const QVector<ConnectionModel> &connections = {});

/// Sync exposedPortIds after create/upgrade; never drops connected ports.
void syncNodePortExposure(NodeModel &node,
                          const ModuleDescriptor &descriptor,
                          const QVector<ConnectionModel> &connections = {});

/// Relayout only canvas-visible ports; keeps relative positions stable for hidden ones.
void layoutExposedPorts(const ModuleDescriptor &descriptor,
                        NodeModel &node,
                        const QVector<ConnectionModel> &connections = {});

} // namespace Selt

#endif // PORTEXPOSURE_H
