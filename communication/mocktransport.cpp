#include "communication/mocktransport.h"

#include "communication/codec.h"

#include <QtEndian>

namespace Selt {
namespace Communication {

MockTransport::MockTransport(Handler handler)
    : m_handler(std::move(handler))
{
}

CommunicationDiagnostic MockTransport::open()
{
    QMutexLocker lock(&m_mutex);
    m_open = true;
    return CommunicationDiagnostic::success();
}

void MockTransport::close()
{
    QMutexLocker lock(&m_mutex);
    m_open = false;
    m_pendingRead.clear();
}

bool MockTransport::isOpen() const
{
    QMutexLocker lock(&m_mutex);
    return m_open;
}

void MockTransport::setHandler(Handler handler)
{
    QMutexLocker lock(&m_mutex);
    m_handler = std::move(handler);
}

void MockTransport::enqueueResponse(const QByteArray &response)
{
    QMutexLocker lock(&m_mutex);
    m_queue.append(response);
}

void MockTransport::setForceTimeout(bool on)
{
    QMutexLocker lock(&m_mutex);
    m_forceTimeout = on;
}

QByteArray MockTransport::lastRequest() const
{
    QMutexLocker lock(&m_mutex);
    return m_lastRequest;
}

CommunicationDiagnostic MockTransport::writeBytes(const QByteArray &data)
{
    QMutexLocker lock(&m_mutex);
    if (!m_open) {
        return CommunicationDiagnostic::fail(Codes::ioError(), QStringLiteral("Mock 未打开"));
    }
    m_lastRequest = data;
    QByteArray response;
    if (!m_queue.isEmpty()) {
        response = m_queue.takeFirst();
    } else if (m_handler) {
        response = m_handler(data);
    } else {
        response = data; // echo
    }
    m_pendingRead.append(response);
    return CommunicationDiagnostic::success();
}

CommunicationDiagnostic MockTransport::readBytes(int minBytes, int maxBytes, int timeoutMs,
                                                 QByteArray *out)
{
    Q_UNUSED(timeoutMs);
    if (!out) {
        return CommunicationDiagnostic::fail(Codes::invalidParameter(),
                                             QStringLiteral("输出缓冲为空"));
    }
    QMutexLocker lock(&m_mutex);
    if (!m_open) {
        return CommunicationDiagnostic::fail(Codes::ioError(), QStringLiteral("Mock 未打开"));
    }
    if (m_forceTimeout || m_pendingRead.size() < minBytes) {
        return CommunicationDiagnostic::fail(Codes::timeout(), QStringLiteral("Mock 读超时"),
                                             {QStringLiteral("检查响应脚本或关闭 forceTimeout")});
    }
    const int take = qMin(maxBytes > 0 ? maxBytes : m_pendingRead.size(), m_pendingRead.size());
    *out = m_pendingRead.left(take);
    m_pendingRead.remove(0, take);
    return CommunicationDiagnostic::success();
}

MockModbusMap::MockModbusMap(int coilCount, int discreteCount, int holdingCount, int inputCount)
{
    coils = QVector<bool>(coilCount, false);
    discreteInputs = QVector<bool>(discreteCount, false);
    holdingRegisters = QVector<quint16>(holdingCount, 0);
    inputRegisters = QVector<quint16>(inputCount, 0);
}

static QByteArray exceptionPdu(quint8 function, quint8 code)
{
    QByteArray pdu;
    pdu.append(char(function | 0x80));
    pdu.append(char(code));
    return pdu;
}

QByteArray MockModbusMap::handlePdu(quint8 unitId, const QByteArray &pdu) const
{
    Q_UNUSED(unitId);
    if (pdu.isEmpty())
        return exceptionPdu(0, 0x01);
    const quint8 fn = uchar(pdu.at(0));
    auto readBits = [&](const QVector<bool> &src) -> QByteArray {
        if (pdu.size() < 5)
            return exceptionPdu(fn, 0x03);
        const int addr = (uchar(pdu.at(1)) << 8) | uchar(pdu.at(2));
        const int qty = (uchar(pdu.at(3)) << 8) | uchar(pdu.at(4));
        if (qty < 1 || qty > 2000 || addr < 0 || addr + qty > src.size())
            return exceptionPdu(fn, 0x02);
        auto packed = Codec::packCoils(src.mid(addr, qty));
        QByteArray resp;
        resp.append(char(fn));
        resp.append(char(packed.bytes.size()));
        resp.append(packed.bytes);
        return resp;
    };
    auto readRegs = [&](const QVector<quint16> &src) -> QByteArray {
        if (pdu.size() < 5)
            return exceptionPdu(fn, 0x03);
        const int addr = (uchar(pdu.at(1)) << 8) | uchar(pdu.at(2));
        const int qty = (uchar(pdu.at(3)) << 8) | uchar(pdu.at(4));
        if (qty < 1 || qty > 125 || addr < 0 || addr + qty > src.size())
            return exceptionPdu(fn, 0x02);
        auto packed = Codec::packRegisters(src.mid(addr, qty));
        QByteArray resp;
        resp.append(char(fn));
        resp.append(char(packed.bytes.size()));
        resp.append(packed.bytes);
        return resp;
    };

    switch (fn) {
    case 0x01:
        return readBits(coils);
    case 0x02:
        return readBits(discreteInputs);
    case 0x03:
        return readRegs(holdingRegisters);
    case 0x04:
        return readRegs(inputRegisters);
    case 0x05: {
        if (pdu.size() < 5)
            return exceptionPdu(fn, 0x03);
        const int addr = (uchar(pdu.at(1)) << 8) | uchar(pdu.at(2));
        const quint16 value = quint16((uchar(pdu.at(3)) << 8) | uchar(pdu.at(4)));
        if (addr < 0 || addr >= coils.size())
            return exceptionPdu(fn, 0x02);
        const_cast<MockModbusMap *>(this)->coils[addr] = (value == 0xFF00);
        return pdu.left(5);
    }
    case 0x06: {
        if (pdu.size() < 5)
            return exceptionPdu(fn, 0x03);
        const int addr = (uchar(pdu.at(1)) << 8) | uchar(pdu.at(2));
        const quint16 value = quint16((uchar(pdu.at(3)) << 8) | uchar(pdu.at(4)));
        if (addr < 0 || addr >= holdingRegisters.size())
            return exceptionPdu(fn, 0x02);
        const_cast<MockModbusMap *>(this)->holdingRegisters[addr] = value;
        return pdu.left(5);
    }
    case 0x0F: {
        if (pdu.size() < 6)
            return exceptionPdu(fn, 0x03);
        const int addr = (uchar(pdu.at(1)) << 8) | uchar(pdu.at(2));
        const int qty = (uchar(pdu.at(3)) << 8) | uchar(pdu.at(4));
        const int byteCount = uchar(pdu.at(5));
        if (qty < 1 || addr < 0 || addr + qty > coils.size() || pdu.size() < 6 + byteCount)
            return exceptionPdu(fn, 0x02);
        auto unpacked = Codec::unpackCoils(pdu.mid(6, byteCount), qty);
        for (int i = 0; i < qty; ++i)
            const_cast<MockModbusMap *>(this)->coils[addr + i] = unpacked.coils.at(i);
        QByteArray resp;
        resp.append(char(fn));
        resp.append(char((addr >> 8) & 0xFF));
        resp.append(char(addr & 0xFF));
        resp.append(char((qty >> 8) & 0xFF));
        resp.append(char(qty & 0xFF));
        return resp;
    }
    case 0x10: {
        if (pdu.size() < 6)
            return exceptionPdu(fn, 0x03);
        const int addr = (uchar(pdu.at(1)) << 8) | uchar(pdu.at(2));
        const int qty = (uchar(pdu.at(3)) << 8) | uchar(pdu.at(4));
        const int byteCount = uchar(pdu.at(5));
        if (qty < 1 || addr < 0 || addr + qty > holdingRegisters.size() || pdu.size() < 6 + byteCount)
            return exceptionPdu(fn, 0x02);
        auto unpacked = Codec::unpackRegisters(pdu.mid(6, byteCount));
        for (int i = 0; i < qty; ++i)
            const_cast<MockModbusMap *>(this)->holdingRegisters[addr + i] = unpacked.registers.at(i);
        QByteArray resp;
        resp.append(char(fn));
        resp.append(char((addr >> 8) & 0xFF));
        resp.append(char(addr & 0xFF));
        resp.append(char((qty >> 8) & 0xFF));
        resp.append(char(qty & 0xFF));
        return resp;
    }
    default:
        return exceptionPdu(fn, 0x01);
    }
}

QByteArray MockModbusMap::handleTcpPdu(const QByteArray &adu) const
{
    if (adu.size() < 7)
        return {};
    const quint16 transaction = quint16((uchar(adu.at(0)) << 8) | uchar(adu.at(1)));
    const quint16 protocol = quint16((uchar(adu.at(2)) << 8) | uchar(adu.at(3)));
    Q_UNUSED(protocol);
    const quint8 unit = uchar(adu.at(6));
    const QByteArray pdu = adu.mid(7);
    const QByteArray respPdu = handlePdu(unit, pdu);
    QByteArray resp;
    resp.append(char((transaction >> 8) & 0xFF));
    resp.append(char(transaction & 0xFF));
    resp.append(char(0));
    resp.append(char(0));
    const int len = respPdu.size() + 1;
    resp.append(char((len >> 8) & 0xFF));
    resp.append(char(len & 0xFF));
    resp.append(char(unit));
    resp.append(respPdu);
    return resp;
}

} // namespace Communication
} // namespace Selt
