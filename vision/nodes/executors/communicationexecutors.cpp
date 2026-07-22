#include "communication/clients.h"
#include "communication/codec.h"
#include "communication/communicationmanager.h"
#include "communication/crc.h"
#include "communication/mocktransport.h"
#include "vision/nodes/imageexecutorhelpers.h"
#include "vision/registry/communicationnodeids.h"
#include "vision/registry/visionnodeids.h"
#include "vision/runtime/nodeexecutorregistry.h"

#include <QElapsedTimer>
#include <cmath>
#include <memory>

namespace Selt {
namespace {

using namespace Communication;

PayloadEncoding parseEncoding(const QString &text)
{
    const QString t = text.toLower();
    if (t == QLatin1String("ascii"))
        return PayloadEncoding::Ascii;
    if (t == QLatin1String("hex"))
        return PayloadEncoding::Hex;
    return PayloadEncoding::Utf8;
}

ModbusWordOrder parseWordOrder(const QString &text)
{
    if (text == QLatin1String("CDAB"))
        return ModbusWordOrder::CDAB;
    if (text == QLatin1String("BADC"))
        return ModbusWordOrder::BADC;
    if (text == QLatin1String("DCBA"))
        return ModbusWordOrder::DCBA;
    return ModbusWordOrder::ABCD;
}

RegisterValueKind parseValueKind(const QString &text)
{
    const QString t = text.toLower();
    if (t == QLatin1String("int16"))
        return RegisterValueKind::Int16;
    if (t == QLatin1String("uint32"))
        return RegisterValueKind::UInt32;
    if (t == QLatin1String("int32"))
        return RegisterValueKind::Int32;
    if (t == QLatin1String("float32"))
        return RegisterValueKind::Float32;
    if (t == QLatin1String("float64"))
        return RegisterValueKind::Float64;
    if (t == QLatin1String("ascii"))
        return RegisterValueKind::AsciiString;
    if (t == QLatin1String("hex"))
        return RegisterValueKind::HexString;
    return RegisterValueKind::UInt16;
}

ModbusTable parseTable(const QString &text)
{
    const QString t = text.toLower();
    if (t == QLatin1String("coils") || t == QLatin1String("coil"))
        return ModbusTable::Coils;
    if (t == QLatin1String("discrete"))
        return ModbusTable::DiscreteInputs;
    if (t == QLatin1String("input"))
        return ModbusTable::InputRegisters;
    return ModbusTable::HoldingRegisters;
}

ExecutionResult failResult(const CommunicationDiagnostic &diag, qint64 elapsedMs)
{
    ExecutionResult r;
    r.status = ModuleStatus::Failed;
    r.errorMessage = diag.message;
    r.diagnosticCode = diag.code;
    r.outputs.insert(QStringLiteral("ok"), DataValue(false));
    r.outputs.insert(QStringLiteral("error"), DataValue(diag.message));
    r.elapsedMs = elapsedMs;
    return r;
}

QByteArray resolvePayload(const ExecutionRequest &request, PayloadEncoding encoding,
                          CommunicationDiagnostic *diag)
{
    if (request.inputs.contains(QStringLiteral("payload"))
        && !request.inputs.value(QStringLiteral("payload")).isNull()) {
        const DataValue v = request.inputs.value(QStringLiteral("payload"));
        if (v.type() == DataTypeId::ByteArray)
            return v.toByteArray();
        auto encoded = Codec::bytesFromText(v.toString(), encoding);
        if (!encoded.diagnostic.ok && diag)
            *diag = encoded.diagnostic;
        return encoded.bytes;
    }
    const QString text = resolveStringParam(request, QStringLiteral("payloadText"), {});
    auto encoded = Codec::bytesFromText(text, encoding);
    if (!encoded.diagnostic.ok && diag)
        *diag = encoded.diagnostic;
    return encoded.bytes;
}

std::unique_ptr<ITransport> makeEchoMock()
{
    return std::make_unique<MockTransport>();
}

std::shared_ptr<MockModbusMap> sharedMockMap()
{
    static std::shared_ptr<MockModbusMap> map = std::make_shared<MockModbusMap>();
    return map;
}

std::unique_ptr<ITransport> makeModbusMock(TransportKind kind)
{
    auto map = sharedMockMap();
    if (kind == TransportKind::ModbusRtu) {
        return std::make_unique<MockTransport>([map](const QByteArray &req) {
            QString crcErr;
            if (!ModbusCrc::validate(req, &crcErr))
                return QByteArray();
            const quint8 unit = uchar(req.at(0));
            const QByteArray pdu = req.mid(1, req.size() - 3);
            QByteArray resp;
            resp.append(char(unit));
            resp.append(map->handlePdu(unit, pdu));
            return ModbusCrc::append(resp);
        });
    }
    return std::make_unique<MockTransport>(
        [map](const QByteArray &req) { return map->handleTcpPdu(req); });
}

ReceiveFramePolicy resolveFramePolicy(const ExecutionRequest &request, int defaultExpectMin)
{
    ReceiveFramePolicy policy;
    policy.mode = receiveFrameModeFromString(
        resolveStringParam(request, QStringLiteral("frameMode"), QStringLiteral("fixed_length")));
    policy.expectMinBytes = resolveIntParam(request, QStringLiteral("expectMinBytes"), defaultExpectMin);
    policy.maxBytes = resolveIntParam(request, QStringLiteral("maxBytes"), 65536);
    policy.idleGapMs = resolveIntParam(request, QStringLiteral("idleGapMs"), 50);
    const QString termHex = resolveStringParam(request, QStringLiteral("terminatorHex"), {});
    if (!termHex.trimmed().isEmpty()) {
        auto enc = Codec::hexToBytes(termHex);
        if (enc.diagnostic.ok)
            policy.terminator = enc.bytes;
    }
    return policy;
}

void ensureProfileFromRequest(const QString &connectionId, TransportKind kind,
                              const ExecutionRequest &request)
{
    if (connectionId.isEmpty())
        return;
    CommunicationProfile profile = CommunicationManager::instance().profile(connectionId);
    if (profile.id.isEmpty()) {
        profile.id = connectionId;
        profile.displayName = connectionId;
    }
    profile.kind = kind;
    if (kind == TransportKind::Serial || kind == TransportKind::ModbusRtu) {
        profile.serial.portName =
            resolveStringParam(request, QStringLiteral("portName"),
                               profile.serial.portName.isEmpty() ? QStringLiteral("COM1")
                                                                 : profile.serial.portName);
        profile.serial.baudRate =
            resolveIntParam(request, QStringLiteral("baudRate"),
                            profile.serial.baudRate > 0 ? profile.serial.baudRate : 9600);
        profile.serial.timeoutMs =
            resolveIntParam(request, QStringLiteral("timeoutMs"),
                            profile.serial.timeoutMs > 0 ? profile.serial.timeoutMs : 1000);
        profile.serial.retries =
            resolveIntParam(request, QStringLiteral("retries"), profile.serial.retries);
        profile.serial.receive = resolveFramePolicy(request, 1);
    }
    if (kind == TransportKind::Tcp || kind == TransportKind::ModbusTcp) {
        profile.tcp.host = resolveStringParam(
            request, QStringLiteral("host"),
            profile.tcp.host.isEmpty() ? QStringLiteral("127.0.0.1") : profile.tcp.host);
        profile.tcp.port =
            resolveIntParam(request, QStringLiteral("port"), profile.tcp.port > 0 ? profile.tcp.port : 502);
        profile.tcp.timeoutMs =
            resolveIntParam(request, QStringLiteral("timeoutMs"),
                            profile.tcp.timeoutMs > 0 ? profile.tcp.timeoutMs : 1000);
        profile.tcp.retries =
            resolveIntParam(request, QStringLiteral("retries"), profile.tcp.retries);
        profile.tcp.receive = resolveFramePolicy(request, 1);
    }
    if (kind == TransportKind::ModbusTcp || kind == TransportKind::ModbusRtu) {
        profile.modbus.transport = kind;
        profile.modbus.unitId =
            resolveIntParam(request, QStringLiteral("unitId"),
                            profile.modbus.unitId > 0 ? profile.modbus.unitId : 1);
        profile.modbus.timeoutMs =
            resolveIntParam(request, QStringLiteral("timeoutMs"),
                            profile.modbus.timeoutMs > 0 ? profile.modbus.timeoutMs : 1000);
        profile.modbus.tcp = profile.tcp;
        profile.modbus.serial = profile.serial;
    }
    CommunicationManager::instance().upsertProfile(profile);
}

class SerialRequestExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return CommunicationNodeIds::serialRequest(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &ctx) override
    {
        QElapsedTimer t;
        t.start();
        const auto encoding = parseEncoding(resolveStringParam(request, QStringLiteral("encoding"),
                                                               QStringLiteral("hex")));
        CommunicationDiagnostic encodeDiag;
        const QByteArray payload = resolvePayload(request, encoding, &encodeDiag);
        if (!encodeDiag.ok)
            return failResult(encodeDiag, t.elapsed());

        const bool useMock = resolveBoolParam(request, QStringLiteral("useMock"), true);
        const ReceiveFramePolicy policy = resolveFramePolicy(request, 1);
        const QString connectionId =
            resolveStringParam(request, QStringLiteral("connectionId"), {});

        RequestResult result;
        if (!connectionId.isEmpty()) {
            ensureProfileFromRequest(connectionId, TransportKind::Serial, request);
            result = CommunicationManager::instance().request(connectionId, payload, policy, useMock,
                                                              ctx.cancellation());
        } else {
            SerialConfig cfg;
            cfg.portName = resolveStringParam(request, QStringLiteral("portName"), QStringLiteral("COM1"));
            cfg.baudRate = resolveIntParam(request, QStringLiteral("baudRate"), 9600);
            cfg.timeoutMs = resolveIntParam(request, QStringLiteral("timeoutMs"), 1000);
            cfg.retries = resolveIntParam(request, QStringLiteral("retries"), 0);
            cfg.receive = policy;
            SerialClient client(useMock ? makeEchoMock() : nullptr);
            client.setConfig(cfg);
            result = client.request(payload, policy);
        }
        if (!result.diagnostic.ok)
            return failResult(result.diagnostic, t.elapsed());

        ExecutionResult r;
        r.outputs.insert(QStringLiteral("response"), DataValue(result.response));
        r.outputs.insert(QStringLiteral("ok"), DataValue(true));
        r.outputs.insert(QStringLiteral("error"), DataValue(QString()));
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class TcpRequestExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return CommunicationNodeIds::tcpRequest(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &ctx) override
    {
        QElapsedTimer t;
        t.start();
        const auto encoding = parseEncoding(resolveStringParam(request, QStringLiteral("encoding"),
                                                               QStringLiteral("hex")));
        CommunicationDiagnostic encodeDiag;
        const QByteArray payload = resolvePayload(request, encoding, &encodeDiag);
        if (!encodeDiag.ok)
            return failResult(encodeDiag, t.elapsed());

        const bool useMock = resolveBoolParam(request, QStringLiteral("useMock"), true);
        const ReceiveFramePolicy policy = resolveFramePolicy(request, 1);
        const QString connectionId =
            resolveStringParam(request, QStringLiteral("connectionId"), {});

        RequestResult result;
        if (!connectionId.isEmpty()) {
            ensureProfileFromRequest(connectionId, TransportKind::Tcp, request);
            result = CommunicationManager::instance().request(connectionId, payload, policy, useMock,
                                                              ctx.cancellation());
        } else {
            TcpConfig cfg;
            cfg.host = resolveStringParam(request, QStringLiteral("host"), QStringLiteral("127.0.0.1"));
            cfg.port = resolveIntParam(request, QStringLiteral("port"), 502);
            cfg.timeoutMs = resolveIntParam(request, QStringLiteral("timeoutMs"), 1000);
            cfg.retries = resolveIntParam(request, QStringLiteral("retries"), 0);
            cfg.receive = policy;
            TcpClient client(useMock ? makeEchoMock() : nullptr);
            client.setConfig(cfg);
            result = client.request(payload, policy);
        }
        if (!result.diagnostic.ok)
            return failResult(result.diagnostic, t.elapsed());

        ExecutionResult r;
        r.outputs.insert(QStringLiteral("response"), DataValue(result.response));
        r.outputs.insert(QStringLiteral("ok"), DataValue(true));
        r.outputs.insert(QStringLiteral("error"), DataValue(QString()));
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

ModbusConfig buildModbusConfig(const ExecutionRequest &request)
{
    ModbusConfig cfg;
    const QString transport = resolveStringParam(request, QStringLiteral("transport"),
                                                 QStringLiteral("tcp"));
    cfg.transport = (transport == QLatin1String("rtu")) ? TransportKind::ModbusRtu
                                                        : TransportKind::ModbusTcp;
    cfg.tcp.host = resolveStringParam(request, QStringLiteral("host"), QStringLiteral("127.0.0.1"));
    cfg.tcp.port = resolveIntParam(request, QStringLiteral("port"), 502);
    cfg.serial.portName = resolveStringParam(request, QStringLiteral("portName"), QStringLiteral("COM1"));
    cfg.unitId = resolveIntParam(request, QStringLiteral("unitId"), 1);
    cfg.timeoutMs = resolveIntParam(request, QStringLiteral("timeoutMs"), 1000);
    return cfg;
}

class ModbusReadExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return CommunicationNodeIds::modbusRead(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &ctx) override
    {
        QElapsedTimer t;
        t.start();
        ModbusConfig cfg = buildModbusConfig(request);
        const bool useMock = resolveBoolParam(request, QStringLiteral("useMock"), true);
        const QString connectionId =
            resolveStringParam(request, QStringLiteral("connectionId"), {});

        const ModbusTable table = parseTable(resolveStringParam(request, QStringLiteral("table"),
                                                                QStringLiteral("holding")));
        const int address = resolveIntParam(request, QStringLiteral("address"), 0);
        const int quantity = resolveIntParam(request, QStringLiteral("quantity"), 1);
        const QString kindText = resolveStringParam(request, QStringLiteral("valueKind"),
                                                    QStringLiteral("uint16"));
        const auto order = parseWordOrder(resolveStringParam(request, QStringLiteral("wordOrder"),
                                                             QStringLiteral("ABCD")));

        ModbusValueResult raw;
        if (!connectionId.isEmpty()) {
            ensureProfileFromRequest(connectionId,
                                     cfg.transport == TransportKind::ModbusRtu
                                         ? TransportKind::ModbusRtu
                                         : TransportKind::ModbusTcp,
                                     request);
            if (table == ModbusTable::Coils || table == ModbusTable::DiscreteInputs)
                raw = CommunicationManager::instance().modbusReadBits(
                    connectionId, table, address, quantity, useMock, ctx.cancellation());
            else
                raw = CommunicationManager::instance().modbusReadRegisters(
                    connectionId, table, address, quantity, useMock, ctx.cancellation());
        } else {
            ModbusClient client(useMock ? makeModbusMock(cfg.transport) : nullptr);
            client.setConfig(cfg);
            if (table == ModbusTable::Coils || table == ModbusTable::DiscreteInputs)
                raw = client.readBits(table, address, quantity);
            else
                raw = client.readRegisters(table, address, quantity);
        }
        if (!raw.diagnostic.ok)
            return failResult(raw.diagnostic, t.elapsed());

        ExecutionResult r;
        if (kindText == QLatin1String("coil") || table == ModbusTable::Coils
            || table == ModbusTable::DiscreteInputs) {
            const bool bit = !raw.coils.isEmpty() && raw.coils.first();
            r.outputs.insert(QStringLiteral("value"), DataValue(bit ? 1.0 : 0.0));
            r.outputs.insert(QStringLiteral("text"),
                             DataValue(bit ? QStringLiteral("1") : QStringLiteral("0")));
        } else {
            auto decoded = Codec::decodeValue(raw.registers, parseValueKind(kindText), order);
            if (!decoded.diagnostic.ok)
                return failResult(decoded.diagnostic, t.elapsed());
            r.outputs.insert(QStringLiteral("value"), DataValue(decoded.realValue));
            r.outputs.insert(QStringLiteral("text"), DataValue(decoded.text));
        }
        r.outputs.insert(QStringLiteral("ok"), DataValue(true));
        r.outputs.insert(QStringLiteral("error"), DataValue(QString()));
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class ModbusWriteExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return CommunicationNodeIds::modbusWrite(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &ctx) override
    {
        QElapsedTimer t;
        t.start();
        const bool allowWrite = resolveBoolParam(request, QStringLiteral("allowWrite"), false);
        if (!allowWrite) {
            return failResult(CommunicationDiagnostic::fail(
                                  Codes::writeProtected(),
                                  QStringLiteral("写操作被保护，请将 allowWrite 设为 true"),
                                  {QStringLiteral("避免误写 PLC")}),
                              t.elapsed());
        }

        ModbusConfig cfg = buildModbusConfig(request);
        const bool useMock = resolveBoolParam(request, QStringLiteral("useMock"), true);
        const QString connectionId =
            resolveStringParam(request, QStringLiteral("connectionId"), {});
        const TransportKind profileKind = cfg.transport == TransportKind::ModbusRtu
                                              ? TransportKind::ModbusRtu
                                              : TransportKind::ModbusTcp;

        const ModbusTable table = parseTable(resolveStringParam(request, QStringLiteral("table"),
                                                                QStringLiteral("holding")));
        const int address = resolveIntParam(request, QStringLiteral("address"), 0);
        const QString kindText = resolveStringParam(request, QStringLiteral("valueKind"),
                                                    QStringLiteral("uint16"));
        const auto order = parseWordOrder(resolveStringParam(request, QStringLiteral("wordOrder"),
                                                             QStringLiteral("ABCD")));
        double value = resolveRealInput(request, QStringLiteral("value"), 0.0);
        if (request.inputs.contains(QStringLiteral("okIn"))) {
            value = request.inputs.value(QStringLiteral("okIn")).toBool(false) ? 1.0 : 0.0;
        } else if (request.inputs.contains(QStringLiteral("value"))) {
            const DataValue rawValue = request.inputs.value(QStringLiteral("value"));
            if (rawValue.type() == DataTypeId::Bool)
                value = rawValue.toBool(false) ? 1.0 : 0.0;
            else
                value = rawValue.toReal(value);
        }
        const QString text = request.inputs.contains(QStringLiteral("text"))
                                 ? request.inputs.value(QStringLiteral("text")).toString()
                                 : resolveStringParam(request, QStringLiteral("text"), {});

        ModbusValueResult raw;
        auto writeCoilFn = [&](bool bit) {
            if (!connectionId.isEmpty()) {
                ensureProfileFromRequest(connectionId, profileKind, request);
                return CommunicationManager::instance().modbusWriteCoil(
                    connectionId, address, bit, true, useMock, ctx.cancellation());
            }
            ModbusClient client(useMock ? makeModbusMock(cfg.transport) : nullptr);
            client.setConfig(cfg);
            return client.writeCoil(address, bit, true);
        };
        auto writeRegsFn = [&](const QVector<quint16> &regs) {
            if (!connectionId.isEmpty()) {
                ensureProfileFromRequest(connectionId, profileKind, request);
                return CommunicationManager::instance().modbusWriteRegisters(
                    connectionId, address, regs, true, useMock, ctx.cancellation());
            }
            ModbusClient client(useMock ? makeModbusMock(cfg.transport) : nullptr);
            client.setConfig(cfg);
            return client.writeRegisters(address, regs, true);
        };

        if (table == ModbusTable::Coils || kindText == QLatin1String("coil")) {
            raw = writeCoilFn(value != 0.0 || text == QLatin1String("1"));
        } else if (kindText == QLatin1String("ascii") || kindText == QLatin1String("hex")) {
            const int regs = resolveIntParam(request, QStringLiteral("stringRegisters"), 4);
            auto encoded = Codec::encodeString(text, parseValueKind(kindText), order, regs, true);
            if (!encoded.diagnostic.ok)
                return failResult(encoded.diagnostic, t.elapsed());
            raw = writeRegsFn(encoded.registers);
        } else {
            auto encoded = Codec::encodeValue(value, parseValueKind(kindText), order);
            if (!encoded.diagnostic.ok)
                return failResult(encoded.diagnostic, t.elapsed());
            raw = writeRegsFn(encoded.registers);
        }
        if (!raw.diagnostic.ok)
            return failResult(raw.diagnostic, t.elapsed());

        ExecutionResult r;
        r.outputs.insert(QStringLiteral("ok"), DataValue(true));
        r.outputs.insert(QStringLiteral("error"), DataValue(QString()));
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class EncodeExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return CommunicationNodeIds::encode(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        QElapsedTimer t;
        t.start();
        const QString kindText = resolveStringParam(request, QStringLiteral("valueKind"),
                                                    QStringLiteral("float32"));
        const auto order = parseWordOrder(resolveStringParam(request, QStringLiteral("wordOrder"),
                                                             QStringLiteral("ABCD")));
        const double value = request.inputs.contains(QStringLiteral("value"))
                                 ? request.inputs.value(QStringLiteral("value")).toReal(0.0)
                                 : resolveRealInput(request, QStringLiteral("value"), 0.0);
        const QString text = request.inputs.contains(QStringLiteral("text"))
                                 ? request.inputs.value(QStringLiteral("text")).toString()
                                 : resolveStringParam(request, QStringLiteral("text"), {});

        CodecResult encoded;
        if (kindText == QLatin1String("decimal_ascii")) {
            encoded = Codec::decimalToAscii(qint64(qRound64(value)));
        } else if (kindText == QLatin1String("ascii") || kindText == QLatin1String("hex")) {
            const int regs = resolveIntParam(request, QStringLiteral("stringRegisters"), 4);
            encoded = Codec::encodeString(text, parseValueKind(kindText), order, regs, true);
        } else {
            encoded = Codec::encodeValue(value, parseValueKind(kindText), order);
        }
        if (!encoded.diagnostic.ok)
            return failResult(encoded.diagnostic, t.elapsed());

        ExecutionResult r;
        r.outputs.insert(QStringLiteral("bytes"), DataValue(encoded.bytes));
        r.outputs.insert(QStringLiteral("ok"), DataValue(true));
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class DecodeExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return CommunicationNodeIds::decode(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        QElapsedTimer t;
        t.start();
        QByteArray bytes = request.inputs.value(QStringLiteral("bytes")).toByteArray();
        if (bytes.isEmpty()) {
            const QString hex = resolveStringParam(request, QStringLiteral("hexText"), {});
            auto parsed = Codec::hexToBytes(hex);
            if (!parsed.diagnostic.ok)
                return failResult(parsed.diagnostic, t.elapsed());
            bytes = parsed.bytes;
        }
        const QString kindText = resolveStringParam(request, QStringLiteral("valueKind"),
                                                    QStringLiteral("float32"));
        const auto order = parseWordOrder(resolveStringParam(request, QStringLiteral("wordOrder"),
                                                             QStringLiteral("ABCD")));

        CodecResult decoded;
        if (kindText == QLatin1String("decimal_ascii")) {
            decoded = Codec::asciiToDecimal(QString::fromLatin1(bytes));
            decoded.realValue = double(decoded.intValue);
        } else if (kindText == QLatin1String("hex") && bytes.size() % 2 != 0) {
            decoded = Codec::bytesToHex(bytes, true);
        } else {
            auto regs = Codec::unpackRegisters(bytes);
            if (!regs.diagnostic.ok)
                return failResult(regs.diagnostic, t.elapsed());
            decoded = Codec::decodeValue(regs.registers, parseValueKind(kindText), order);
        }
        if (!decoded.diagnostic.ok)
            return failResult(decoded.diagnostic, t.elapsed());

        ExecutionResult r;
        r.outputs.insert(QStringLiteral("value"), DataValue(decoded.realValue));
        r.outputs.insert(QStringLiteral("text"), DataValue(decoded.text));
        r.outputs.insert(QStringLiteral("ok"), DataValue(true));
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class NumericExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return CommunicationNodeIds::numeric(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        QElapsedTimer t;
        t.start();
        const double a = request.inputs.contains(QStringLiteral("a"))
                             ? request.inputs.value(QStringLiteral("a")).toReal(0.0)
                             : resolveRealInput(request, QStringLiteral("a"), 0.0);
        const double b = request.inputs.contains(QStringLiteral("b"))
                             ? request.inputs.value(QStringLiteral("b")).toReal(0.0)
                             : resolveRealInput(request, QStringLiteral("b"), 0.0);
        const QString op = resolveStringParam(request, QStringLiteral("op"), QStringLiteral("add"));
        const double scale = resolveRealInput(request, QStringLiteral("scale"), 1.0);
        const double offset = resolveRealInput(request, QStringLiteral("offset"), 0.0);
        double out = 0.0;
        bool ok = true;
        if (op == QLatin1String("sub"))
            out = a - b;
        else if (op == QLatin1String("mul"))
            out = a * b;
        else if (op == QLatin1String("div")) {
            if (std::abs(b) < 1e-15) {
                ok = false;
            } else {
                out = a / b;
            }
        } else if (op == QLatin1String("scale"))
            out = a * scale + offset;
        else
            out = a + b;

        ExecutionResult r;
        if (!ok) {
            r.status = ModuleStatus::Failed;
            r.diagnosticCode = DiagnosticCodes::commInvalidParameter();
            r.errorMessage = QStringLiteral("除零");
            r.outputs.insert(QStringLiteral("ok"), DataValue(false));
        } else {
            r.status = ModuleStatus::Success;
            r.outputs.insert(QStringLiteral("out"), DataValue(out));
            r.outputs.insert(QStringLiteral("ok"), DataValue(true));
        }
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class StringOpExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return CommunicationNodeIds::stringOp(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        QElapsedTimer t;
        t.start();
        const QString a = request.inputs.contains(QStringLiteral("a"))
                              ? request.inputs.value(QStringLiteral("a")).toString()
                              : resolveStringParam(request, QStringLiteral("a"), {});
        const QString b = request.inputs.contains(QStringLiteral("b"))
                              ? request.inputs.value(QStringLiteral("b")).toString()
                              : resolveStringParam(request, QStringLiteral("b"), {});
        const QString op = resolveStringParam(request, QStringLiteral("op"), QStringLiteral("concat"));
        const QString fmt = resolveStringParam(request, QStringLiteral("format"),
                                               QStringLiteral("{0}{1}"));
        const double number = resolveRealInput(request, QStringLiteral("number"), 0.0);

        QString out;
        QByteArray bytes;
        if (op == QLatin1String("concat")) {
            out = a + b;
            bytes = out.toUtf8();
        } else if (op == QLatin1String("format")) {
            out = fmt;
            out.replace(QStringLiteral("{0}"), a);
            out.replace(QStringLiteral("{1}"), b);
            out.replace(QStringLiteral("{n}"), QString::number(number));
            bytes = out.toUtf8();
        } else if (op == QLatin1String("decimal_to_ascii")) {
            auto enc = Codec::decimalToAscii(qint64(qRound64(number)));
            out = enc.text;
            bytes = enc.bytes;
        } else if (op == QLatin1String("ascii_to_decimal")) {
            auto dec = Codec::asciiToDecimal(a.isEmpty() ? b : a);
            if (!dec.diagnostic.ok)
                return failResult(dec.diagnostic, t.elapsed());
            out = QString::number(dec.intValue);
            bytes = out.toLatin1();
        } else if (op == QLatin1String("to_hex")) {
            auto hex = Codec::bytesToHex((a.isEmpty() ? b : a).toUtf8(), true);
            out = hex.text;
            bytes = hex.bytes;
        } else if (op == QLatin1String("from_hex")) {
            auto parsed = Codec::hexToBytes(a.isEmpty() ? b : a);
            if (!parsed.diagnostic.ok)
                return failResult(parsed.diagnostic, t.elapsed());
            bytes = parsed.bytes;
            out = QString::fromUtf8(bytes);
        } else {
            return failResult(CommunicationDiagnostic::fail(Codes::invalidParameter(),
                                                            QStringLiteral("未知字符串操作")),
                              t.elapsed());
        }

        ExecutionResult r;
        r.outputs.insert(QStringLiteral("out"), DataValue(out));
        r.outputs.insert(QStringLiteral("bytes"), DataValue(bytes));
        r.outputs.insert(QStringLiteral("ok"), DataValue(true));
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class ChecksumExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return CommunicationNodeIds::checksum(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        QElapsedTimer t;
        t.start();
        QByteArray bytes = request.inputs.value(QStringLiteral("bytes")).toByteArray();
        if (bytes.isEmpty()) {
            auto parsed = Codec::hexToBytes(resolveStringParam(request, QStringLiteral("hexText"), {}));
            if (!parsed.diagnostic.ok)
                return failResult(parsed.diagnostic, t.elapsed());
            bytes = parsed.bytes;
        }
        const QString algo = resolveStringParam(request, QStringLiteral("algo"), QStringLiteral("crc16"));
        quint32 value = 0;
        QByteArray appended;
        if (algo == QLatin1String("lrc")) {
            value = Checksum::lrc(bytes);
            appended.append(char(value & 0xFF));
        } else if (algo == QLatin1String("xor")) {
            value = Checksum::xorSum(bytes);
            appended.append(char(value & 0xFF));
        } else if (algo == QLatin1String("sum16")) {
            value = Checksum::sum16(bytes);
            appended.append(char((value >> 8) & 0xFF));
            appended.append(char(value & 0xFF));
        } else {
            value = Checksum::crc16Modbus(bytes);
            appended.clear();
            const quint16 crc = quint16(value);
            appended.append(char(crc & 0xFF));
            appended.append(char((crc >> 8) & 0xFF));
        }
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("value"), DataValue(int(value)));
        r.outputs.insert(QStringLiteral("bytes"), DataValue(bytes + appended));
        r.outputs.insert(QStringLiteral("ok"), DataValue(true));
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class FrameWrapExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return CommunicationNodeIds::frameWrap(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        QElapsedTimer t;
        t.start();
        QByteArray body = request.inputs.value(QStringLiteral("bytes")).toByteArray();
        if (body.isEmpty()) {
            auto parsed = Codec::bytesFromText(resolveStringParam(request, QStringLiteral("payloadText"), {}),
                                               parseEncoding(resolveStringParam(request, QStringLiteral("encoding"),
                                                                                QStringLiteral("hex"))));
            if (!parsed.diagnostic.ok)
                return failResult(parsed.diagnostic, t.elapsed());
            body = parsed.bytes;
        }
        auto head = Codec::hexToBytes(resolveStringParam(request, QStringLiteral("headHex"), {}));
        auto tail = Codec::hexToBytes(resolveStringParam(request, QStringLiteral("tailHex"), {}));
        if (!head.diagnostic.ok)
            return failResult(head.diagnostic, t.elapsed());
        if (!tail.diagnostic.ok)
            return failResult(tail.diagnostic, t.elapsed());
        const bool appendCrc = resolveBoolParam(request, QStringLiteral("appendCrc16"), false);
        QByteArray out = head.bytes + body;
        if (appendCrc)
            out = ModbusCrc::append(out);
        out += tail.bytes;
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("out"), DataValue(out));
        r.outputs.insert(QStringLiteral("bytes"), DataValue(out));
        r.outputs.insert(QStringLiteral("length"), DataValue(int(out.size())));
        r.outputs.insert(QStringLiteral("ok"), DataValue(true));
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class ClampScaleExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return CommunicationNodeIds::clampScale(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        QElapsedTimer t;
        t.start();
        double v = resolveRealInput(request, QStringLiteral("value"), 0.0);
        const double scale = resolveRealInput(request, QStringLiteral("scale"), 1.0);
        const double offset = resolveRealInput(request, QStringLiteral("offset"), 0.0);
        const double lo = resolveRealInput(request, QStringLiteral("min"), -1e18);
        const double hi = resolveRealInput(request, QStringLiteral("max"), 1e18);
        v = v * scale + offset;
        if (v < lo)
            v = lo;
        if (v > hi)
            v = hi;
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("out"), DataValue(v));
        r.outputs.insert(QStringLiteral("ok"), DataValue(true));
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class RegisterTableExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return CommunicationNodeIds::registerTable(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        QElapsedTimer t;
        t.start();
        QByteArray bytes = request.inputs.value(QStringLiteral("bytes")).toByteArray();
        if (bytes.isEmpty()) {
            auto parsed = Codec::hexToBytes(resolveStringParam(request, QStringLiteral("hexText"), {}));
            if (!parsed.diagnostic.ok)
                return failResult(parsed.diagnostic, t.elapsed());
            bytes = parsed.bytes;
        }
        auto regs = Codec::unpackRegisters(bytes);
        if (!regs.diagnostic.ok)
            return failResult(regs.diagnostic, t.elapsed());
        TableData table;
        table.columns = {QStringLiteral("index"), QStringLiteral("value")};
        for (int i = 0; i < regs.registers.size(); ++i) {
            QVariantList row;
            row << i << int(regs.registers.at(i));
            table.rows.append(row);
        }
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("table"), DataValue(table));
        r.outputs.insert(QStringLiteral("count"), DataValue(int(regs.registers.size())));
        r.outputs.insert(QStringLiteral("ok"), DataValue(true));
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

} // namespace

void registerCommunicationExecutors(NodeExecutorRegistry &reg)
{
    reg.registerFactory(CommunicationNodeIds::serialRequest(),
                        [] { return std::make_shared<SerialRequestExecutor>(); });
    reg.registerFactory(CommunicationNodeIds::tcpRequest(),
                        [] { return std::make_shared<TcpRequestExecutor>(); });
    reg.registerFactory(CommunicationNodeIds::modbusRead(),
                        [] { return std::make_shared<ModbusReadExecutor>(); });
    reg.registerFactory(CommunicationNodeIds::modbusWrite(),
                        [] { return std::make_shared<ModbusWriteExecutor>(); });
    reg.registerFactory(CommunicationNodeIds::encode(),
                        [] { return std::make_shared<EncodeExecutor>(); });
    reg.registerFactory(CommunicationNodeIds::decode(),
                        [] { return std::make_shared<DecodeExecutor>(); });
    reg.registerFactory(CommunicationNodeIds::numeric(),
                        [] { return std::make_shared<NumericExecutor>(); });
    reg.registerFactory(CommunicationNodeIds::stringOp(),
                        [] { return std::make_shared<StringOpExecutor>(); });
    reg.registerFactory(CommunicationNodeIds::checksum(),
                        [] { return std::make_shared<ChecksumExecutor>(); });
    reg.registerFactory(CommunicationNodeIds::frameWrap(),
                        [] { return std::make_shared<FrameWrapExecutor>(); });
    reg.registerFactory(CommunicationNodeIds::clampScale(),
                        [] { return std::make_shared<ClampScaleExecutor>(); });
    reg.registerFactory(CommunicationNodeIds::registerTable(),
                        [] { return std::make_shared<RegisterTableExecutor>(); });
}

} // namespace Selt
