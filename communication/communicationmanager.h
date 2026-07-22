#pragma once

#include "communication/clients.h"
#include "communication/types.h"

#include <QHash>
#include <QMutex>
#include <QString>
#include <QVector>
#include <atomic>
#include <memory>

namespace Selt {

class CancellationToken;

namespace Communication {

enum class ConnectionState {
    Closed,
    Opening,
    Open,
    Busy,
    Reconnecting,
    Failed
};

QString connectionStateToString(ConnectionState state);

class CommunicationManager
{
public:
    static CommunicationManager &instance();

    void upsertProfile(const CommunicationProfile &profile);
    bool removeProfile(const QString &id);
    bool contains(const QString &id) const;
    CommunicationProfile profile(const QString &id) const;
    QVector<CommunicationProfile> profiles() const;
    void clearProfiles();

    CommunicationDiagnostic open(const QString &profileId, bool useMock = false);
    void close(const QString &profileId);
    void closeAll();
    ConnectionState state(const QString &profileId) const;
    QString stateText(const QString &profileId) const;

    RequestResult request(const QString &profileId, const QByteArray &payload,
                          const ReceiveFramePolicy &policy, bool useMock = false,
                          CancellationToken *cancel = nullptr);

    ModbusValueResult modbusReadBits(const QString &profileId, ModbusTable table, int address,
                                     int quantity, bool useMock = false,
                                     CancellationToken *cancel = nullptr);
    ModbusValueResult modbusReadRegisters(const QString &profileId, ModbusTable table, int address,
                                          int quantity, bool useMock = false,
                                          CancellationToken *cancel = nullptr);
    ModbusValueResult modbusWriteCoil(const QString &profileId, int address, bool value,
                                      bool allowWrite, bool useMock = false,
                                      CancellationToken *cancel = nullptr);
    ModbusValueResult modbusWriteRegisters(const QString &profileId, int address,
                                           const QVector<quint16> &values, bool allowWrite,
                                           bool useMock = false, CancellationToken *cancel = nullptr);

    void cancelAll();
    void clearCancel();
    bool isCancelRequested() const;

    QJsonObject profilesToJson() const;
    void profilesFromJson(const QJsonObject &obj);

private:
    CommunicationManager() = default;

    struct Entry {
        CommunicationProfile profile;
        ConnectionState state{ConnectionState::Closed};
        std::unique_ptr<ITransport> transport;
        std::unique_ptr<ModbusClient> modbus;
        std::unique_ptr<TcpClient> tcp;
        std::unique_ptr<SerialClient> serial;
        bool useMock{false};
    };

    Entry *ensureEntry(const QString &profileId);
    CommunicationDiagnostic ensureOpenLocked(Entry *entry, bool useMock);
    RequestResult doRawRequest(Entry *entry, const QByteArray &payload,
                               const ReceiveFramePolicy &policy, CancellationToken *cancel);

    mutable QMutex m_mutex;
    QHash<QString, std::shared_ptr<Entry>> m_entries;
    std::atomic_bool m_cancelAll{false};
};

} // namespace Communication
} // namespace Selt
