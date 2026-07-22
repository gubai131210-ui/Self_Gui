#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <cstdint>

namespace Selt {
namespace Communication {

enum class TransportKind {
    Serial,
    Tcp,
    ModbusTcp,
    ModbusRtu
};

enum class PayloadEncoding {
    Raw,
    Hex,
    Ascii,
    Utf8
};

enum class ModbusTable {
    Coils,
    DiscreteInputs,
    HoldingRegisters,
    InputRegisters
};

enum class ModbusWordOrder {
    ABCD, // big-endian words, big-endian bytes (Modbus default style)
    CDAB, // little-endian words, big-endian bytes
    BADC, // big-endian words, little-endian bytes
    DCBA  // little-endian words, little-endian bytes
};

enum class RegisterValueKind {
    UInt16,
    Int16,
    UInt32,
    Int32,
    Float32,
    Float64,
    AsciiString,
    HexString
};

enum class ReceiveFrameMode {
    FixedLength,   // wait until expectMinBytes
    IdleGap,       // collect until inter-byte idle timeout
    Terminator,    // collect until terminator bytes appear
    MaxBytes       // read up to maxBytes within timeout
};

struct ReceiveFramePolicy
{
    ReceiveFrameMode mode{ReceiveFrameMode::FixedLength};
    int expectMinBytes{1};
    int maxBytes{65536};
    int idleGapMs{50};
    QByteArray terminator;
};

struct CommunicationDiagnostic
{
    bool ok{true};
    QString code;
    QString message;
    QStringList hints;

    static CommunicationDiagnostic success()
    {
        return {};
    }

    static CommunicationDiagnostic fail(const QString &code, const QString &message,
                                        const QStringList &hints = {})
    {
        CommunicationDiagnostic d;
        d.ok = false;
        d.code = code;
        d.message = message;
        d.hints = hints;
        return d;
    }
};

struct SerialConfig
{
    QString portName;
    int baudRate{9600};
    int dataBits{8}; // 5..8
    QString parity{QStringLiteral("none")}; // none/even/odd/space/mark
    QString stopBits{QStringLiteral("1")};  // 1/1.5/2
    QString flowControl{QStringLiteral("none")}; // none/hardware/software
    int timeoutMs{1000};
    int retries{0};
    int interByteTimeoutMs{50};
    ReceiveFramePolicy receive;
};

struct TcpConfig
{
    QString host{QStringLiteral("127.0.0.1")};
    int port{502};
    int timeoutMs{1000};
    int retries{0};
    bool keepAlive{true};
    ReceiveFramePolicy receive;
};

struct ModbusConfig
{
    TransportKind transport{TransportKind::ModbusTcp};
    TcpConfig tcp;
    SerialConfig serial;
    int unitId{1};
    int timeoutMs{1000};
    int retries{0};
};

struct CommunicationProfile
{
    QString id;
    QString displayName;
    TransportKind kind{TransportKind::Tcp};
    TcpConfig tcp;
    SerialConfig serial;
    ModbusConfig modbus;
    bool autoReconnect{true};
    int reconnectDelayMs{500};

    QJsonObject toJson() const;
    static CommunicationProfile fromJson(const QJsonObject &obj);
};

struct RequestResult
{
    CommunicationDiagnostic diagnostic;
    QByteArray request;
    QByteArray response;
    qint64 elapsedMs{0};
    QString connectionId;
    QString connectionState;
};

struct ModbusValueResult
{
    CommunicationDiagnostic diagnostic;
    ModbusTable table{ModbusTable::HoldingRegisters};
    int address{0};
    int quantity{0};
    QVector<bool> coils;
    QVector<quint16> registers;
    qint64 elapsedMs{0};
};

namespace Codes {
inline QString invalidParameter() { return QStringLiteral("comm_invalid_parameter"); }
inline QString notAvailable() { return QStringLiteral("comm_not_available"); }
inline QString connectFailed() { return QStringLiteral("comm_connect_failed"); }
inline QString timeout() { return QStringLiteral("comm_timeout"); }
inline QString cancelled() { return QStringLiteral("comm_cancelled"); }
inline QString ioError() { return QStringLiteral("comm_io_error"); }
inline QString protocolError() { return QStringLiteral("comm_protocol_error"); }
inline QString deviceException() { return QStringLiteral("comm_device_exception"); }
inline QString writeProtected() { return QStringLiteral("comm_write_protected"); }
inline QString decodeError() { return QStringLiteral("comm_decode_error"); }
inline QString encodeError() { return QStringLiteral("comm_encode_error"); }
inline QString crcError() { return QStringLiteral("comm_crc_error"); }
inline QString busy() { return QStringLiteral("comm_busy"); }
inline QString reconnecting() { return QStringLiteral("comm_reconnecting"); }
} // namespace Codes

QString transportKindToString(TransportKind kind);
TransportKind transportKindFromString(const QString &text, bool *ok = nullptr);
QString receiveFrameModeToString(ReceiveFrameMode mode);
ReceiveFrameMode receiveFrameModeFromString(const QString &text, bool *ok = nullptr);

} // namespace Communication
} // namespace Selt
