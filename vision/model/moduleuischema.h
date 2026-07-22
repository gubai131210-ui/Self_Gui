#ifndef MODULEUISCHEMA_H
#define MODULEUISCHEMA_H

#include <QString>
#include <QStringList>

namespace Selt {

/// Declarative UI hints for operators (built-in + plugins).
/// Kept separate from execution so UI widgets stay algorithm-agnostic.
struct ModuleUiSchema
{
    /// Suggested parameter editor: "default" | "color" | "roi" | "file" | "binding"
    QString editorHint;
    /// Preview overlay kinds: image | contour | geometry | measurement | none
    QStringList previewLayers;
    /// Capability tags: capability_limited | needs_calibration | teachable
    QStringList capabilityTags;
    QString helpUrl;
    QString exampleSnippet;
    QString compatVersion;

    bool hasTag(const QString &tag) const { return capabilityTags.contains(tag); }
};

} // namespace Selt

#endif // MODULEUISCHEMA_H
