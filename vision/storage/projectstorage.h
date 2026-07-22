#ifndef PROJECTSTORAGE_H
#define PROJECTSTORAGE_H

#include "core/model/document.h"
#include "vision/domain/visionproject.h"
#include "vision/storage/projectresourcestore.h"
#include "vision/storage/projectcontainer.h"

#include <QString>

namespace Selt {

struct MigrationReport
{
    int fromVersion{0};
    int toVersion{0};
    QStringList notes;
    bool ok{true};
    QString error;
};

class ProjectStorage
{
public:
    /// Load legacy .selt into VisionProject (v1/v2/v3).
    static bool loadLegacySelt(const QString &path, VisionProject &project, MigrationReport *report = nullptr);
    /// Save active flow via legacy serializer for compatibility.
    static bool saveLegacySelt(const QString &path, const VisionProject &project, QString *error = nullptr);
    /// Save Document canvas state plus optional project variables / resources extension.
    static bool saveDocumentWithVariables(const QString &path, const Document &document,
                                          const ProjectVariableStore &variables, QString *error = nullptr);
    static bool saveDocumentWithExtensions(const QString &path, const Document &document,
                                           const ProjectVariableStore &variables,
                                           const ProjectResourceStore &resources,
                                           QString *error = nullptr);
    static bool saveContainer(const QString &path, const VisionProject &project,
                              QString *error = nullptr);
    static bool loadContainer(const QString &path, VisionProject &project,
                              ProjectContainerReport *report = nullptr);
    static constexpr const char *ContainerFormatName = "selt-project-container";
    static constexpr int ContainerFormatVersion = 1;
};

} // namespace Selt

#endif
