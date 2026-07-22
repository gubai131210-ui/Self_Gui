#pragma once

#include "vision/runtime/resultsink.h"

#include <QDateTime>
#include <QString>
#include <QVector>

namespace Selt {

struct InspectionRecord
{
    QString id;
    QString recipeId;
    QString recipeVersionId;
    QString batchId;
    ResultRecord result;
    QString ngImagePath;
    QDateTime timestamp{QDateTime::currentDateTimeUtc()};

    QJsonObject toJson() const;
    static InspectionRecord fromJson(const QJsonObject &obj);
};

struct InspectionQuery
{
    QString resultState;
    QString batchId;
    QString recipeId;
    int limit{0};
};

class InspectionHistoryStore
{
public:
    explicit InspectionHistoryStore(QString filePath = {});

    QString filePath() const { return m_filePath; }
    void setFilePath(const QString &filePath) { m_filePath = filePath; }
    bool append(const InspectionRecord &record, QString *error = nullptr) const;
    QVector<InspectionRecord> query(const InspectionQuery &query,
                                    QString *error = nullptr) const;

private:
    QString m_filePath;
};

struct NgImageRetentionPolicy
{
    int maxAgeDays{30};
    int maxFiles{0};
};

class NgImageArchivePolicy
{
public:
    explicit NgImageArchivePolicy(QString rootDirectory = {});

    QString rootDirectory() const { return m_rootDirectory; }
    void setRootDirectory(const QString &rootDirectory) { m_rootDirectory = rootDirectory; }
    QString archivePath(const InspectionRecord &record,
                        const QString &extension = QStringLiteral("png")) const;
    bool archive(const QString &sourcePath, const InspectionRecord &record,
                 const QString &extension = QStringLiteral("png"),
                 QString *archivedPath = nullptr, QString *error = nullptr) const;
    int applyRetention(const NgImageRetentionPolicy &policy,
                       const QDateTime &now = QDateTime::currentDateTimeUtc(),
                       QString *error = nullptr) const;

private:
    QString m_rootDirectory;
};

} // namespace Selt
