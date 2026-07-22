#pragma once

#include "communication/itransport.h"

#include <QByteArray>
#include <QHash>
#include <QMutex>
#include <QVector>
#include <functional>

namespace Selt {
namespace Communication {

/// In-process transport for unit tests: echo, scripted replies, or Modbus memory map.
class MockTransport final : public ITransport
{
public:
    using Handler = std::function<QByteArray(const QByteArray &request)>;

    explicit MockTransport(Handler handler = {});
    QString name() const override { return QStringLiteral("mock"); }
    CommunicationDiagnostic open() override;
    void close() override;
    bool isOpen() const override;
    CommunicationDiagnostic writeBytes(const QByteArray &data) override;
    CommunicationDiagnostic readBytes(int minBytes, int maxBytes, int timeoutMs,
                                      QByteArray *out) override;

    void setHandler(Handler handler);
    void enqueueResponse(const QByteArray &response);
    void setForceTimeout(bool on);
    QByteArray lastRequest() const;

private:
    mutable QMutex m_mutex;
    bool m_open{false};
    bool m_forceTimeout{false};
    Handler m_handler;
    QByteArray m_lastRequest;
    QByteArray m_pendingRead;
    QVector<QByteArray> m_queue;
};

/// Simple Modbus TCP slave memory for loopback tests (zero-based addresses).
class MockModbusMap
{
public:
    QVector<bool> coils;
    QVector<bool> discreteInputs;
    QVector<quint16> holdingRegisters;
    QVector<quint16> inputRegisters;

    explicit MockModbusMap(int coilCount = 256, int discreteCount = 256, int holdingCount = 256,
                           int inputCount = 256);

    QByteArray handleTcpPdu(const QByteArray &adu) const;
    QByteArray handlePdu(quint8 unitId, const QByteArray &pdu) const;
};

} // namespace Communication
} // namespace Selt
