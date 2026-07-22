#include "vision/runtime/resultsink.h"

#include <QFile>
#include <QJsonDocument>
#include <QTextStream>

namespace Selt {

QJsonObject ResultRecord::toJson() const
{
    return QJsonObject{
        {QStringLiteral("batchId"), batchId},
        {QStringLiteral("frameId"), frameId},
        {QStringLiteral("nodeId"), nodeId},
        {QStringLiteral("deviceId"), deviceId},
        {QStringLiteral("calibrationId"), calibrationId},
        {QStringLiteral("decisionState"), decisionState},
        {QStringLiteral("timestamp"), timestamp.toString(Qt::ISODateWithMs)},
        {QStringLiteral("measurement"), measurement.toJson()},
    };
}

ResultRecord ResultRecord::fromJson(const QJsonObject &obj)
{
    ResultRecord record;
    record.batchId = obj.value(QStringLiteral("batchId")).toString();
    record.frameId = obj.value(QStringLiteral("frameId")).toString();
    record.nodeId = obj.value(QStringLiteral("nodeId")).toString();
    record.deviceId = obj.value(QStringLiteral("deviceId")).toString();
    record.calibrationId = obj.value(QStringLiteral("calibrationId")).toString();
    record.decisionState = obj.value(QStringLiteral("decisionState"))
                               .toString(QStringLiteral("unknown"));
    record.timestamp = QDateTime::fromString(
        obj.value(QStringLiteral("timestamp")).toString(), Qt::ISODateWithMs);
    if (!record.timestamp.isValid())
        record.timestamp = QDateTime::currentDateTimeUtc();
    record.measurement = MeasurementResult::fromJson(
        obj.value(QStringLiteral("measurement")).toObject());
    return record;
}

Error IResultSink::publishRecord(const ResultRecord &record)
{
    return publish(QStringLiteral("result"),
                   QString::fromUtf8(QJsonDocument(record.toJson()).toJson(QJsonDocument::Compact)));
}

CsvResultSink::CsvResultSink(const QString &filePath)
    : m_filePath(filePath)
{
}

Error CsvResultSink::publish(const QString &key, const QString &value)
{
    ResultRecord record;
    record.nodeId = key;
    record.measurement.message = value;
    record.measurement.valid = true;
    return publishRecord(record);
}

Error CsvResultSink::publishRecord(const ResultRecord &record)
{
    QFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return Error::fail(ErrorCode::ExecutionFailed,
                           QStringLiteral("无法写入 CSV: %1").arg(file.errorString()));
    QTextStream out(&file);
    if (!m_headerWritten && file.size() == 0) {
        out << QStringLiteral("timestamp,batchId,frameId,nodeId,deviceId,calibrationId,decision,width,height,area,unit,message\n");
        m_headerWritten = true;
    }
    out << record.timestamp.toString(Qt::ISODateWithMs) << ','
        << record.batchId << ','
        << record.frameId << ','
        << record.nodeId << ','
        << record.deviceId << ','
        << record.calibrationId << ','
        << record.decisionState << ','
        << record.measurement.width << ','
        << record.measurement.height << ','
        << record.measurement.area << ','
        << record.measurement.unit << ','
        << '"' << record.measurement.message << '"' << '\n';
    return Error::success();
}

JsonLinesResultSink::JsonLinesResultSink(const QString &filePath)
    : m_filePath(filePath)
{
}

Error JsonLinesResultSink::publish(const QString &key, const QString &value)
{
    ResultRecord record;
    record.nodeId = key;
    record.measurement.message = value;
    record.measurement.valid = true;
    return publishRecord(record);
}

Error JsonLinesResultSink::publishRecord(const ResultRecord &record)
{
    QFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return Error::fail(ErrorCode::ExecutionFailed,
                           QStringLiteral("无法写入 JSON: %1").arg(file.errorString()));
    QTextStream out(&file);
    out << QString::fromUtf8(QJsonDocument(record.toJson()).toJson(QJsonDocument::Compact)) << '\n';
    return Error::success();
}

ResultSinkRegistry &ResultSinkRegistry::instance()
{
    static ResultSinkRegistry reg;
    return reg;
}

void ResultSinkRegistry::clear()
{
    m_sinks.clear();
}

void ResultSinkRegistry::addSink(std::shared_ptr<IResultSink> sink)
{
    if (sink)
        m_sinks.append(std::move(sink));
}

QStringList ResultSinkRegistry::publishAll(const ResultRecord &record)
{
    QStringList errors;
    for (const std::shared_ptr<IResultSink> &sink : m_sinks) {
        if (!sink)
            continue;
        const Error err = sink->publishRecord(record);
        if (!err.ok())
            errors.append(err.message);
    }
    return errors;
}

} // namespace Selt
