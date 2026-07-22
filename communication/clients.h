#pragma once

#include "communication/itransport.h"
#include "communication/types.h"

#include <memory>

namespace Selt {
namespace Communication {

class TcpClient
{
public:
    explicit TcpClient(std::unique_ptr<ITransport> transport = nullptr);
    void setConfig(const TcpConfig &config);
    TcpConfig config() const { return m_config; }
    void setTransport(std::unique_ptr<ITransport> transport);

    RequestResult request(const QByteArray &payload, int expectMinBytes = 1);
    RequestResult request(const QByteArray &payload, const ReceiveFramePolicy &policy);
    CommunicationDiagnostic open();
    void close();
    bool isOpen() const;

private:
    TcpConfig m_config;
    std::unique_ptr<ITransport> m_transport;
};

class SerialClient
{
public:
    explicit SerialClient(std::unique_ptr<ITransport> transport = nullptr);
    void setConfig(const SerialConfig &config);
    SerialConfig config() const { return m_config; }
    void setTransport(std::unique_ptr<ITransport> transport);

    RequestResult request(const QByteArray &payload, int expectMinBytes = 1);
    RequestResult request(const QByteArray &payload, const ReceiveFramePolicy &policy);
    CommunicationDiagnostic open();
    void close();
    bool isOpen() const;

private:
    SerialConfig m_config;
    std::unique_ptr<ITransport> m_transport;
};

class ModbusClient
{
public:
    explicit ModbusClient(std::unique_ptr<ITransport> transport = nullptr);
    void setConfig(const ModbusConfig &config);
    ModbusConfig config() const { return m_config; }
    void setTransport(std::unique_ptr<ITransport> transport);

    ModbusValueResult readBits(ModbusTable table, int address, int quantity);
    ModbusValueResult readRegisters(ModbusTable table, int address, int quantity);
    ModbusValueResult writeCoil(int address, bool value, bool allowWrite);
    ModbusValueResult writeCoils(int address, const QVector<bool> &values, bool allowWrite);
    ModbusValueResult writeRegister(int address, quint16 value, bool allowWrite);
    ModbusValueResult writeRegisters(int address, const QVector<quint16> &values, bool allowWrite);

    CommunicationDiagnostic open();
    void close();
    bool isOpen() const;

private:
    ModbusValueResult transact(const QByteArray &pdu, int expectedQuantity = 0);
    QByteArray wrapTcpAdu(const QByteArray &pdu);
    CommunicationDiagnostic ensureOpen();

    ModbusConfig m_config;
    std::unique_ptr<ITransport> m_transport;
    quint16 m_transactionId{1};
};

std::unique_ptr<ITransport> createQtTcpTransport(const TcpConfig &config);
std::unique_ptr<ITransport> createQtSerialTransport(const SerialConfig &config);

bool networkAvailable();
bool serialPortAvailable();
bool serialBusAvailable();
QStringList deploymentRequirements();
QStringList capabilitySummary();
QStringList runtimeDeploymentHints();

} // namespace Communication
} // namespace Selt
