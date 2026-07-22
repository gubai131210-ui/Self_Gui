#include "projectresourcestore.h"

#include <QDir>
#include <QFileInfo>
#include <QUuid>

namespace Selt {
namespace {

bool isEscapingRoot(const QString &path)
{
    const QString cleaned = QDir::cleanPath(path).replace(QLatin1Char('\\'), QLatin1Char('/'));
    return cleaned == QLatin1String("..") || cleaned.startsWith(QStringLiteral("../"));
}

} // namespace

QJsonObject ProjectResource::toJson() const
{
    return QJsonObject{
        {QStringLiteral("id"), id},
        {QStringLiteral("kind"), kind},
        {QStringLiteral("displayName"), displayName},
        {QStringLiteral("relativePath"), relativePath},
        {QStringLiteral("notes"), notes}};
}

ProjectResource ProjectResource::fromJson(const QJsonObject &obj)
{
    ProjectResource r;
    r.id = obj.value(QStringLiteral("id")).toString();
    r.kind = obj.value(QStringLiteral("kind")).toString();
    r.displayName = obj.value(QStringLiteral("displayName")).toString();
    r.relativePath = obj.value(QStringLiteral("relativePath")).toString();
    r.notes = obj.value(QStringLiteral("notes")).toString();
    return r;
}

void ProjectResourceStore::setProjectRoot(const QString &rootDir)
{
    m_projectRoot = QDir::cleanPath(rootDir);
    refreshExistence();
}

QString ProjectResourceStore::addOrUpdate(const ProjectResource &resource)
{
    ProjectResource copy = resource;
    if (copy.id.isEmpty())
        copy.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    copy.relativePath = normalizeRelativePath(copy.relativePath);
    m_resources.insert(copy.id, copy);
    refreshExistence();
    return copy.id;
}

bool ProjectResourceStore::remove(const QString &id)
{
    return m_resources.remove(id) > 0;
}

bool ProjectResourceStore::contains(const QString &id) const
{
    return m_resources.contains(id);
}

ProjectResource ProjectResourceStore::resource(const QString &id) const
{
    return m_resources.value(id);
}

QVector<ProjectResource> ProjectResourceStore::resources() const
{
    QVector<ProjectResource> out;
    out.reserve(m_resources.size());
    for (const ProjectResource &r : m_resources)
        out.append(r);
    return out;
}

QVector<ProjectResource> ProjectResourceStore::resourcesOfKind(const QString &kind) const
{
    QVector<ProjectResource> out;
    for (const ProjectResource &r : m_resources)
        if (r.kind == kind)
            out.append(r);
    return out;
}

QStringList ProjectResourceStore::refreshExistence()
{
    QStringList missing;
    for (auto it = m_resources.begin(); it != m_resources.end(); ++it) {
        ProjectResource &r = it.value();
        r.relativePath = normalizeRelativePath(r.relativePath);
        if (m_projectRoot.isEmpty() || r.relativePath.isEmpty() || isEscapingRoot(r.relativePath)) {
            r.absolutePath.clear();
            r.exists = false;
            missing.append(r.id);
            continue;
        }
        r.absolutePath = QDir(m_projectRoot).filePath(r.relativePath);
        r.exists = QFileInfo::exists(r.absolutePath);
        if (!r.exists)
            missing.append(r.id);
    }
    return missing;
}

QStringList ProjectResourceStore::missingDiagnostics() const
{
    QStringList lines;
    const QVector<ResourceDiagnostic> items = diagnostics();
    for (const ResourceDiagnostic &item : items) {
        if (item.severity != QLatin1String("error"))
            continue;
        lines.append(QStringLiteral("[%1] %2").arg(item.code, item.message));
    }
    return lines;
}

QVector<ResourceDiagnostic> ProjectResourceStore::diagnostics() const
{
    QVector<ResourceDiagnostic> out;
    for (const ProjectResource &r : m_resources) {
        ResourceDiagnostic item;
        item.resourceId = r.id;
        item.relativePath = r.relativePath;
        item.severity = QStringLiteral("error");
        if (r.relativePath.trimmed().isEmpty()) {
            item.code = QStringLiteral("resource_path_empty");
            item.message = QStringLiteral("资源 %1 缺少相对路径")
                               .arg(r.displayName.isEmpty() ? r.id : r.displayName);
            out.append(item);
            continue;
        }
        if (QFileInfo(r.relativePath).isAbsolute()) {
            item.code = QStringLiteral("resource_path_absolute");
            item.message = QStringLiteral("资源 %1 使用了绝对路径: %2")
                               .arg(r.displayName.isEmpty() ? r.id : r.displayName, r.relativePath);
            out.append(item);
            continue;
        }
        if (isEscapingRoot(r.relativePath)) {
            item.code = QStringLiteral("resource_path_invalid");
            item.message = QStringLiteral("资源 %1 路径越过工程根目录: %2")
                               .arg(r.displayName.isEmpty() ? r.id : r.displayName, r.relativePath);
            out.append(item);
            continue;
        }
        if (!r.exists) {
            item.code = QStringLiteral("resource_missing");
            item.message = QStringLiteral("缺失资源 %1 (%2): %3")
                               .arg(r.displayName.isEmpty() ? r.id : r.displayName,
                                    r.kind,
                                    r.relativePath);
            out.append(item);
        }
    }
    return out;
}

QString ProjectResourceStore::absolutePathFor(const QString &id) const
{
    return m_resources.value(id).absolutePath;
}

QString ProjectResourceStore::makeRelativePath(const QString &absolutePath) const
{
    if (m_projectRoot.isEmpty())
        return normalizeRelativePath(absolutePath);
    return normalizeRelativePath(QDir(m_projectRoot).relativeFilePath(absolutePath));
}

QString ProjectResourceStore::normalizeRelativePath(const QString &path) const
{
    if (path.trimmed().isEmpty())
        return QString();
    QString normalized = QDir::cleanPath(path.trimmed());
    normalized.replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (!m_projectRoot.isEmpty() && QFileInfo(normalized).isAbsolute())
        normalized = QDir(m_projectRoot).relativeFilePath(normalized);
    normalized = QDir::cleanPath(normalized).replace(QLatin1Char('\\'), QLatin1Char('/'));
    return normalized;
}

QJsonObject ProjectResourceStore::toJson() const
{
    QJsonArray arr;
    for (const ProjectResource &r : m_resources)
        arr.append(r.toJson());
    return QJsonObject{{QStringLiteral("resources"), arr}};
}

void ProjectResourceStore::fromJson(const QJsonObject &obj)
{
    m_resources.clear();
    const QJsonArray arr = obj.value(QStringLiteral("resources")).toArray();
    for (const QJsonValue &v : arr) {
        const ProjectResource r = ProjectResource::fromJson(v.toObject());
        if (!r.id.isEmpty())
            m_resources.insert(r.id, r);
    }
    refreshExistence();
}

void ProjectResourceStore::clear()
{
    m_resources.clear();
}

} // namespace Selt
