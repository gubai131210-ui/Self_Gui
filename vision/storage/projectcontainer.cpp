#include "vision/storage/projectcontainer.h"

#include "core/serialization/documentserializer.h"
#include "foundation/seltversion.h"
#include "plugin_host/pluginhost.h"
#include "vision/registry/visionnoderegistry.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QUuid>

namespace Selt {
namespace {

bool writeJson(const QString &path, const QJsonObject &object, QString *error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = QStringLiteral("无法写入 %1: %2").arg(path, file.errorString());
        return false;
    }
    const QByteArray data = QJsonDocument(object).toJson(QJsonDocument::Indented);
    if (file.write(data) != data.size() || !file.commit()) {
        if (error)
            *error = QStringLiteral("提交文件失败: %1").arg(path);
        return false;
    }
    return true;
}

bool copyFileTo(const QString &source, const QString &target, QString *error)
{
    QDir().mkpath(QFileInfo(target).absolutePath());
    if (QFile::exists(target))
        QFile::remove(target);
    if (QFile::copy(source, target))
        return true;
    if (error)
        *error = QStringLiteral("复制资源失败: %1 -> %2").arg(source, target);
    return false;
}

ProjectContainerManifest makeManifest(const VisionProject &project)
{
    ProjectContainerManifest manifest;
    manifest.projectId = project.projectId();
    manifest.title = project.title();
    manifest.activeFlowId = project.activeFlowId();
    manifest.applicationVersion = QStringLiteral("0.2.0");
    manifest.flowIds = project.flowIds();
    QSet<QString> usedPluginTypes;
    for (VisionFlow *flow : project.flows()) {
        manifest.flows.append(QJsonObject{
            {QStringLiteral("id"), flow->flowId()},
            {QStringLiteral("name"), flow->name()},
            {QStringLiteral("main"), flow->isMainFlow()}});
        for (const NodeModel &node : flow->nodes()) {
            if (VisionNodeRegistry::isExternalType(node.type))
                usedPluginTypes.insert(node.type);
        }
    }
    for (const ProjectResource &resource : project.resources().resources()) {
        manifest.assetPaths.append(resource.relativePath);
        manifest.resourceEntries.append(resource.toJson());
    }
    for (const QString &typeId : usedPluginTypes) {
        const QString pluginId = VisionNodeRegistry::pluginIdForType(typeId);
        if (pluginId.isEmpty())
            continue;
        manifest.pluginDependencies.append(QJsonObject{
            {QStringLiteral("pluginId"), pluginId},
            {QStringLiteral("nodeTypeId"), typeId},
            {QStringLiteral("sdkVersion"), VersionInfo::pluginSdkVersion},
            {QStringLiteral("abiTag"), QStringLiteral("qt6-opencv")},
            {QStringLiteral("compiler"), QStringLiteral("mingw")}});
    }
    for (const ResourceDiagnostic &diagnostic : project.resources().diagnostics())
        manifest.diagnostics.append(diagnostic.toJson());
    manifest.extensions.insert(QStringLiteral("projectVariables"), project.variables().toJson());
    return manifest;
}

} // namespace

bool ProjectContainerStorage::save(const QString &containerPath,
                                   const VisionProject &project,
                                   QString *error)
{
    const QString target = QDir::cleanPath(containerPath);
    const QString temp = target + QStringLiteral(".tmp-")
        + QUuid::createUuid().toString(QUuid::WithoutBraces);
    QDir tempDir;
    if (!tempDir.mkpath(temp + QStringLiteral("/flows"))
        || !tempDir.mkpath(temp + QStringLiteral("/assets"))) {
        if (error)
            *error = QStringLiteral("无法创建临时工程目录: %1").arg(temp);
        return false;
    }

    const ProjectContainerManifest manifest = makeManifest(project);
    for (VisionFlow *flow : project.flows()) {
        Document document;
        project.exportActiveFlowToDocument(document);
        if (flow != project.activeFlow()) {
            document.replaceAll(flow->name(), document.settings(),
                               flow->nodes(), flow->connections());
        }
        const QString flowPath = QDir(temp).filePath(
            QStringLiteral("flows/%1.json").arg(flow->flowId()));
        QJsonDocument flowJson = QJsonDocument::fromJson(DocumentSerializer::toJson(document));
        if (!flowJson.isObject()) {
            QDir(temp).removeRecursively();
            if (error)
                *error = QStringLiteral("流程序列化失败: %1").arg(flow->flowId());
            return false;
        }
        QJsonObject flowObj = flowJson.object();
        flowObj.insert(QStringLiteral("flowScopedVariables"), flow->variablesToJson());
        if (!writeJson(flowPath, flowObj, error)) {
            QDir(temp).removeRecursively();
            return false;
        }
    }

    for (const ProjectResource &resource : project.resources().resources()) {
        const QString source = project.resources().absolutePathFor(resource.id);
        if (!QFileInfo::exists(source)) {
            continue;
        }
        const QString destination = QDir(temp).filePath(resource.relativePath);
        if (!copyFileTo(source, destination, error)) {
            QDir(temp).removeRecursively();
            return false;
        }
    }

    if (!writeJson(QDir(temp).filePath(QStringLiteral("manifest.json")),
                   manifest.toJson(), error)) {
        QDir(temp).removeRecursively();
        return false;
    }

    const QString backup = target + QStringLiteral(".backup");
    QDir targetDir(target);
    if (targetDir.exists()) {
        QDir(backup).removeRecursively();
        if (!QDir().rename(target, backup)) {
            if (error)
                *error = QStringLiteral("无法备份原工程目录: %1").arg(target);
            QDir(temp).removeRecursively();
            return false;
        }
    }
    if (!QDir().rename(temp, target)) {
        if (QDir(backup).exists())
            QDir().rename(backup, target);
        if (error)
            *error = QStringLiteral("无法提交工程容器: %1").arg(target);
        QDir(temp).removeRecursively();
        return false;
    }
    QDir(backup).removeRecursively();
    return true;
}

bool ProjectContainerStorage::load(const QString &containerPath,
                                   VisionProject &project,
                                   ProjectContainerReport *report)
{
    ProjectContainerReport local;
    ProjectContainerReport *out = report ? report : &local;
    const QString root = QDir::cleanPath(containerPath);
    QFile manifestFile(QDir(root).filePath(QStringLiteral("manifest.json")));
    if (!manifestFile.open(QIODevice::ReadOnly)) {
        out->ok = false;
        out->error = QStringLiteral("无法打开工程清单: %1").arg(manifestFile.errorString());
        return false;
    }
    const QJsonDocument json = QJsonDocument::fromJson(manifestFile.readAll());
    if (!json.isObject()) {
        out->ok = false;
        out->error = QStringLiteral("manifest.json 必须是对象");
        return false;
    }
    out->manifest = ProjectContainerManifest::fromJson(json.object());
    if (!ProjectContainerFormat::validateManifest(out->manifest, &out->error)) {
        out->ok = false;
        return false;
    }
    out->diagnostics.append(
        PluginHost::instance().diagnoseDependencies(out->manifest.pluginDependencies));
    for (const QJsonValue &value : out->manifest.diagnostics) {
        const QJsonObject item = value.toObject();
        out->diagnostics.append(
            QStringLiteral("[%1] %2")
                .arg(item.value(QStringLiteral("code")).toString(),
                     item.value(QStringLiteral("message")).toString()));
    }

    project.clearAllFlows();
    project.setProjectId(out->manifest.projectId);
    project.setTitle(out->manifest.title);
    for (const QJsonValue &entryValue : out->manifest.flows) {
        const QJsonObject entry = entryValue.toObject();
        const QString flowId = entry.value(QStringLiteral("id")).toString();
        VisionFlow *flow = project.createFlowWithId(
            flowId, entry.value(QStringLiteral("name")).toString(QStringLiteral("流程")));
        if (!flow)
            continue;
        QFile flowFile(QDir(root).filePath(QStringLiteral("flows/%1.json").arg(flowId)));
        Document document;
        QString flowError;
        if (!DocumentSerializer::loadFromFile(document, flowFile.fileName(), &flowError)
            || !flow->replaceGraph(document.nodes(), document.connections(), &flowError)) {
            out->diagnostics.append(QStringLiteral("流程 %1 加载失败: %2").arg(flowId, flowError));
        } else {
            // Optional flow/group scoped variables embedded in flow JSON.
            QFile raw(flowFile.fileName());
            if (raw.open(QIODevice::ReadOnly)) {
                const QJsonDocument docJson = QJsonDocument::fromJson(raw.readAll());
                if (docJson.isObject()) {
                    const QJsonObject scoped =
                        docJson.object().value(QStringLiteral("flowScopedVariables")).toObject();
                    if (!scoped.isEmpty()) {
                        QString varErr;
                        if (!flow->variablesFromJson(scoped, &varErr))
                            out->diagnostics.append(
                                QStringLiteral("流程 %1 变量加载: %2").arg(flowId, varErr));
                    }
                }
            }
        }
    }
    if (!out->manifest.activeFlowId.isEmpty())
        project.setActiveFlow(out->manifest.activeFlowId);

    // Restore project variables from extensions (compat: missing → empty).
    {
        ProjectVariableStore vars;
        const QJsonArray arr =
            out->manifest.extensions.value(QStringLiteral("projectVariables")).toArray();
        QString varErr;
        if (!arr.isEmpty() && !vars.fromJson(arr, &varErr))
            out->diagnostics.append(QStringLiteral("工程变量加载失败: %1").arg(varErr));
        else
            project.setVariables(vars);
    }

    ProjectResourceStore resources;
    resources.setProjectRoot(root);
    QJsonObject resourceObject;
    QJsonArray resourceArray = out->manifest.resourceEntries;
    if (resourceArray.isEmpty()) {
        for (const QString &path : out->manifest.assetPaths) {
            ProjectResource resource;
            resource.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            resource.kind = QStringLiteral("asset");
            resource.relativePath = path;
            resource.displayName = QFileInfo(path).fileName();
            resourceArray.append(resource.toJson());
        }
    }
    resourceObject.insert(QStringLiteral("resources"), resourceArray);
    resources.fromJson(resourceObject);
    out->diagnostics.append(resources.missingDiagnostics());
    project.setResources(resources);
    project.markClean();
    out->ok = true;
    return true;
}

} // namespace Selt
