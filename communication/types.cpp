#include "communication/types.h"

namespace Selt {
namespace Communication {

QString transportKindToString(TransportKind kind)
{
    switch (kind) {
    case TransportKind::Serial:
        return QStringLiteral("serial");
    case TransportKind::ModbusTcp:
        return QStringLiteral("modbus_tcp");
    case TransportKind::ModbusRtu:
        return QStringLiteral("modbus_rtu");
    case TransportKind::Tcp:
    default:
        return QStringLiteral("tcp");
    }
}

TransportKind transportKindFromString(const QString &text, bool *ok)
{
    const QString t = text.trimmed().toLower();
    auto setOk = [ok](bool v) {
        if (ok)
            *ok = v;
    };
    if (t == QLatin1String("serial") || t == QLatin1String("com")) {
        setOk(true);
        return TransportKind::Serial;
    }
    if (t == QLatin1String("modbus_tcp") || t == QLatin1String("modbustcp")) {
        setOk(true);
        return TransportKind::ModbusTcp;
    }
    if (t == QLatin1String("modbus_rtu") || t == QLatin1String("modbusrtu")) {
        setOk(true);
        return TransportKind::ModbusRtu;
    }
    if (t == QLatin1String("tcp") || t.isEmpty()) {
        setOk(true);
        return TransportKind::Tcp;
    }
    setOk(false);
    return TransportKind::Tcp;
}

QString receiveFrameModeToString(ReceiveFrameMode mode)
{
    switch (mode) {
    case ReceiveFrameMode::IdleGap:
        return QStringLiteral("idle_gap");
    case ReceiveFrameMode::Terminator:
        return QStringLiteral("terminator");
    case ReceiveFrameMode::MaxBytes:
        return QStringLiteral("max_bytes");
    case ReceiveFrameMode::FixedLength:
    default:
        return QStringLiteral("fixed_length");
    }
}

ReceiveFrameMode receiveFrameModeFromString(const QString &text, bool *ok)
{
    const QString t = text.trimmed().toLower();
    auto setOk = [ok](bool v) {
        if (ok)
            *ok = v;
    };
    if (t == QLatin1String("idle_gap") || t == QLatin1String("idle")) {
        setOk(true);
        return ReceiveFrameMode::IdleGap;
    }
    if (t == QLatin1String("terminator") || t == QLatin1String("tail")) {
        setOk(true);
        return ReceiveFrameMode::Terminator;
    }
    if (t == QLatin1String("max_bytes") || t == QLatin1String("max")) {
        setOk(true);
        return ReceiveFrameMode::MaxBytes;
    }
    if (t == QLatin1String("fixed_length") || t == QLatin1String("fixed") || t.isEmpty()) {
        setOk(true);
        return ReceiveFrameMode::FixedLength;
    }
    setOk(false);
    return ReceiveFrameMode::FixedLength;
}

static QJsonObject receiveToJson(const ReceiveFramePolicy &p)
{
    return QJsonObject{
        {QStringLiteral("mode"), receiveFrameModeToString(p.mode)},
        {QStringLiteral("expectMinBytes"), p.expectMinBytes},
        {QStringLiteral("maxBytes"), p.maxBytes},
        {QStringLiteral("idleGapMs"), p.idleGapMs},
        {QStringLiteral("terminatorHex"), QString::fromLatin1(p.terminator.toHex())}};
}

static ReceiveFramePolicy receiveFromJson(const QJsonObject &obj)
{
    ReceiveFramePolicy p;
    p.mode = receiveFrameModeFromString(obj.value(QStringLiteral("mode")).toString());
    p.expectMinBytes = obj.value(QStringLiteral("expectMinBytes")).toInt(1);
    p.maxBytes = obj.value(QStringLiteral("maxBytes")).toInt(65536);
    p.idleGapMs = obj.value(QStringLiteral("idleGapMs")).toInt(50);
    p.terminator = QByteArray::fromHex(obj.value(QStringLiteral("terminatorHex")).toString().toLatin1());
    return p;
}

static QJsonObject tcpToJson(const TcpConfig &c)
{
    return QJsonObject{
        {QStringLiteral("host"), c.host},
        {QStringLiteral("port"), c.port},
        {QStringLiteral("timeoutMs"), c.timeoutMs},
        {QStringLiteral("retries"), c.retries},
        {QStringLiteral("keepAlive"), c.keepAlive},
        {QStringLiteral("receive"), receiveToJson(c.receive)}};
}

static TcpConfig tcpFromJson(const QJsonObject &obj)
{
    TcpConfig c;
    c.host = obj.value(QStringLiteral("host")).toString(QStringLiteral("127.0.0.1"));
    c.port = obj.value(QStringLiteral("port")).toInt(502);
    c.timeoutMs = obj.value(QStringLiteral("timeoutMs")).toInt(1000);
    c.retries = obj.value(QStringLiteral("retries")).toInt(0);
    c.keepAlive = obj.value(QStringLiteral("keepAlive")).toBool(true);
    c.receive = receiveFromJson(obj.value(QStringLiteral("receive")).toObject());
    return c;
}

static QJsonObject serialToJson(const SerialConfig &c)
{
    return QJsonObject{
        {QStringLiteral("portName"), c.portName},
        {QStringLiteral("baudRate"), c.baudRate},
        {QStringLiteral("dataBits"), c.dataBits},
        {QStringLiteral("parity"), c.parity},
        {QStringLiteral("stopBits"), c.stopBits},
        {QStringLiteral("flowControl"), c.flowControl},
        {QStringLiteral("timeoutMs"), c.timeoutMs},
        {QStringLiteral("retries"), c.retries},
        {QStringLiteral("interByteTimeoutMs"), c.interByteTimeoutMs},
        {QStringLiteral("receive"), receiveToJson(c.receive)}};
}

static SerialConfig serialFromJson(const QJsonObject &obj)
{
    SerialConfig c;
    c.portName = obj.value(QStringLiteral("portName")).toString(QStringLiteral("COM1"));
    c.baudRate = obj.value(QStringLiteral("baudRate")).toInt(9600);
    c.dataBits = obj.value(QStringLiteral("dataBits")).toInt(8);
    c.parity = obj.value(QStringLiteral("parity")).toString(QStringLiteral("none"));
    c.stopBits = obj.value(QStringLiteral("stopBits")).toString(QStringLiteral("1"));
    c.flowControl = obj.value(QStringLiteral("flowControl")).toString(QStringLiteral("none"));
    c.timeoutMs = obj.value(QStringLiteral("timeoutMs")).toInt(1000);
    c.retries = obj.value(QStringLiteral("retries")).toInt(0);
    c.interByteTimeoutMs = obj.value(QStringLiteral("interByteTimeoutMs")).toInt(50);
    c.receive = receiveFromJson(obj.value(QStringLiteral("receive")).toObject());
    return c;
}

QJsonObject CommunicationProfile::toJson() const
{
    return QJsonObject{
        {QStringLiteral("id"), id},
        {QStringLiteral("displayName"), displayName},
        {QStringLiteral("kind"), transportKindToString(kind)},
        {QStringLiteral("tcp"), tcpToJson(tcp)},
        {QStringLiteral("serial"), serialToJson(serial)},
        {QStringLiteral("modbus"),
         QJsonObject{{QStringLiteral("transport"), transportKindToString(modbus.transport)},
                     {QStringLiteral("unitId"), modbus.unitId},
                     {QStringLiteral("timeoutMs"), modbus.timeoutMs},
                     {QStringLiteral("retries"), modbus.retries},
                     {QStringLiteral("tcp"), tcpToJson(modbus.tcp)},
                     {QStringLiteral("serial"), serialToJson(modbus.serial)}}},
        {QStringLiteral("autoReconnect"), autoReconnect},
        {QStringLiteral("reconnectDelayMs"), reconnectDelayMs}};
}

CommunicationProfile CommunicationProfile::fromJson(const QJsonObject &obj)
{
    CommunicationProfile p;
    p.id = obj.value(QStringLiteral("id")).toString();
    p.displayName = obj.value(QStringLiteral("displayName")).toString(p.id);
    p.kind = transportKindFromString(obj.value(QStringLiteral("kind")).toString());
    p.tcp = tcpFromJson(obj.value(QStringLiteral("tcp")).toObject());
    p.serial = serialFromJson(obj.value(QStringLiteral("serial")).toObject());
    const QJsonObject mb = obj.value(QStringLiteral("modbus")).toObject();
    p.modbus.transport = transportKindFromString(mb.value(QStringLiteral("transport")).toString(
        QStringLiteral("modbus_tcp")));
    p.modbus.unitId = mb.value(QStringLiteral("unitId")).toInt(1);
    p.modbus.timeoutMs = mb.value(QStringLiteral("timeoutMs")).toInt(1000);
    p.modbus.retries = mb.value(QStringLiteral("retries")).toInt(0);
    p.modbus.tcp = tcpFromJson(mb.value(QStringLiteral("tcp")).toObject());
    p.modbus.serial = serialFromJson(mb.value(QStringLiteral("serial")).toObject());
    p.autoReconnect = obj.value(QStringLiteral("autoReconnect")).toBool(true);
    p.reconnectDelayMs = obj.value(QStringLiteral("reconnectDelayMs")).toInt(500);
    return p;
}

} // namespace Communication
} // namespace Selt
