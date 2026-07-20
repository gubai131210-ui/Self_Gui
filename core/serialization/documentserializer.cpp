#include "documentserializer.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

QByteArray DocumentSerializer::toJson(const Document &document)
{
    QJsonObject root;
    root.insert(QStringLiteral("format"), QString::fromLatin1(FormatName));
    root.insert(QStringLiteral("version"), CurrentVersion);

    QJsonObject docObj;
    docObj.insert(QStringLiteral("title"), document.title());
    docObj.insert(QStringLiteral("settings"), document.settings().toJson());

    QJsonArray nodes;
    for (const NodeModel &node : document.nodes())
        nodes.append(node.toJson());
    docObj.insert(QStringLiteral("nodes"), nodes);

    QJsonArray connections;
    for (const ConnectionModel &c : document.connections())
        connections.append(c.toJson());
    docObj.insert(QStringLiteral("connections"), connections);

    root.insert(QStringLiteral("document"), docObj);
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

bool DocumentSerializer::fromJson(Document &document, const QByteArray &data, QString *errorMessage)
{
    QJsonParseError parseError;
    const QJsonDocument json = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (errorMessage)
            *errorMessage = QStringLiteral("JSON 解析失败: %1").arg(parseError.errorString());
        return false;
    }
    if (!json.isObject()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("根对象必须是 JSON Object");
        return false;
    }

    const QJsonObject root = json.object();
    const QString format = root.value(QStringLiteral("format")).toString();
    if (format != QLatin1String(FormatName)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("不支持的文件格式: %1").arg(format);
        return false;
    }

    const int version = root.value(QStringLiteral("version")).toInt(0);
    if (version <= 0 || version > CurrentVersion) {
        if (errorMessage)
            *errorMessage = QStringLiteral("不支持的文件版本: %1").arg(version);
        return false;
    }

    const QJsonObject docObj = root.value(QStringLiteral("document")).toObject();
    const QString title = docObj.value(QStringLiteral("title")).toString(QStringLiteral("未命名文档"));
    const DocumentSettings settings = DocumentSettings::fromJson(docObj.value(QStringLiteral("settings")).toObject());

    QVector<NodeModel> nodes;
    const QJsonArray nodesArray = docObj.value(QStringLiteral("nodes")).toArray();
    for (const QJsonValue &v : nodesArray) {
        NodeModel node = NodeModel::fromJson(v.toObject());
        if (node.id.isEmpty())
            node.id = Document::createId(QStringLiteral("node"));
        nodes.append(node);
    }

    QVector<ConnectionModel> connections;
    const QJsonArray connectionsArray = docObj.value(QStringLiteral("connections")).toArray();
    for (const QJsonValue &v : connectionsArray) {
        ConnectionModel c = ConnectionModel::fromJson(v.toObject());
        if (c.id.isEmpty())
            c.id = Document::createId(QStringLiteral("conn"));
        connections.append(c);
    }

    document.replaceAll(title, settings, nodes, connections);
    return true;
}

bool DocumentSerializer::saveToFile(const Document &document, const QString &filePath, QString *errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("无法写入文件: %1").arg(file.errorString());
        return false;
    }
    const QByteArray data = toJson(document);
    if (file.write(data) != data.size()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("写入文件时发生错误");
        return false;
    }
    return true;
}

bool DocumentSerializer::loadFromFile(Document &document, const QString &filePath, QString *errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("无法打开文件: %1").arg(file.errorString());
        return false;
    }
    return fromJson(document, file.readAll(), errorMessage);
}
