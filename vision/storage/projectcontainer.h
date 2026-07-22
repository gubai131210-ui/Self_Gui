#ifndef PROJECTCONTAINER_H
#define PROJECTCONTAINER_H

#include "vision/domain/visionproject.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace Selt {

struct ProjectContainerManifest
{
    QString formatName{QStringLiteral("selt-project-container")};
    int formatVersion{1};
    int schemaVersion{1};
    int nodeSchemaVersion{1};
    QString projectId;
    QString title;
    QStringList flowIds;
    QString activeFlowId;
    QString applicationVersion;
    QStringList assetPaths;
    QJsonArray resourceEntries;
    QJsonArray flows;
    QJsonArray pluginDependencies;
    QJsonArray diagnostics;
    QJsonObject extensions;

    QJsonObject toJson() const
    {
        return QJsonObject{
            {QStringLiteral("format"), formatName},
            {QStringLiteral("version"), formatVersion},
            {QStringLiteral("schemaVersion"), schemaVersion},
            {QStringLiteral("nodeSchemaVersion"), nodeSchemaVersion},
            {QStringLiteral("projectId"), projectId},
            {QStringLiteral("title"), title},
            {QStringLiteral("flows"), QJsonArray::fromStringList(flowIds)},
            {QStringLiteral("activeFlowId"), activeFlowId},
            {QStringLiteral("applicationVersion"), applicationVersion},
            {QStringLiteral("assets"), QJsonArray::fromStringList(assetPaths)},
            {QStringLiteral("resourceEntries"), resourceEntries},
            {QStringLiteral("flowEntries"), flows},
            {QStringLiteral("pluginDependencies"), pluginDependencies},
            {QStringLiteral("diagnostics"), diagnostics},
            {QStringLiteral("extensions"), extensions}};
    }

    static ProjectContainerManifest fromJson(const QJsonObject &obj)
    {
        ProjectContainerManifest m;
        m.formatName = obj.value(QStringLiteral("format")).toString(m.formatName);
        m.formatVersion = obj.value(QStringLiteral("version")).toInt(1);
        m.schemaVersion = obj.value(QStringLiteral("schemaVersion")).toInt(1);
        m.nodeSchemaVersion = obj.value(QStringLiteral("nodeSchemaVersion")).toInt(1);
        m.projectId = obj.value(QStringLiteral("projectId")).toString();
        m.title = obj.value(QStringLiteral("title")).toString();
        for (const QJsonValue &v : obj.value(QStringLiteral("flows")).toArray())
            m.flowIds.append(v.toString());
        m.activeFlowId = obj.value(QStringLiteral("activeFlowId")).toString();
        m.applicationVersion = obj.value(QStringLiteral("applicationVersion")).toString();
        for (const QJsonValue &v : obj.value(QStringLiteral("assets")).toArray())
            m.assetPaths.append(v.toString());
        m.resourceEntries = obj.value(QStringLiteral("resourceEntries")).toArray();
        m.flows = obj.value(QStringLiteral("flowEntries")).toArray();
        m.pluginDependencies = obj.value(QStringLiteral("pluginDependencies")).toArray();
        m.diagnostics = obj.value(QStringLiteral("diagnostics")).toArray();
        m.extensions = obj.value(QStringLiteral("extensions")).toObject();
        return m;
    }
};

struct ProjectContainerReport
{
    bool ok{true};
    QString error;
    QStringList diagnostics;
    ProjectContainerManifest manifest;
};

class ProjectContainerFormat
{
public:
    static constexpr const char *FormatName = "selt-project-container";
    static constexpr int FormatVersion = 1;

    /// Validate manifest shape without reading assets.
    static bool validateManifest(const ProjectContainerManifest &manifest, QString *error = nullptr);
};

class ProjectContainerStorage
{
public:
    static bool save(const QString &containerPath, const VisionProject &project,
                     QString *error = nullptr);
    static bool load(const QString &containerPath, VisionProject &project,
                     ProjectContainerReport *report = nullptr);
};

inline bool ProjectContainerFormat::validateManifest(const ProjectContainerManifest &manifest, QString *error)
{
    if (manifest.formatName != QLatin1String(FormatName)) {
        if (error)
            *error = QStringLiteral("容器格式标识不匹配");
        return false;
    }
    if (manifest.formatVersion < 1 || manifest.formatVersion > FormatVersion) {
        if (error)
            *error = QStringLiteral("不支持的容器版本: %1").arg(manifest.formatVersion);
        return false;
    }
    if (manifest.projectId.isEmpty()) {
        if (error)
            *error = QStringLiteral("容器缺少 projectId");
        return false;
    }
    return true;
}

} // namespace Selt

#endif // PROJECTCONTAINER_H
