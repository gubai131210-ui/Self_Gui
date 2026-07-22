#include "communication/communicationmanager.h"

#include "communication/crc.h"
#include "communication/mocktransport.h"
#include "vision/runtime/inodeexecutor.h"

#include <QJsonArray>
#include <QThread>
#include <QUuid>

namespace Selt {
namespace Communication {
namespace {

std::unique_ptr<ITransport> makeEchoMock()
{
    return std::make_unique<MockTransport>();
}

std::shared_ptr<MockModbusMap> sharedMap()
{
    static auto map = std::make_shared<MockModbusMap>();
    return map;
}

std::unique_ptr<ITransport> makeModbusMock(TransportKind kind)
{
    auto map = sharedMap();
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

} // namespace

QString connectionStateToString(ConnectionState state)
{
    switch (state) {
    case ConnectionState::Opening:
        return QStringLiteral("opening");
    case ConnectionState::Open:
        return QStringLiteral("open");
    case ConnectionState::Busy:
        return QStringLiteral("busy");
    case ConnectionState::Reconnecting:
        return QStringLiteral("reconnecting");
    case ConnectionState::Failed:
        return QStringLiteral("failed");
    case ConnectionState::Closed:
    default:
        return QStringLiteral("closed");
    }
}

CommunicationManager &CommunicationManager::instance()
{
    static CommunicationManager mgr;
    return mgr;
}

void CommunicationManager::upsertProfile(const CommunicationProfile &profile)
{
    QMutexLocker lock(&m_mutex);
    auto &ptr = m_entries[profile.id];
    if (!ptr)
        ptr = std::make_shared<Entry>();
    ptr->profile = profile;
    if (ptr->profile.id.isEmpty())
        ptr->profile.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
}

bool CommunicationManager::removeProfile(const QString &id)
{
    QMutexLocker lock(&m_mutex);
    auto it = m_entries.find(id);
    if (it == m_entries.end())
        return false;
    if ((*it)->transport)
        (*it)->transport->close();
    m_entries.erase(it);
    return true;
}

bool CommunicationManager::contains(const QString &id) const
{
    QMutexLocker lock(&m_mutex);
    return m_entries.contains(id);
}

CommunicationProfile CommunicationManager::profile(const QString &id) const
{
    QMutexLocker lock(&m_mutex);
    auto it = m_entries.constFind(id);
    if (it == m_entries.cend() || !(*it))
        return {};
    return (*it)->profile;
}

QVector<CommunicationProfile> CommunicationManager::profiles() const
{
    QMutexLocker lock(&m_mutex);
    QVector<CommunicationProfile> out;
    out.reserve(m_entries.size());
    for (auto it = m_entries.cbegin(); it != m_entries.cend(); ++it) {
        if (*it)
            out.append((*it)->profile);
    }
    return out;
}

void CommunicationManager::clearProfiles()
{
    QMutexLocker lock(&m_mutex);
    for (auto &e : m_entries) {
        if (e && e->transport)
            e->transport->close();
    }
    m_entries.clear();
    m_cancelAll.store(false);
}

CommunicationManager::Entry *CommunicationManager::ensureEntry(const QString &profileId)
{
    auto it = m_entries.find(profileId);
    if (it == m_entries.end() || !(*it))
        return nullptr;
    return it->get();
}

CommunicationDiagnostic CommunicationManager::ensureOpenLocked(Entry *entry, bool useMock)
{
    if (!entry)
        return CommunicationDiagnostic::fail(Codes::invalidParameter(), QStringLiteral("连接配置不存在"));
    if (m_cancelAll.load())
        return CommunicationDiagnostic::fail(Codes::cancelled(), QStringLiteral("通讯已取消"));

    if (entry->state == ConnectionState::Busy)
        return CommunicationDiagnostic::fail(Codes::busy(), QStringLiteral("连接忙碌"),
                                             {QStringLiteral("等待当前请求完成")});

    const bool mockChanged = entry->useMock != useMock;
    auto stillOpen = [&]() -> bool {
        if (entry->state != ConnectionState::Open || mockChanged)
            return false;
        if (entry->profile.kind == TransportKind::ModbusTcp
            || entry->profile.kind == TransportKind::ModbusRtu)
            return entry->modbus && entry->modbus->isOpen();
        if (entry->profile.kind == TransportKind::Serial)
            return entry->serial && entry->serial->isOpen();
        return entry->tcp && entry->tcp->isOpen();
    };
    if (stillOpen())
        return CommunicationDiagnostic::success();

    entry->useMock = useMock;
    entry->state = ConnectionState::Opening;

    if (entry->profile.kind == TransportKind::ModbusTcp
        || entry->profile.kind == TransportKind::ModbusRtu) {
        if (!entry->modbus)
            entry->modbus = std::make_unique<ModbusClient>();
        ModbusConfig cfg = entry->profile.modbus;
        cfg.transport = entry->profile.kind;
        if (cfg.transport == TransportKind::ModbusTcp)
            cfg.tcp = entry->profile.tcp.host.isEmpty() ? cfg.tcp : entry->profile.tcp;
        else
            cfg.serial = entry->profile.serial.portName.isEmpty() ? cfg.serial : entry->profile.serial;
        entry->modbus->setConfig(cfg);
        if (mockChanged || !entry->modbus->isOpen()) {
            if (useMock)
                entry->modbus->setTransport(makeModbusMock(cfg.transport));
            else if (mockChanged)
                entry->modbus->setTransport(nullptr);
        }
        auto d = entry->modbus->open();
        entry->state = d.ok ? ConnectionState::Open : ConnectionState::Failed;
        return d;
    }

    if (entry->profile.kind == TransportKind::Serial) {
        if (!entry->serial)
            entry->serial = std::make_unique<SerialClient>();
        entry->serial->setConfig(entry->profile.serial);
        if (mockChanged || !entry->serial->isOpen()) {
            if (useMock)
                entry->serial->setTransport(makeEchoMock());
            else if (mockChanged)
                entry->serial->setTransport(nullptr);
        }
        auto d = entry->serial->open();
        entry->state = d.ok ? ConnectionState::Open : ConnectionState::Failed;
        return d;
    }

    if (!entry->tcp)
        entry->tcp = std::make_unique<TcpClient>();
    entry->tcp->setConfig(entry->profile.tcp);
    if (mockChanged || !entry->tcp->isOpen()) {
        if (useMock)
            entry->tcp->setTransport(makeEchoMock());
        else if (mockChanged)
            entry->tcp->setTransport(nullptr);
    }
    auto d = entry->tcp->open();
    entry->state = d.ok ? ConnectionState::Open : ConnectionState::Failed;
    return d;
}

CommunicationDiagnostic CommunicationManager::open(const QString &profileId, bool useMock)
{
    QMutexLocker lock(&m_mutex);
    Entry *entry = ensureEntry(profileId);
    return ensureOpenLocked(entry, useMock);
}

void CommunicationManager::close(const QString &profileId)
{
    QMutexLocker lock(&m_mutex);
    Entry *entry = ensureEntry(profileId);
    if (!entry)
        return;
    if (entry->tcp)
        entry->tcp->close();
    if (entry->serial)
        entry->serial->close();
    if (entry->modbus)
        entry->modbus->close();
    if (entry->transport)
        entry->transport->close();
    entry->state = ConnectionState::Closed;
}

void CommunicationManager::closeAll()
{
    QMutexLocker lock(&m_mutex);
    for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
        if (!(*it))
            continue;
        if ((*it)->tcp)
            (*it)->tcp->close();
        if ((*it)->serial)
            (*it)->serial->close();
        if ((*it)->modbus)
            (*it)->modbus->close();
        (*it)->state = ConnectionState::Closed;
    }
}

ConnectionState CommunicationManager::state(const QString &profileId) const
{
    QMutexLocker lock(&m_mutex);
    auto it = m_entries.constFind(profileId);
    if (it == m_entries.cend() || !(*it))
        return ConnectionState::Closed;
    return (*it)->state;
}

QString CommunicationManager::stateText(const QString &profileId) const
{
    return connectionStateToString(state(profileId));
}

RequestResult CommunicationManager::doRawRequest(Entry *entry, const QByteArray &payload,
                                                 const ReceiveFramePolicy &policy,
                                                 CancellationToken *cancel)
{
    RequestResult result;
    result.connectionId = entry->profile.id;
    if (cancel && cancel->isCancelled()) {
        result.diagnostic = CommunicationDiagnostic::fail(Codes::cancelled(), QStringLiteral("请求已取消"));
        return result;
    }
    if (m_cancelAll.load()) {
        result.diagnostic = CommunicationDiagnostic::fail(Codes::cancelled(), QStringLiteral("通讯已取消"));
        return result;
    }
    entry->state = ConnectionState::Busy;
    if (entry->profile.kind == TransportKind::Serial)
        result = entry->serial->request(payload, policy);
    else
        result = entry->tcp->request(payload, policy);
    result.connectionId = entry->profile.id;
    result.connectionState = connectionStateToString(
        result.diagnostic.ok ? ConnectionState::Open : ConnectionState::Failed);
    entry->state = result.diagnostic.ok ? ConnectionState::Open : ConnectionState::Failed;

    if (!result.diagnostic.ok && entry->profile.autoReconnect && !entry->useMock) {
        entry->state = ConnectionState::Reconnecting;
        QThread::msleep(quint32(qMax(0, entry->profile.reconnectDelayMs)));
        auto reopen = ensureOpenLocked(entry, entry->useMock);
        if (reopen.ok)
            entry->state = ConnectionState::Open;
    }
    return result;
}

RequestResult CommunicationManager::request(const QString &profileId, const QByteArray &payload,
                                            const ReceiveFramePolicy &policy, bool useMock,
                                            CancellationToken *cancel)
{
    QMutexLocker lock(&m_mutex);
    Entry *entry = ensureEntry(profileId);
    auto openDiag = ensureOpenLocked(entry, useMock);
    if (!openDiag.ok) {
        RequestResult r;
        r.diagnostic = openDiag;
        r.connectionId = profileId;
        r.connectionState = connectionStateToString(ConnectionState::Failed);
        return r;
    }
    return doRawRequest(entry, payload, policy, cancel);
}

ModbusValueResult CommunicationManager::modbusReadBits(const QString &profileId, ModbusTable table,
                                                       int address, int quantity, bool useMock,
                                                       CancellationToken *cancel)
{
    QMutexLocker lock(&m_mutex);
    Entry *entry = ensureEntry(profileId);
    ModbusValueResult r;
    auto openDiag = ensureOpenLocked(entry, useMock);
    if (!openDiag.ok) {
        r.diagnostic = openDiag;
        return r;
    }
    if (cancel && cancel->isCancelled()) {
        r.diagnostic = CommunicationDiagnostic::fail(Codes::cancelled(), QStringLiteral("请求已取消"));
        return r;
    }
    entry->state = ConnectionState::Busy;
    r = entry->modbus->readBits(table, address, quantity);
    entry->state = r.diagnostic.ok ? ConnectionState::Open : ConnectionState::Failed;
    return r;
}

ModbusValueResult CommunicationManager::modbusReadRegisters(const QString &profileId, ModbusTable table,
                                                            int address, int quantity, bool useMock,
                                                            CancellationToken *cancel)
{
    QMutexLocker lock(&m_mutex);
    Entry *entry = ensureEntry(profileId);
    ModbusValueResult r;
    auto openDiag = ensureOpenLocked(entry, useMock);
    if (!openDiag.ok) {
        r.diagnostic = openDiag;
        return r;
    }
    if (cancel && cancel->isCancelled()) {
        r.diagnostic = CommunicationDiagnostic::fail(Codes::cancelled(), QStringLiteral("请求已取消"));
        return r;
    }
    entry->state = ConnectionState::Busy;
    r = entry->modbus->readRegisters(table, address, quantity);
    entry->state = r.diagnostic.ok ? ConnectionState::Open : ConnectionState::Failed;
    return r;
}

ModbusValueResult CommunicationManager::modbusWriteCoil(const QString &profileId, int address, bool value,
                                                        bool allowWrite, bool useMock,
                                                        CancellationToken *cancel)
{
    QMutexLocker lock(&m_mutex);
    Entry *entry = ensureEntry(profileId);
    ModbusValueResult r;
    auto openDiag = ensureOpenLocked(entry, useMock);
    if (!openDiag.ok) {
        r.diagnostic = openDiag;
        return r;
    }
    if (cancel && cancel->isCancelled()) {
        r.diagnostic = CommunicationDiagnostic::fail(Codes::cancelled(), QStringLiteral("请求已取消"));
        return r;
    }
    entry->state = ConnectionState::Busy;
    r = entry->modbus->writeCoil(address, value, allowWrite);
    entry->state = r.diagnostic.ok ? ConnectionState::Open : ConnectionState::Failed;
    return r;
}

ModbusValueResult CommunicationManager::modbusWriteRegisters(const QString &profileId, int address,
                                                             const QVector<quint16> &values,
                                                             bool allowWrite, bool useMock,
                                                             CancellationToken *cancel)
{
    QMutexLocker lock(&m_mutex);
    Entry *entry = ensureEntry(profileId);
    ModbusValueResult r;
    auto openDiag = ensureOpenLocked(entry, useMock);
    if (!openDiag.ok) {
        r.diagnostic = openDiag;
        return r;
    }
    if (cancel && cancel->isCancelled()) {
        r.diagnostic = CommunicationDiagnostic::fail(Codes::cancelled(), QStringLiteral("请求已取消"));
        return r;
    }
    entry->state = ConnectionState::Busy;
    r = entry->modbus->writeRegisters(address, values, allowWrite);
    entry->state = r.diagnostic.ok ? ConnectionState::Open : ConnectionState::Failed;
    return r;
}

void CommunicationManager::cancelAll()
{
    m_cancelAll.store(true);
}

void CommunicationManager::clearCancel()
{
    m_cancelAll.store(false);
}

bool CommunicationManager::isCancelRequested() const
{
    return m_cancelAll.load();
}

QJsonObject CommunicationManager::profilesToJson() const
{
    QMutexLocker lock(&m_mutex);
    QJsonArray arr;
    for (auto it = m_entries.cbegin(); it != m_entries.cend(); ++it) {
        if (*it)
            arr.append((*it)->profile.toJson());
    }
    return QJsonObject{{QStringLiteral("profiles"), arr}};
}

void CommunicationManager::profilesFromJson(const QJsonObject &obj)
{
    QMutexLocker lock(&m_mutex);
    m_entries.clear();
    const QJsonArray arr = obj.value(QStringLiteral("profiles")).toArray();
    for (const QJsonValue &v : arr) {
        CommunicationProfile p = CommunicationProfile::fromJson(v.toObject());
        if (p.id.isEmpty())
            continue;
        auto e = std::make_shared<Entry>();
        e->profile = p;
        m_entries.insert(p.id, e);
    }
    m_cancelAll.store(false);
}

} // namespace Communication
} // namespace Selt
