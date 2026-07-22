#ifndef PROJECTRESOURCESTORE_H
#define PROJECTRESOURCESTORE_H

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace Selt {

struct ProjectResource
{
    QString id;
    QString kind;           // e.g. "template"
    QString displayName;
    QString relativePath;   // relative to project root
    QString absolutePath;   // resolved at load time; may be empty if missing
    bool exists{false};
    QString notes;

    QJsonObject toJson() const;
    static ProjectResource fromJson(const QJsonObject &obj);
};

struct ResourceDiagnostic
{
    QString code;
    QString severity;
    QString resourceId;
    QString relativePath;
    QString message;

    QJsonObject toJson() const
    {
        return QJsonObject{
            {QStringLiteral("code"), code},
            {QStringLiteral("severity"), severity},
            {QStringLiteral("resourceId"), resourceId},
            {QStringLiteral("relativePath"), relativePath},
            {QStringLiteral("message"), message}};
    }
};

class ProjectResourceStore
{
public:
    void setProjectRoot(const QString &rootDir);
    QString projectRoot() const { return m_projectRoot; }

    QString addOrUpdate(const ProjectResource &resource);
    bool remove(const QString &id);
    bool contains(const QString &id) const;
    ProjectResource resource(const QString &id) const;
    QVector<ProjectResource> resources() const;
    QVector<ProjectResource> resourcesOfKind(const QString &kind) const;

    /// Resolve relative paths against project root and refresh exists flags.
    QStringList refreshExistence();
    /// Missing resources return diagnostics (id + relativePath).
    QStringList missingDiagnostics() const;
    QVector<ResourceDiagnostic> diagnostics() const;

    QString absolutePathFor(const QString &id) const;
    QString makeRelativePath(const QString &absolutePath) const;
    QString normalizeRelativePath(const QString &path) const;

    QJsonObject toJson() const;
    void fromJson(const QJsonObject &obj);
    void clear();

private:
    QString m_projectRoot;
    QHash<QString, ProjectResource> m_resources;
};

} // namespace Selt

#endif // PROJECTRESOURCESTORE_H
