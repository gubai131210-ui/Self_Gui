#include "communication/clients.h"

#include "communication/codec.h"
#include "communication/crc.h"

#include <QElapsedTimer>

#if defined(SELT_HAS_QT_NETWORK) && SELT_HAS_QT_NETWORK
#include <QEventLoop>
#include <QTcpSocket>
#include <QTimer>
#endif

#if defined(SELT_HAS_QT_SERIALPORT) && SELT_HAS_QT_SERIALPORT
#include <QSerialPort>
#endif

namespace Selt {
namespace Communication {
namespace {

class NullTransport final : public ITransport
{
public:
    explicit NullTransport(QString reason)
        : m_reason(std::move(reason))
    {
    }
    QString name() const override { return QStringLiteral("null"); }
    CommunicationDiagnostic open() override
    {
        return CommunicationDiagnostic::fail(Codes::notAvailable(), m_reason,
                                             deploymentRequirements());
    }
    void close() override {}
    bool isOpen() const override { return false; }
    CommunicationDiagnostic writeBytes(const QByteArray &) override
    {
        return open();
    }
    CommunicationDiagnostic readBytes(int, int, int, QByteArray *) override
    {
        return open();
    }

private:
    QString m_reason;
};

#if defined(SELT_HAS_QT_NETWORK) && SELT_HAS_QT_NETWORK
class QtTcpTransport final : public ITransport
{
public:
    explicit QtTcpTransport(TcpConfig config)
        : m_config(std::move(config))
    {
    }
    QString name() const override { return QStringLiteral("tcp"); }
    CommunicationDiagnostic open() override
    {
        if (m_socket.state() == QAbstractSocket::ConnectedState)
            return CommunicationDiagnostic::success();
        m_socket.connectToHost(m_config.host, quint16(m_config.port));
        if (!m_socket.waitForConnected(m_config.timeoutMs)) {
            return CommunicationDiagnostic::fail(
                Codes::connectFailed(),
                QStringLiteral("TCP 连接失败: %1:%2 (%3)")
                    .arg(m_config.host)
                    .arg(m_config.port)
                    .arg(m_socket.errorString()),
                {QStringLiteral("检查主机/端口与防火墙")});
        }
        m_socket.setSocketOption(QAbstractSocket::KeepAliveOption, m_config.keepAlive ? 1 : 0);
        return CommunicationDiagnostic::success();
    }
    void close() override
    {
        m_socket.disconnectFromHost();
        if (m_socket.state() != QAbstractSocket::UnconnectedState)
            m_socket.waitForDisconnected(200);
    }
    bool isOpen() const override
    {
        return m_socket.state() == QAbstractSocket::ConnectedState;
    }
    CommunicationDiagnostic writeBytes(const QByteArray &data) override
    {
        if (!isOpen()) {
            auto d = open();
            if (!d.ok)
                return d;
        }
        const qint64 n = m_socket.write(data);
        if (n != data.size() || !m_socket.waitForBytesWritten(m_config.timeoutMs)) {
            return CommunicationDiagnostic::fail(Codes::ioError(),
                                                 QStringLiteral("TCP 写失败: %1").arg(m_socket.errorString()));
        }
        return CommunicationDiagnostic::success();
    }
    CommunicationDiagnostic readBytes(int minBytes, int maxBytes, int timeoutMs,
                                      QByteArray *out) override
    {
        if (!out)
            return CommunicationDiagnostic::fail(Codes::invalidParameter(), QStringLiteral("输出为空"));
        QElapsedTimer timer;
        timer.start();
        QByteArray buffer;
        while (buffer.size() < minBytes) {
            const int remain = timeoutMs - int(timer.elapsed());
            if (remain <= 0)
                return CommunicationDiagnostic::fail(Codes::timeout(), QStringLiteral("TCP 读超时"));
            if (m_socket.bytesAvailable() <= 0 && !m_socket.waitForReadyRead(remain)) {
                if (buffer.size() >= minBytes)
                    break;
                return CommunicationDiagnostic::fail(Codes::timeout(), QStringLiteral("TCP 读超时"),
                                                     {m_socket.errorString()});
            }
            buffer.append(m_socket.read(maxBytes > 0 ? maxBytes - buffer.size() : 65536));
            if (maxBytes > 0 && buffer.size() >= maxBytes)
                break;
        }
        *out = buffer;
        return CommunicationDiagnostic::success();
    }

private:
    TcpConfig m_config;
    QTcpSocket m_socket;
};
#endif

#if defined(SELT_HAS_QT_SERIALPORT) && SELT_HAS_QT_SERIALPORT
class QtSerialTransport final : public ITransport
{
public:
    explicit QtSerialTransport(SerialConfig config)
        : m_config(std::move(config))
    {
    }
    QString name() const override { return QStringLiteral("serial"); }
    CommunicationDiagnostic open() override
    {
        if (m_port.isOpen())
            return CommunicationDiagnostic::success();
        m_port.setPortName(m_config.portName);
        m_port.setBaudRate(m_config.baudRate);
        switch (m_config.dataBits) {
        case 5:
            m_port.setDataBits(QSerialPort::Data5);
            break;
        case 6:
            m_port.setDataBits(QSerialPort::Data6);
            break;
        case 7:
            m_port.setDataBits(QSerialPort::Data7);
            break;
        default:
            m_port.setDataBits(QSerialPort::Data8);
            break;
        }
        const QString parity = m_config.parity.toLower();
        if (parity == QLatin1String("even"))
            m_port.setParity(QSerialPort::EvenParity);
        else if (parity == QLatin1String("odd"))
            m_port.setParity(QSerialPort::OddParity);
        else if (parity == QLatin1String("space"))
            m_port.setParity(QSerialPort::SpaceParity);
        else if (parity == QLatin1String("mark"))
            m_port.setParity(QSerialPort::MarkParity);
        else
            m_port.setParity(QSerialPort::NoParity);
        if (m_config.stopBits == QLatin1String("2"))
            m_port.setStopBits(QSerialPort::TwoStop);
        else if (m_config.stopBits == QLatin1String("1.5"))
            m_port.setStopBits(QSerialPort::OneAndHalfStop);
        else
            m_port.setStopBits(QSerialPort::OneStop);
        if (m_config.flowControl == QLatin1String("hardware"))
            m_port.setFlowControl(QSerialPort::HardwareControl);
        else if (m_config.flowControl == QLatin1String("software"))
            m_port.setFlowControl(QSerialPort::SoftwareControl);
        else
            m_port.setFlowControl(QSerialPort::NoFlowControl);
        if (!m_port.open(QIODevice::ReadWrite)) {
            return CommunicationDiagnostic::fail(
                Codes::connectFailed(),
                QStringLiteral("串口打开失败: %1 (%2)").arg(m_config.portName, m_port.errorString()),
                {QStringLiteral("确认端口名、驱动权限与未被占用")});
        }
        return CommunicationDiagnostic::success();
    }
    void close() override
    {
        if (m_port.isOpen())
            m_port.close();
    }
    bool isOpen() const override { return m_port.isOpen(); }
    CommunicationDiagnostic writeBytes(const QByteArray &data) override
    {
        if (!isOpen()) {
            auto d = open();
            if (!d.ok)
                return d;
        }
        const qint64 n = m_port.write(data);
        if (n != data.size() || !m_port.waitForBytesWritten(m_config.timeoutMs)) {
            return CommunicationDiagnostic::fail(Codes::ioError(),
                                                 QStringLiteral("串口写失败: %1").arg(m_port.errorString()));
        }
        return CommunicationDiagnostic::success();
    }
    CommunicationDiagnostic readBytes(int minBytes, int maxBytes, int timeoutMs,
                                      QByteArray *out) override
    {
        if (!out)
            return CommunicationDiagnostic::fail(Codes::invalidParameter(), QStringLiteral("输出为空"));
        QElapsedTimer timer;
        timer.start();
        QByteArray buffer;
        while (buffer.size() < minBytes) {
            const int remain = timeoutMs - int(timer.elapsed());
            if (remain <= 0)
                return CommunicationDiagnostic::fail(Codes::timeout(), QStringLiteral("串口读超时"));
            if (m_port.bytesAvailable() <= 0 && !m_port.waitForReadyRead(remain)) {
                if (buffer.size() >= minBytes)
                    break;
                return CommunicationDiagnostic::fail(Codes::timeout(), QStringLiteral("串口读超时"),
                                                     {m_port.errorString()});
            }
            buffer.append(m_port.read(maxBytes > 0 ? maxBytes - buffer.size() : 4096));
            if (maxBytes > 0 && buffer.size() >= maxBytes)
                break;
        }
        *out = buffer;
        return CommunicationDiagnostic::success();
    }

private:
    SerialConfig m_config;
    QSerialPort m_port;
};
#endif

RequestResult runRequest(ITransport *transport, const QByteArray &payload,
                         const ReceiveFramePolicy &policy, int timeoutMs, int retries)
{
    RequestResult result;
    result.request = payload;
    QElapsedTimer timer;
    timer.start();
    if (!transport) {
        result.diagnostic = CommunicationDiagnostic::fail(Codes::notAvailable(),
                                                          QStringLiteral("未配置传输层"));
        result.elapsedMs = timer.elapsed();
        return result;
    }
    ReceiveFramePolicy effective = policy;
    if (effective.expectMinBytes < 0)
        effective.expectMinBytes = 0;
    if (effective.maxBytes <= 0)
        effective.maxBytes = 65536;
    if (effective.idleGapMs <= 0)
        effective.idleGapMs = 50;

    const int attempts = qMax(1, retries + 1);
    for (int i = 0; i < attempts; ++i) {
        auto openDiag = transport->isOpen() ? CommunicationDiagnostic::success() : transport->open();
        if (!openDiag.ok) {
            result.diagnostic = openDiag;
            continue;
        }
        auto writeDiag = transport->writeBytes(payload);
        if (!writeDiag.ok) {
            result.diagnostic = writeDiag;
            continue;
        }

        QByteArray response;
        CommunicationDiagnostic readDiag = CommunicationDiagnostic::success();
        if (effective.mode == ReceiveFrameMode::FixedLength) {
            const int minBytes = qMax(1, effective.expectMinBytes);
            readDiag = transport->readBytes(minBytes, effective.maxBytes, timeoutMs, &response);
        } else if (effective.mode == ReceiveFrameMode::MaxBytes) {
            readDiag = transport->readBytes(1, effective.maxBytes, timeoutMs, &response);
        } else if (effective.mode == ReceiveFrameMode::Terminator) {
            QElapsedTimer total;
            total.start();
            while (total.elapsed() < timeoutMs) {
                QByteArray chunk;
                const int remain = timeoutMs - int(total.elapsed());
                auto part = transport->readBytes(1, effective.maxBytes - response.size(),
                                                 qMin(remain, effective.idleGapMs), &chunk);
                if (part.ok)
                    response.append(chunk);
                if (response.contains(effective.terminator) || response.size() >= effective.maxBytes)
                    break;
                if (!part.ok && response.size() >= effective.expectMinBytes)
                    break;
                if (!part.ok && response.isEmpty()) {
                    readDiag = part;
                    break;
                }
            }
            if (response.isEmpty() && readDiag.ok)
                readDiag = CommunicationDiagnostic::fail(Codes::timeout(), QStringLiteral("帧尾接收超时"));
        } else { // IdleGap
            QElapsedTimer total;
            total.start();
            while (total.elapsed() < timeoutMs && response.size() < effective.maxBytes) {
                QByteArray chunk;
                auto part = transport->readBytes(1, effective.maxBytes - response.size(),
                                                 effective.idleGapMs, &chunk);
                if (!part.ok) {
                    if (response.size() >= qMax(1, effective.expectMinBytes))
                        break;
                    readDiag = part;
                    break;
                }
                response.append(chunk);
            }
            if (response.isEmpty() && readDiag.ok)
                readDiag = CommunicationDiagnostic::fail(Codes::timeout(), QStringLiteral("空闲间隔收包超时"));
        }

        if (!readDiag.ok) {
            result.diagnostic = readDiag;
            continue;
        }
        result.response = response;
        result.diagnostic = CommunicationDiagnostic::success();
        result.elapsedMs = timer.elapsed();
        return result;
    }
    result.elapsedMs = timer.elapsed();
    return result;
}

} // namespace

bool networkAvailable()
{
#if defined(SELT_HAS_QT_NETWORK) && SELT_HAS_QT_NETWORK
    return true;
#else
    return false;
#endif
}

bool serialPortAvailable()
{
#if defined(SELT_HAS_QT_SERIALPORT) && SELT_HAS_QT_SERIALPORT
    return true;
#else
    return false;
#endif
}

bool serialBusAvailable()
{
#if defined(SELT_HAS_QT_SERIALBUS) && SELT_HAS_QT_SERIALBUS
    return true;
#else
    return false;
#endif
}

QStringList deploymentRequirements()
{
    QStringList req;
    req << QStringLiteral("Qt6 Network DLL（TCP/IP）");
    req << QStringLiteral("Qt6 SerialPort DLL（COM / Modbus RTU）");
    req << QStringLiteral("可选 Qt6 SerialBus（官方 Modbus 客户端 API）");
    req << QStringLiteral("Windows: 串口驱动与管理员/用户权限");
    return req;
}

QStringList capabilitySummary()
{
    QStringList lines;
    lines << QStringLiteral("Qt Network: %1").arg(networkAvailable() ? QStringLiteral("yes")
                                                                     : QStringLiteral("no"));
    lines << QStringLiteral("Qt SerialPort: %1").arg(serialPortAvailable() ? QStringLiteral("yes")
                                                                           : QStringLiteral("no"));
    lines << QStringLiteral("Qt SerialBus: %1").arg(serialBusAvailable() ? QStringLiteral("yes")
                                                                         : QStringLiteral("no"));
    return lines;
}

QStringList runtimeDeploymentHints()
{
    QStringList hints = deploymentRequirements();
    if (!networkAvailable())
        hints.prepend(QStringLiteral("当前构建未启用 Qt Network：真实 TCP/Modbus TCP 不可用"));
    if (!serialPortAvailable())
        hints.prepend(QStringLiteral(
            "当前构建未启用 Qt SerialPort：真实 COM/Modbus RTU 不可用，请用 Mock 或安装模块后重新 Configure"));
    if (!serialBusAvailable())
        hints.append(QStringLiteral("Qt SerialBus 未启用：使用自研 Modbus 帧（含 CRC）；可选安装 SerialBus"));
    return hints;
}

std::unique_ptr<ITransport> createQtTcpTransport(const TcpConfig &config)
{
#if defined(SELT_HAS_QT_NETWORK) && SELT_HAS_QT_NETWORK
    return std::make_unique<QtTcpTransport>(config);
#else
    Q_UNUSED(config);
    return std::make_unique<NullTransport>(QStringLiteral("未编译 Qt Network 支持"));
#endif
}

std::unique_ptr<ITransport> createQtSerialTransport(const SerialConfig &config)
{
#if defined(SELT_HAS_QT_SERIALPORT) && SELT_HAS_QT_SERIALPORT
    return std::make_unique<QtSerialTransport>(config);
#else
    Q_UNUSED(config);
    return std::make_unique<NullTransport>(QStringLiteral("未编译 Qt SerialPort 支持"));
#endif
}

TcpClient::TcpClient(std::unique_ptr<ITransport> transport)
    : m_transport(std::move(transport))
{
}

void TcpClient::setConfig(const TcpConfig &config)
{
    m_config = config;
}

void TcpClient::setTransport(std::unique_ptr<ITransport> transport)
{
    m_transport = std::move(transport);
}

CommunicationDiagnostic TcpClient::open()
{
    if (!m_transport)
        m_transport = createQtTcpTransport(m_config);
    return m_transport->open();
}

void TcpClient::close()
{
    if (m_transport)
        m_transport->close();
}

bool TcpClient::isOpen() const
{
    return m_transport && m_transport->isOpen();
}

RequestResult TcpClient::request(const QByteArray &payload, int expectMinBytes)
{
    ReceiveFramePolicy policy = m_config.receive;
    policy.mode = ReceiveFrameMode::FixedLength;
    policy.expectMinBytes = expectMinBytes;
    return request(payload, policy);
}

RequestResult TcpClient::request(const QByteArray &payload, const ReceiveFramePolicy &policy)
{
    if (!m_transport)
        m_transport = createQtTcpTransport(m_config);
    return runRequest(m_transport.get(), payload, policy, m_config.timeoutMs, m_config.retries);
}

SerialClient::SerialClient(std::unique_ptr<ITransport> transport)
    : m_transport(std::move(transport))
{
}

void SerialClient::setConfig(const SerialConfig &config)
{
    m_config = config;
}

void SerialClient::setTransport(std::unique_ptr<ITransport> transport)
{
    m_transport = std::move(transport);
}

CommunicationDiagnostic SerialClient::open()
{
    if (!m_transport)
        m_transport = createQtSerialTransport(m_config);
    return m_transport->open();
}

void SerialClient::close()
{
    if (m_transport)
        m_transport->close();
}

bool SerialClient::isOpen() const
{
    return m_transport && m_transport->isOpen();
}

RequestResult SerialClient::request(const QByteArray &payload, int expectMinBytes)
{
    ReceiveFramePolicy policy = m_config.receive;
    policy.mode = ReceiveFrameMode::FixedLength;
    policy.expectMinBytes = expectMinBytes;
    if (policy.idleGapMs <= 0)
        policy.idleGapMs = m_config.interByteTimeoutMs;
    return request(payload, policy);
}

RequestResult SerialClient::request(const QByteArray &payload, const ReceiveFramePolicy &policy)
{
    if (!m_transport)
        m_transport = createQtSerialTransport(m_config);
    return runRequest(m_transport.get(), payload, policy, m_config.timeoutMs, m_config.retries);
}

ModbusClient::ModbusClient(std::unique_ptr<ITransport> transport)
    : m_transport(std::move(transport))
{
}

void ModbusClient::setConfig(const ModbusConfig &config)
{
    m_config = config;
}

void ModbusClient::setTransport(std::unique_ptr<ITransport> transport)
{
    m_transport = std::move(transport);
}

CommunicationDiagnostic ModbusClient::ensureOpen()
{
    if (!m_transport) {
        if (m_config.transport == TransportKind::ModbusRtu)
            m_transport = createQtSerialTransport(m_config.serial);
        else
            m_transport = createQtTcpTransport(m_config.tcp);
    }
    if (m_transport->isOpen())
        return CommunicationDiagnostic::success();
    return m_transport->open();
}

CommunicationDiagnostic ModbusClient::open()
{
    return ensureOpen();
}

void ModbusClient::close()
{
    if (m_transport)
        m_transport->close();
}

bool ModbusClient::isOpen() const
{
    return m_transport && m_transport->isOpen();
}

QByteArray ModbusClient::wrapTcpAdu(const QByteArray &pdu)
{
    QByteArray adu;
    const quint16 tid = m_transactionId++;
    if (m_transactionId == 0)
        m_transactionId = 1;
    adu.append(char((tid >> 8) & 0xFF));
    adu.append(char(tid & 0xFF));
    adu.append(char(0));
    adu.append(char(0));
    const int len = pdu.size() + 1;
    adu.append(char((len >> 8) & 0xFF));
    adu.append(char(len & 0xFF));
    adu.append(char(m_config.unitId & 0xFF));
    adu.append(pdu);
    return adu;
}

ModbusValueResult ModbusClient::transact(const QByteArray &pdu, int expectedQuantity)
{
    ModbusValueResult result;
    result.quantity = expectedQuantity;
    QElapsedTimer timer;
    timer.start();
    auto openDiag = ensureOpen();
    if (!openDiag.ok) {
        result.diagnostic = openDiag;
        result.elapsedMs = timer.elapsed();
        return result;
    }

    QByteArray request = pdu;
    ReceiveFramePolicy policy;
    policy.mode = ReceiveFrameMode::FixedLength;
    if (m_config.transport != TransportKind::ModbusRtu) {
        request = wrapTcpAdu(pdu);
        policy.expectMinBytes = 9;
        policy.maxBytes = 260;
    } else {
        QByteArray rtu;
        rtu.append(char(m_config.unitId & 0xFF));
        rtu.append(pdu);
        request = ModbusCrc::append(rtu);
        policy.expectMinBytes = 5; // addr+fn+...+crc
        policy.maxBytes = 256;
        policy.mode = ReceiveFrameMode::IdleGap;
        policy.idleGapMs = qMax(5, m_config.serial.interByteTimeoutMs);
    }

    const int timeout = m_config.timeoutMs > 0 ? m_config.timeoutMs
                                               : (m_config.transport == TransportKind::ModbusRtu
                                                      ? m_config.serial.timeoutMs
                                                      : m_config.tcp.timeoutMs);
    auto req = runRequest(m_transport.get(), request, policy, timeout, m_config.retries);
    if (!req.diagnostic.ok) {
        result.diagnostic = req.diagnostic;
        result.elapsedMs = timer.elapsed();
        return result;
    }

    QByteArray respPdu;
    if (m_config.transport != TransportKind::ModbusRtu) {
        if (req.response.size() < 8) {
            result.diagnostic = CommunicationDiagnostic::fail(Codes::protocolError(),
                                                              QStringLiteral("Modbus TCP 响应过短"));
            result.elapsedMs = timer.elapsed();
            return result;
        }
        respPdu = req.response.mid(7);
    } else {
        QString crcError;
        if (!ModbusCrc::validate(req.response, &crcError)) {
            result.diagnostic = CommunicationDiagnostic::fail(Codes::crcError(), crcError,
                                                              {QStringLiteral("检查波特率/线序/干扰")});
            result.elapsedMs = timer.elapsed();
            return result;
        }
        if (req.response.size() < 5) {
            result.diagnostic = CommunicationDiagnostic::fail(Codes::protocolError(),
                                                              QStringLiteral("Modbus RTU 响应过短"));
            result.elapsedMs = timer.elapsed();
            return result;
        }
        respPdu = req.response.mid(1, req.response.size() - 3); // skip unit + CRC
    }

    if (respPdu.isEmpty()) {
        result.diagnostic = CommunicationDiagnostic::fail(Codes::protocolError(),
                                                          QStringLiteral("空 PDU"));
        result.elapsedMs = timer.elapsed();
        return result;
    }
    const quint8 fn = uchar(respPdu.at(0));
    if (fn & 0x80) {
        const int code = respPdu.size() > 1 ? uchar(respPdu.at(1)) : -1;
        result.diagnostic = CommunicationDiagnostic::fail(
            Codes::deviceException(),
            QStringLiteral("Modbus 异常响应: function=0x%1 exception=%2")
                .arg(fn & 0x7F, 2, 16, QLatin1Char('0'))
                .arg(code),
            {QStringLiteral("检查地址/数量/功能码权限")});
        result.elapsedMs = timer.elapsed();
        return result;
    }

    if ((fn == 0x01 || fn == 0x02) && respPdu.size() >= 2) {
        const int byteCount = uchar(respPdu.at(1));
        const int qty = expectedQuantity > 0 ? expectedQuantity : byteCount * 8;
        auto unpacked = Codec::unpackCoils(respPdu.mid(2, byteCount), qty);
        result.coils = unpacked.coils;
        if (!unpacked.diagnostic.ok)
            result.diagnostic = unpacked.diagnostic;
    } else if ((fn == 0x03 || fn == 0x04) && respPdu.size() >= 2) {
        const int byteCount = uchar(respPdu.at(1));
        auto unpacked = Codec::unpackRegisters(respPdu.mid(2, byteCount));
        result.registers = unpacked.registers;
        if (!unpacked.diagnostic.ok)
            result.diagnostic = unpacked.diagnostic;
    }

    result.elapsedMs = timer.elapsed();
    return result;
}

ModbusValueResult ModbusClient::readBits(ModbusTable table, int address, int quantity)
{
    ModbusValueResult result;
    result.table = table;
    result.address = address;
    result.quantity = quantity;
    if (address < 0 || quantity < 1 || quantity > 2000) {
        result.diagnostic = CommunicationDiagnostic::fail(Codes::invalidParameter(),
                                                          QStringLiteral("线圈读参数无效"));
        return result;
    }
    const quint8 fn = (table == ModbusTable::DiscreteInputs) ? 0x02 : 0x01;
    if (table != ModbusTable::Coils && table != ModbusTable::DiscreteInputs) {
        result.diagnostic = CommunicationDiagnostic::fail(Codes::invalidParameter(),
                                                          QStringLiteral("读位表类型错误"));
        return result;
    }
    QByteArray pdu;
    pdu.append(char(fn));
    pdu.append(char((address >> 8) & 0xFF));
    pdu.append(char(address & 0xFF));
    pdu.append(char((quantity >> 8) & 0xFF));
    pdu.append(char(quantity & 0xFF));
    auto r = transact(pdu, quantity);
    r.table = table;
    r.address = address;
    r.quantity = quantity;
    return r;
}

ModbusValueResult ModbusClient::readRegisters(ModbusTable table, int address, int quantity)
{
    ModbusValueResult result;
    result.table = table;
    result.address = address;
    result.quantity = quantity;
    if (address < 0 || quantity < 1 || quantity > 125) {
        result.diagnostic = CommunicationDiagnostic::fail(Codes::invalidParameter(),
                                                          QStringLiteral("寄存器读参数无效"));
        return result;
    }
    if (table != ModbusTable::HoldingRegisters && table != ModbusTable::InputRegisters) {
        result.diagnostic = CommunicationDiagnostic::fail(Codes::invalidParameter(),
                                                          QStringLiteral("读寄存器表类型错误"));
        return result;
    }
    const quint8 fn = (table == ModbusTable::InputRegisters) ? 0x04 : 0x03;
    QByteArray pdu;
    pdu.append(char(fn));
    pdu.append(char((address >> 8) & 0xFF));
    pdu.append(char(address & 0xFF));
    pdu.append(char((quantity >> 8) & 0xFF));
    pdu.append(char(quantity & 0xFF));
    auto r = transact(pdu, quantity);
    r.table = table;
    r.address = address;
    r.quantity = quantity;
    return r;
}

ModbusValueResult ModbusClient::writeCoil(int address, bool value, bool allowWrite)
{
    ModbusValueResult result;
    result.table = ModbusTable::Coils;
    result.address = address;
    result.quantity = 1;
    if (!allowWrite) {
        result.diagnostic = CommunicationDiagnostic::fail(
            Codes::writeProtected(), QStringLiteral("写线圈被保护，请显式启用 allowWrite"),
            {QStringLiteral("节点参数 allowWrite=true")});
        return result;
    }
    if (address < 0) {
        result.diagnostic = CommunicationDiagnostic::fail(Codes::invalidParameter(),
                                                          QStringLiteral("线圈地址无效"));
        return result;
    }
    QByteArray pdu;
    pdu.append(char(0x05));
    pdu.append(char((address >> 8) & 0xFF));
    pdu.append(char(address & 0xFF));
    pdu.append(char(value ? 0xFF : 0x00));
    pdu.append(char(0x00));
    return transact(pdu);
}

ModbusValueResult ModbusClient::writeCoils(int address, const QVector<bool> &values, bool allowWrite)
{
    ModbusValueResult result;
    result.table = ModbusTable::Coils;
    result.address = address;
    result.quantity = values.size();
    if (!allowWrite) {
        result.diagnostic = CommunicationDiagnostic::fail(Codes::writeProtected(),
                                                          QStringLiteral("写多线圈被保护"));
        return result;
    }
    if (address < 0 || values.isEmpty() || values.size() > 1968) {
        result.diagnostic = CommunicationDiagnostic::fail(Codes::invalidParameter(),
                                                          QStringLiteral("写多线圈参数无效"));
        return result;
    }
    auto packed = Codec::packCoils(values);
    QByteArray pdu;
    pdu.append(char(0x0F));
    pdu.append(char((address >> 8) & 0xFF));
    pdu.append(char(address & 0xFF));
    pdu.append(char((values.size() >> 8) & 0xFF));
    pdu.append(char(values.size() & 0xFF));
    pdu.append(char(packed.bytes.size()));
    pdu.append(packed.bytes);
    auto r = transact(pdu);
    r.coils = values;
    return r;
}

ModbusValueResult ModbusClient::writeRegister(int address, quint16 value, bool allowWrite)
{
    ModbusValueResult result;
    result.table = ModbusTable::HoldingRegisters;
    result.address = address;
    result.quantity = 1;
    if (!allowWrite) {
        result.diagnostic = CommunicationDiagnostic::fail(Codes::writeProtected(),
                                                          QStringLiteral("写寄存器被保护"));
        return result;
    }
    if (address < 0) {
        result.diagnostic = CommunicationDiagnostic::fail(Codes::invalidParameter(),
                                                          QStringLiteral("寄存器地址无效"));
        return result;
    }
    QByteArray pdu;
    pdu.append(char(0x06));
    pdu.append(char((address >> 8) & 0xFF));
    pdu.append(char(address & 0xFF));
    pdu.append(char((value >> 8) & 0xFF));
    pdu.append(char(value & 0xFF));
    auto r = transact(pdu);
    r.registers = {value};
    return r;
}

ModbusValueResult ModbusClient::writeRegisters(int address, const QVector<quint16> &values,
                                               bool allowWrite)
{
    ModbusValueResult result;
    result.table = ModbusTable::HoldingRegisters;
    result.address = address;
    result.quantity = values.size();
    if (!allowWrite) {
        result.diagnostic = CommunicationDiagnostic::fail(Codes::writeProtected(),
                                                          QStringLiteral("写多寄存器被保护"));
        return result;
    }
    if (address < 0 || values.isEmpty() || values.size() > 123) {
        result.diagnostic = CommunicationDiagnostic::fail(Codes::invalidParameter(),
                                                          QStringLiteral("写多寄存器参数无效"));
        return result;
    }
    auto packed = Codec::packRegisters(values);
    QByteArray pdu;
    pdu.append(char(0x10));
    pdu.append(char((address >> 8) & 0xFF));
    pdu.append(char(address & 0xFF));
    pdu.append(char((values.size() >> 8) & 0xFF));
    pdu.append(char(values.size() & 0xFF));
    pdu.append(char(packed.bytes.size()));
    pdu.append(packed.bytes);
    auto r = transact(pdu);
    r.registers = values;
    return r;
}

} // namespace Communication
} // namespace Selt
