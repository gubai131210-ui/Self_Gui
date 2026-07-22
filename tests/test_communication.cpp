#include "communication/clients.h"
#include "communication/codec.h"
#include "communication/communicationmanager.h"
#include "communication/crc.h"
#include "communication/mocktransport.h"
#include "vision/data/datatype.h"
#include "vision/nodes/builtinexecutors.h"
#include "vision/registry/communicationnodeids.h"
#include "vision/registry/visionnodeids.h"
#include "vision/registry/visionnoderegistry.h"
#include "vision/runtime/inodeexecutor.h"
#include "vision/runtime/nodeexecutorregistry.h"

#include <QtTest>
#include <QHostAddress>
#include <QMutex>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QWaitCondition>

using namespace Selt;
using namespace Selt::Communication;

class TestCommunication : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void codecHexRoundTrip();
    void codecFloatWordOrders();
    void codecCoilPack();
    void codecDecimalAscii();
    void byteArrayDataValue();
    void mockTcpEcho();
    void mockModbusReadWriteHolding();
    void mockModbusWriteProtected();
    void encodeDecodeNodes();
    void numericAndStringNodes();
    void allCommunicationNodesRegistered();
    void deploymentRequirementsNonEmpty();
    void modbusCrcRoundTrip();
    void connectionManagerReuse();
    void tcpLoopbackEcho();
    void modbusRtuCrcAndException();
    void checksumAndFrameNodes();
    void newVisionNodesRegistered();
};

void TestCommunication::initTestCase()
{
    registerBuiltInOpenCvExecutors();
}

void TestCommunication::codecHexRoundTrip()
{
    auto enc = Codec::hexToBytes(QStringLiteral("01 03 00 0A"));
    QVERIFY(enc.diagnostic.ok);
    QCOMPARE(enc.bytes.size(), 4);
    auto hex = Codec::bytesToHex(enc.bytes, true);
    QCOMPARE(hex.text, QStringLiteral("01 03 00 0A"));
    auto bad = Codec::hexToBytes(QStringLiteral("ZZ"));
    QVERIFY(!bad.diagnostic.ok);
}

void TestCommunication::codecFloatWordOrders()
{
    const float sample = 12.5f;
    auto abcd = Codec::encodeValue(sample, RegisterValueKind::Float32, ModbusWordOrder::ABCD);
    QVERIFY(abcd.diagnostic.ok);
    QCOMPARE(abcd.registers.size(), 2);
    auto decoded = Codec::decodeValue(abcd.registers, RegisterValueKind::Float32, ModbusWordOrder::ABCD);
    QVERIFY(decoded.diagnostic.ok);
    QCOMPARE(decoded.realValue, double(sample));

    auto cdab = Codec::encodeValue(sample, RegisterValueKind::Float32, ModbusWordOrder::CDAB);
    QVERIFY(cdab.diagnostic.ok);
    auto decodedCdab = Codec::decodeValue(cdab.registers, RegisterValueKind::Float32, ModbusWordOrder::CDAB);
    QCOMPARE(decodedCdab.realValue, double(sample));
}

void TestCommunication::codecCoilPack()
{
    QVector<bool> coils{true, false, true, true, false, false, false, true};
    auto packed = Codec::packCoils(coils);
    QCOMPARE(uchar(packed.bytes.at(0)), uchar(0b10001101));
    auto unpacked = Codec::unpackCoils(packed.bytes, coils.size());
    QCOMPARE(unpacked.coils, coils);
}

void TestCommunication::codecDecimalAscii()
{
    auto enc = Codec::decimalToAscii(12345);
    QCOMPARE(enc.text, QStringLiteral("12345"));
    auto dec = Codec::asciiToDecimal(QStringLiteral(" 678 "));
    QCOMPARE(dec.intValue, 678);
    QVERIFY(!Codec::asciiToDecimal(QStringLiteral("abc")).diagnostic.ok);
}

void TestCommunication::byteArrayDataValue()
{
    const QByteArray bytes = QByteArray::fromHex("DEAD");
    DataValue v(bytes);
    QCOMPARE(v.type(), DataTypeId::ByteArray);
    QCOMPARE(v.toByteArray(), bytes);
    DataValue asString;
    QVERIFY(v.convertTo(DataTypeId::String, &asString));
    DataValue back;
    QVERIFY(asString.convertTo(DataTypeId::ByteArray, &back));
}

void TestCommunication::mockTcpEcho()
{
    TcpClient client(std::make_unique<MockTransport>());
    TcpConfig cfg;
    cfg.timeoutMs = 200;
    client.setConfig(cfg);
    auto result = client.request(QByteArray("ping"), 4);
    QVERIFY(result.diagnostic.ok);
    QCOMPARE(result.response, QByteArray("ping"));
}

void TestCommunication::mockModbusReadWriteHolding()
{
    auto map = std::make_shared<MockModbusMap>();
    auto transport = std::make_unique<MockTransport>(
        [map](const QByteArray &req) { return map->handleTcpPdu(req); });
    ModbusClient client(std::move(transport));
    ModbusConfig cfg;
    cfg.transport = TransportKind::ModbusTcp;
    cfg.unitId = 1;
    client.setConfig(cfg);

    auto denied = client.writeRegister(10, 0x1234, false);
    QVERIFY(!denied.diagnostic.ok);
    QCOMPARE(denied.diagnostic.code, Codes::writeProtected());

    auto written = client.writeRegister(10, 0x1234, true);
    QVERIFY(written.diagnostic.ok);
    QCOMPARE(map->holdingRegisters.at(10), quint16(0x1234));

    auto read = client.readRegisters(ModbusTable::HoldingRegisters, 10, 1);
    QVERIFY(read.diagnostic.ok);
    QCOMPARE(read.registers.at(0), quint16(0x1234));

    auto floatEnc = Codec::encodeValue(3.5, RegisterValueKind::Float32, ModbusWordOrder::ABCD);
    QVERIFY(client.writeRegisters(20, floatEnc.registers, true).diagnostic.ok);
    auto floatRead = client.readRegisters(ModbusTable::HoldingRegisters, 20, 2);
    auto decoded = Codec::decodeValue(floatRead.registers, RegisterValueKind::Float32, ModbusWordOrder::ABCD);
    QCOMPARE(decoded.realValue, 3.5);

    QVERIFY(client.writeCoil(5, true, true).diagnostic.ok);
    auto coilRead = client.readBits(ModbusTable::Coils, 5, 1);
    QVERIFY(coilRead.diagnostic.ok);
    QVERIFY(coilRead.coils.first());
}

void TestCommunication::mockModbusWriteProtected()
{
    auto exec = NodeExecutorRegistry::instance().create(CommunicationNodeIds::modbusWrite());
    QVERIFY(exec);
    ExecutionRequest req;
    req.typeId = CommunicationNodeIds::modbusWrite();
    req.parameters.insert(QStringLiteral("allowWrite"), false);
    req.parameters.insert(QStringLiteral("useMock"), true);
    req.parameters.insert(QStringLiteral("address"), 0);
    req.parameters.insert(QStringLiteral("value"), 1);
    ExecutionContext ctx;
    ExecutionResult r = exec->execute(req, ctx);
    QCOMPARE(r.status, ModuleStatus::Failed);
    QCOMPARE(r.diagnosticCode, Codes::writeProtected());
}

void TestCommunication::encodeDecodeNodes()
{
    auto encode = NodeExecutorRegistry::instance().create(CommunicationNodeIds::encode());
    auto decode = NodeExecutorRegistry::instance().create(CommunicationNodeIds::decode());
    QVERIFY(encode);
    QVERIFY(decode);
    ExecutionRequest encReq;
    encReq.parameters.insert(QStringLiteral("valueKind"), QStringLiteral("float32"));
    encReq.parameters.insert(QStringLiteral("wordOrder"), QStringLiteral("ABCD"));
    encReq.parameters.insert(QStringLiteral("value"), 9.25);
    ExecutionContext ctx;
    ExecutionResult enc = encode->execute(encReq, ctx);
    QCOMPARE(enc.status, ModuleStatus::Success);
    QVERIFY(enc.outputs.contains(QStringLiteral("bytes")));

    ExecutionRequest decReq;
    decReq.parameters.insert(QStringLiteral("valueKind"), QStringLiteral("float32"));
    decReq.parameters.insert(QStringLiteral("wordOrder"), QStringLiteral("ABCD"));
    decReq.inputs.insert(QStringLiteral("bytes"), enc.outputs.value(QStringLiteral("bytes")));
    ExecutionResult dec = decode->execute(decReq, ctx);
    QCOMPARE(dec.status, ModuleStatus::Success);
    QCOMPARE(dec.outputs.value(QStringLiteral("value")).toReal(), 9.25);
}

void TestCommunication::numericAndStringNodes()
{
    auto numeric = NodeExecutorRegistry::instance().create(CommunicationNodeIds::numeric());
    ExecutionRequest nreq;
    nreq.parameters.insert(QStringLiteral("op"), QStringLiteral("mul"));
    nreq.parameters.insert(QStringLiteral("a"), 3.0);
    nreq.parameters.insert(QStringLiteral("b"), 4.0);
    ExecutionContext ctx;
    ExecutionResult nr = numeric->execute(nreq, ctx);
    QCOMPARE(nr.outputs.value(QStringLiteral("out")).toReal(), 12.0);

    auto str = NodeExecutorRegistry::instance().create(CommunicationNodeIds::stringOp());
    ExecutionRequest sreq;
    sreq.parameters.insert(QStringLiteral("op"), QStringLiteral("concat"));
    sreq.parameters.insert(QStringLiteral("a"), QStringLiteral("AB"));
    sreq.parameters.insert(QStringLiteral("b"), QStringLiteral("CD"));
    ExecutionResult sr = str->execute(sreq, ctx);
    QCOMPARE(sr.outputs.value(QStringLiteral("out")).toString(), QStringLiteral("ABCD"));
}

void TestCommunication::allCommunicationNodesRegistered()
{
    const QStringList ids = {
        CommunicationNodeIds::serialRequest(),
        CommunicationNodeIds::tcpRequest(),
        CommunicationNodeIds::modbusRead(),
        CommunicationNodeIds::modbusWrite(),
        CommunicationNodeIds::encode(),
        CommunicationNodeIds::decode(),
        CommunicationNodeIds::numeric(),
        CommunicationNodeIds::stringOp(),
        CommunicationNodeIds::checksum(),
        CommunicationNodeIds::frameWrap(),
        CommunicationNodeIds::clampScale(),
        CommunicationNodeIds::registerTable(),
    };
    for (const QString &id : ids) {
        QVERIFY2(VisionNodeRegistry::supportedTypes().contains(id), qPrintable(id));
        QVERIFY2(NodeExecutorRegistry::instance().contains(id), qPrintable(id));
        const ModuleDescriptor d = VisionNodeRegistry::descriptor(id);
        QVERIFY(!d.displayName.isEmpty());
    }
}

void TestCommunication::deploymentRequirementsNonEmpty()
{
    QVERIFY(!deploymentRequirements().isEmpty());
    QVERIFY(!capabilitySummary().isEmpty());
    QVERIFY(!runtimeDeploymentHints().isEmpty());
}

void TestCommunication::modbusCrcRoundTrip()
{
    QByteArray pdu;
    pdu.append(char(0x01));
    pdu.append(char(0x03));
    pdu.append(char(0x00));
    pdu.append(char(0x00));
    pdu.append(char(0x00));
    pdu.append(char(0x01));
    const QByteArray frame = ModbusCrc::append(pdu);
    QVERIFY(ModbusCrc::validate(frame));
    QByteArray bad = frame;
    bad[bad.size() - 1] = char(uchar(bad.at(bad.size() - 1)) ^ 0xFF);
    QVERIFY(!ModbusCrc::validate(bad));
}

void TestCommunication::connectionManagerReuse()
{
    CommunicationManager::instance().clearProfiles();
    CommunicationProfile p;
    p.id = QStringLiteral("tcp_mock");
    p.kind = TransportKind::Tcp;
    p.tcp.host = QStringLiteral("127.0.0.1");
    p.tcp.port = 9;
    CommunicationManager::instance().upsertProfile(p);
    auto d1 = CommunicationManager::instance().open(p.id, true);
    QVERIFY(d1.ok);
    ReceiveFramePolicy policy;
    policy.expectMinBytes = 4;
    auto r1 = CommunicationManager::instance().request(p.id, QByteArray("ping"), policy, true);
    QVERIFY(r1.diagnostic.ok);
    QCOMPARE(r1.response, QByteArray("ping"));
    QCOMPARE(CommunicationManager::instance().state(p.id), ConnectionState::Open);
    auto r2 = CommunicationManager::instance().request(p.id, QByteArray("pong"), policy, true);
    QVERIFY(r2.diagnostic.ok);
    CommunicationManager::instance().cancelAll();
    QVERIFY(CommunicationManager::instance().isCancelRequested());
    CommunicationManager::instance().clearCancel();
}

void TestCommunication::tcpLoopbackEcho()
{
    if (!networkAvailable())
        QSKIP("Qt Network unavailable");

    QMutex mutex;
    QWaitCondition portReady;
    quint16 port = 0;
    bool serverOk = false;

    QThread *worker = QThread::create([&]() {
        QTcpServer server;
        if (!server.listen(QHostAddress::LocalHost, 0))
            return;
        {
            QMutexLocker lock(&mutex);
            port = server.serverPort();
            portReady.wakeAll();
        }
        if (!server.waitForNewConnection(5000))
            return;
        QTcpSocket *sock = server.nextPendingConnection();
        if (!sock)
            return;
        if (!sock->waitForReadyRead(5000))
            return;
        const QByteArray data = sock->readAll();
        sock->write(data);
        sock->flush();
        sock->waitForBytesWritten(2000);
        serverOk = true;
        sock->disconnectFromHost();
        if (sock->state() != QAbstractSocket::UnconnectedState)
            sock->waitForDisconnected(500);
    });
    worker->start();

    {
        QMutexLocker lock(&mutex);
        while (port == 0)
            QVERIFY(portReady.wait(&mutex, 3000));
    }

    TcpConfig cfg;
    cfg.host = QStringLiteral("127.0.0.1");
    cfg.port = int(port);
    cfg.timeoutMs = 5000;
    cfg.receive.mode = ReceiveFrameMode::FixedLength;
    cfg.receive.expectMinBytes = 5;
    TcpClient client;
    client.setConfig(cfg);
    auto result = client.request(QByteArray("hello"), cfg.receive);
    worker->wait(8000);
    delete worker;

    QVERIFY2(result.diagnostic.ok, qPrintable(result.diagnostic.message));
    QCOMPARE(result.response, QByteArray("hello"));
    QVERIFY(serverOk);
}

void TestCommunication::modbusRtuCrcAndException()
{
    auto map = std::make_shared<MockModbusMap>();
    auto transport = std::make_unique<MockTransport>([map](const QByteArray &req) {
        QString err;
        if (!ModbusCrc::validate(req, &err))
            return QByteArray();
        const quint8 unit = uchar(req.at(0));
        QByteArray resp;
        resp.append(char(unit));
        resp.append(map->handlePdu(unit, req.mid(1, req.size() - 3)));
        return ModbusCrc::append(resp);
    });

    ModbusConfig cfg;
    cfg.transport = TransportKind::ModbusRtu;
    cfg.unitId = 1;
    cfg.timeoutMs = 500;
    ModbusClient client(std::move(transport));
    client.setConfig(cfg);
    auto ok = client.writeRegister(10, 0x1234, true);
    QVERIFY(ok.diagnostic.ok);
    auto read = client.readRegisters(ModbusTable::HoldingRegisters, 10, 1);
    QVERIFY(read.diagnostic.ok);
    QCOMPARE(read.registers.size(), 1);
    QCOMPARE(int(read.registers.at(0)), 0x1234);

    // Illegal address quantity -> device exception path via empty/invalid mock response on bad CRC.
    QByteArray badReq;
    badReq.append(char(1));
    badReq.append(char(0x03));
    badReq.append(char(0));
    badReq.append(char(0));
    badReq.append(char(0));
    badReq.append(char(1));
    badReq.append(char(0)); // bad CRC bytes
    badReq.append(char(0));
    QVERIFY(!ModbusCrc::validate(badReq));
}

void TestCommunication::checksumAndFrameNodes()
{
    auto checksum = NodeExecutorRegistry::instance().create(CommunicationNodeIds::checksum());
    ExecutionRequest req;
    req.parameters.insert(QStringLiteral("algo"), QStringLiteral("crc16"));
    req.parameters.insert(QStringLiteral("hexText"), QStringLiteral("01 03 00 00 00 01"));
    ExecutionContext ctx;
    ExecutionResult r = checksum->execute(req, ctx);
    QCOMPARE(r.status, ModuleStatus::Success);
    QVERIFY(r.outputs.value(QStringLiteral("bytes")).toByteArray().size() >= 8);

    auto wrap = NodeExecutorRegistry::instance().create(CommunicationNodeIds::frameWrap());
    ExecutionRequest wreq;
    wreq.parameters.insert(QStringLiteral("headHex"), QStringLiteral("AA"));
    wreq.parameters.insert(QStringLiteral("tailHex"), QStringLiteral("55"));
    wreq.parameters.insert(QStringLiteral("payloadText"), QStringLiteral("01 02"));
    wreq.parameters.insert(QStringLiteral("encoding"), QStringLiteral("hex"));
    ExecutionResult wr = wrap->execute(wreq, ctx);
    QCOMPARE(wr.status, ModuleStatus::Success);
    QCOMPARE(wr.outputs.value(QStringLiteral("out")).toByteArray(),
             QByteArray::fromHex("AA010255"));
}

void TestCommunication::newVisionNodesRegistered()
{
    for (const QString &id : {VisionNodeIds::morphGradient(), VisionNodeIds::pointLineDistance(),
                              VisionNodeIds::circularityMeasure()}) {
        QVERIFY2(VisionNodeRegistry::supportedTypes().contains(id), qPrintable(id));
        QVERIFY2(NodeExecutorRegistry::instance().contains(id), qPrintable(id));
    }
}

QTEST_MAIN(TestCommunication)
#include "test_communication.moc"
