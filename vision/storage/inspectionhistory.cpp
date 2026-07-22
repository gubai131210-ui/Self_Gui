#include "vision/storage/inspectionhistory.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QTextStream>

#include <algorithm>

namespace Selt {
namespace {

void setError(QString *error, const QString &message)
{
    if (error)
        *error = message;
}

QString safePathPart(QString value)
{
    value = value.trimmed();
    value.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._-]+")),
                  QStringLiteral("_"));
    return value.isEmpty() ? QStringLiteral("unknown") : value;
}

} // namespace

QJsonObject InspectionRecord::toJson() const
{
    QJsonObject obj{
        {QStringLiteral("id"), id},
        {QStringLiteral("recipeId"), recipeId},
        {QStringLiteral("recipeVersionId"), recipeVersionId},
        {QStringLiteral("batchId"), batchId},
        {QStringLiteral("result"), result.toJson()},
        {QStringLiteral("ngImagePath"), ngImagePath},
        {QStringLiteral("timestamp"), timestamp.toString(Qt::ISODateWithMs)},
    };
    return obj;
}

InspectionRecord InspectionRecord::fromJson(const QJsonObject &obj)
{
    InspectionRecord record;
    record.id = obj.value(QStringLiteral("id")).toString();
    record.recipeId = obj.value(QStringLiteral("recipeId")).toString();
    record.recipeVersionId = obj.value(QStringLiteral("recipeVersionId")).toString();
    record.batchId = obj.value(QStringLiteral("batchId")).toString();
    record.result = ResultRecord::fromJson(obj.value(QStringLiteral("result")).toObject());
    record.ngImagePath = obj.value(QStringLiteral("ngImagePath")).toString();
    record.timestamp = QDateTime::fromString(
        obj.value(QStringLiteral("timestamp")).toString(), Qt::ISODateWithMs);
    if (!record.timestamp.isValid())
        record.timestamp = record.result.timestamp;
    return record;
}

InspectionHistoryStore::InspectionHistoryStore(QString filePath)
    : m_filePath(std::move(filePath))
{
}

bool InspectionHistoryStore::append(const InspectionRecord &record, QString *error) const
{
    if (m_filePath.trimmed().isEmpty()) {
        setError(error, QStringLiteral("检测历史文件路径不能为空"));
        return false;
    }
    QFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        setError(error, file.errorString());
        return false;
    }
    QTextStream stream(&file);
    stream << QString::fromUtf8(QJsonDocument(record.toJson()).toJson(QJsonDocument::Compact))
           << '\n';
    return true;
}

QVector<InspectionRecord> InspectionHistoryStore::query(const InspectionQuery &filter,
                                                        QString *error) const
{
    QVector<InspectionRecord> records;
    QFile file(m_filePath);
    if (!file.exists())
        return records;
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setError(error, file.errorString());
        return records;
    }
    while (!file.atEnd()) {
        const QByteArray line = file.readLine().trimmed();
        if (line.isEmpty())
            continue;
        const QJsonDocument document = QJsonDocument::fromJson(line);
        if (!document.isObject())
            continue;
        const InspectionRecord record = InspectionRecord::fromJson(document.object());
        const QString state = record.result.decisionState;
        if (!filter.resultState.isEmpty() && state != filter.resultState)
            continue;
        if (!filter.batchId.isEmpty() && record.batchId != filter.batchId)
            continue;
        if (!filter.recipeId.isEmpty() && record.recipeId != filter.recipeId)
            continue;
        records.append(record);
        if (filter.limit > 0 && records.size() >= filter.limit)
            break;
    }
    return records;
}

NgImageArchivePolicy::NgImageArchivePolicy(QString rootDirectory)
    : m_rootDirectory(std::move(rootDirectory))
{
}

QString NgImageArchivePolicy::archivePath(const InspectionRecord &record,
                                          const QString &extension) const
{
    const QDateTime timestamp = record.timestamp.isValid()
                                    ? record.timestamp.toUTC()
                                    : QDateTime::currentDateTimeUtc();
    const QString datePath = timestamp.toString(QStringLiteral("yyyy/MM/dd"));
    const QString fileId = safePathPart(record.id.isEmpty()
                                            ? timestamp.toString(QStringLiteral("HHmmsszzz"))
                                            : record.id);
    QString suffix = extension.trimmed();
    if (suffix.startsWith(QChar('.')))
        suffix.remove(0, 1);
    if (suffix.isEmpty())
        suffix = QStringLiteral("png");
    return QDir(m_rootDirectory).filePath(
        QStringLiteral("%1/%2/%3.%4")
            .arg(datePath, safePathPart(record.batchId), fileId, safePathPart(suffix)));
}

bool NgImageArchivePolicy::archive(const QString &sourcePath, const InspectionRecord &record,
                                   const QString &extension, QString *archivedPath,
                                   QString *error) const
{
    const QString target = archivePath(record, extension);
    const QFileInfo targetInfo(target);
    if (!QDir().mkpath(targetInfo.absolutePath())) {
        setError(error, QStringLiteral("无法创建 NG 图像目录"));
        return false;
    }
    if (!QFile::copy(sourcePath, target)) {
        setError(error, QStringLiteral("无法归档 NG 图像: %1").arg(sourcePath));
        return false;
    }
    if (archivedPath)
        *archivedPath = target;
    return true;
}

int NgImageArchivePolicy::applyRetention(const NgImageRetentionPolicy &policy,
                                         const QDateTime &now, QString *error) const
{
    if (m_rootDirectory.isEmpty() || !QDir(m_rootDirectory).exists())
        return 0;
    int removedCount = 0;
    QVector<QFileInfo> files;
    QDirIterator iterator(m_rootDirectory, QStringList() << QStringLiteral("*.png")
                                                          << QStringLiteral("*.jpg")
                                                          << QStringLiteral("*.jpeg"),
                          QDir::Files, QDirIterator::Subdirectories);
    const QDateTime cutoff = now.addDays(-qMax(0, policy.maxAgeDays));
    while (iterator.hasNext()) {
        const QFileInfo info(iterator.next());
        if (policy.maxAgeDays > 0 && info.lastModified() < cutoff) {
            if (!QFile::remove(info.absoluteFilePath()))
                setError(error, QStringLiteral("无法删除过期 NG 图像: %1").arg(info.fileName()));
            else
                ++removedCount;
        } else {
            files.append(info);
        }
    }
    if (policy.maxFiles > 0 && files.size() > policy.maxFiles) {
        std::sort(files.begin(), files.end(),
                  [](const QFileInfo &left, const QFileInfo &right) {
                      return left.lastModified() < right.lastModified();
                  });
        const int removeCount = files.size() - policy.maxFiles;
        for (int i = 0; i < removeCount; ++i) {
            if (!QFile::remove(files.at(i).absoluteFilePath()))
                setError(error, QStringLiteral("无法删除超量 NG 图像: %1")
                                     .arg(files.at(i).fileName()));
        }
        return removedCount + removeCount;
    }
    return removedCount;
}

} // namespace Selt
