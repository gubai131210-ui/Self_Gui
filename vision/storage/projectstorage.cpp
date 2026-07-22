#include "vision/storage/projectstorage.h"

#include "core/serialization/documentserializer.h"
#include "vision/registry/visionnoderegistry.h"
#include "vision/storage/projectresourcestore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace Selt {

namespace {

int peekSeltVersion(const QString &path, QString *formatOut = nullptr)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return 0;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return 0;
    const QJsonObject root = doc.object();
    if (formatOut)
        *formatOut = root.value(QStringLiteral("format")).toString();
    return root.value(QStringLiteral("version")).toInt(0);
}

QJsonObject readRootObject(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    return doc.isObject() ? doc.object() : QJsonObject{};
}

} // namespace

bool ProjectStorage::loadLegacySelt(const QString &path, VisionProject &project, MigrationReport *report)
{
    QString format;
    const int fileVersion = peekSeltVersion(path, &format);

    Document doc;
    QString error;
    if (!DocumentSerializer::loadFromFile(doc, path, &error)) {
        if (report) {
            report->ok = false;
            report->fromVersion = fileVersion;
            report->toVersion = DocumentSerializer::CurrentVersion;
            report->error = error;
            if (fileVersion > 0 && fileVersion < DocumentSerializer::CurrentVersion) {
                report->notes.append(
                    QStringLiteral("检测到旧版本 .selt (v%1)，但解析失败").arg(fileVersion));
            }
        }
        return false;
    }

    QVector<NodeModel> upgradedNodes = doc.nodes();
    QVector<ConnectionModel> upgradedConnections = doc.connections();
    VisionNodeRegistry::upgradeGraph(upgradedNodes, upgradedConnections);
    Document upgradedDoc;
    upgradedDoc.replaceAll(doc.title(), doc.settings(), upgradedNodes, upgradedConnections);
    project.importLegacyDocument(upgradedDoc);

    int bindingCount = 0;
    int constantFallback = 0;
    for (const NodeModel &node : upgradedDoc.nodes()) {
        if (node.parameterBindings.isEmpty()) {
            constantFallback += node.parameters.size();
        } else {
            bindingCount += node.parameterBindings.size();
        }
    }

    // Optional extension field under document.projectVariables (does not bump file version).
    const QJsonObject root = readRootObject(path);
    const QJsonObject docObj = root.value(QStringLiteral("document")).toObject();
    int lostVariables = 0;
    QString variableLoadNote;
    if (docObj.contains(QStringLiteral("projectVariables"))) {
        ProjectVariableStore vars;
        QString varError;
        if (vars.fromJson(docObj.value(QStringLiteral("projectVariables")).toArray(), &varError)) {
            project.setVariables(vars);
        } else {
            ++lostVariables;
            variableLoadNote = QStringLiteral("项目变量解析失败: %1").arg(varError);
        }
    }

    if (docObj.contains(QStringLiteral("projectResources"))) {
        ProjectResourceStore resources;
        resources.fromJson(docObj.value(QStringLiteral("projectResources")).toObject());
        resources.setProjectRoot(QFileInfo(path).absolutePath());
        project.setResources(resources);
        if (report) {
            const QStringList missing = resources.missingDiagnostics();
            for (const QString &line : missing)
                report->notes.append(line);
            report->notes.append(
                QStringLiteral("项目资源 %1 个").arg(resources.resources().size()));
        }
    } else {
        ProjectResourceStore resources;
        resources.setProjectRoot(QFileInfo(path).absolutePath());
        project.setResources(resources);
    }

    if (report) {
        report->ok = true;
        report->error.clear();
        report->fromVersion = fileVersion > 0 ? fileVersion : DocumentSerializer::CurrentVersion;
        report->toVersion = DocumentSerializer::CurrentVersion;
        // Keep any resource notes already appended above.
        if (report->notes.isEmpty())
            report->notes.clear();
        if (!variableLoadNote.isEmpty())
            report->notes.append(variableLoadNote);
        if (!format.isEmpty() && format != QLatin1String(DocumentSerializer::FormatName)) {
            report->notes.append(QStringLiteral("格式标识: %1").arg(format));
        }
        if (report->fromVersion < DocumentSerializer::CurrentVersion) {
            report->notes.append(
                QStringLiteral("已从 .selt v%1 迁移到运行时 v%2（未改写文件版本）")
                    .arg(report->fromVersion)
                    .arg(report->toVersion));
        } else {
            report->notes.append(QStringLiteral("已将单文档工程导入默认主流程「主流程」"));
        }
        report->notes.append(
            QStringLiteral("导入节点 %1 个，连线 %2 个")
                .arg(upgradedDoc.nodes().size())
                .arg(upgradedDoc.connections().size()));
        report->notes.append(
            QStringLiteral("参数绑定 %1 个；按常量解释的旧参数 %2 个；丢失变量 %3 个")
                .arg(bindingCount)
                .arg(constantFallback)
                .arg(lostVariables));
    }
    return true;
}

bool ProjectStorage::saveLegacySelt(const QString &path, const VisionProject &project, QString *error)
{
    if (!project.activeFlow()) {
        if (error)
            *error = QStringLiteral("没有可保存的活动流程");
        return false;
    }
    Document doc;
    project.exportActiveFlowToDocument(doc);
    ProjectResourceStore resources = project.resources();
    resources.setProjectRoot(QFileInfo(path).absolutePath());
    return saveDocumentWithExtensions(path, doc, project.variables(), resources, error);
}

bool ProjectStorage::saveDocumentWithVariables(const QString &path, const Document &document,
                                               const ProjectVariableStore &variables, QString *error)
{
    ProjectResourceStore empty;
    return saveDocumentWithExtensions(path, document, variables, empty, error);
}

bool ProjectStorage::saveDocumentWithExtensions(const QString &path, const Document &document,
                                                const ProjectVariableStore &variables,
                                                const ProjectResourceStore &resources,
                                                QString *error)
{
    QByteArray data = DocumentSerializer::toJson(document);
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
    if (jsonDoc.isObject()) {
        QJsonObject root = jsonDoc.object();
        QJsonObject docObj = root.value(QStringLiteral("document")).toObject();
        const QJsonArray vars = variables.toJson();
        if (!vars.isEmpty())
            docObj.insert(QStringLiteral("projectVariables"), vars);
        else
            docObj.remove(QStringLiteral("projectVariables"));

        const QJsonObject resourceObj = resources.toJson();
        if (!resourceObj.value(QStringLiteral("resources")).toArray().isEmpty())
            docObj.insert(QStringLiteral("projectResources"), resourceObj);
        else
            docObj.remove(QStringLiteral("projectResources"));

        root.insert(QStringLiteral("document"), docObj);
        data = QJsonDocument(root).toJson(QJsonDocument::Indented);
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error)
            *error = QStringLiteral("无法写入文件: %1").arg(file.errorString());
        return false;
    }
    if (file.write(data) != data.size()) {
        if (error)
            *error = QStringLiteral("写入文件时发生错误");
        return false;
    }
    return true;
}

bool ProjectStorage::saveContainer(const QString &path, const VisionProject &project,
                                   QString *error)
{
    return ProjectContainerStorage::save(path, project, error);
}

bool ProjectStorage::loadContainer(const QString &path, VisionProject &project,
                                   ProjectContainerReport *report)
{
    return ProjectContainerStorage::load(path, project, report);
}

} // namespace Selt
