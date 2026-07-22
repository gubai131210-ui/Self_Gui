#ifndef RESULTSINK_H
#define RESULTSINK_H

#include "foundation/selterror.h"
#include "vision/model/measurementresult.h"

#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QVector>
#include <memory>

namespace Selt {

struct ResultRecord
{
    QString batchId;
    QString frameId;
    QString nodeId;
    QString deviceId;
    QString calibrationId;
    MeasurementResult measurement;
    QString decisionState{QStringLiteral("unknown")};
    QDateTime timestamp{QDateTime::currentDateTimeUtc()};

    QJsonObject toJson() const;
    static ResultRecord fromJson(const QJsonObject &obj);
};

class IResultSink
{
public:
    virtual ~IResultSink() = default;
    virtual Error publish(const QString &key, const QString &value) = 0;
    virtual Error publishRecord(const ResultRecord &record);
};

class CsvResultSink : public IResultSink
{
public:
    explicit CsvResultSink(const QString &filePath);
    Error publish(const QString &key, const QString &value) override;
    Error publishRecord(const ResultRecord &record) override;

private:
    QString m_filePath;
    bool m_headerWritten{false};
};

class JsonLinesResultSink : public IResultSink
{
public:
    explicit JsonLinesResultSink(const QString &filePath);
    Error publish(const QString &key, const QString &value) override;
    Error publishRecord(const ResultRecord &record) override;

private:
    QString m_filePath;
};

class ResultSinkRegistry
{
public:
    static ResultSinkRegistry &instance();
    void clear();
    void addSink(std::shared_ptr<IResultSink> sink);
    QVector<std::shared_ptr<IResultSink>> sinks() const { return m_sinks; }
    QStringList publishAll(const ResultRecord &record);

private:
    QVector<std::shared_ptr<IResultSink>> m_sinks;
};

} // namespace Selt

#endif // RESULTSINK_H
