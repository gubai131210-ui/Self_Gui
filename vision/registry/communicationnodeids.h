#ifndef COMMUNICATIONNODEIDS_H
#define COMMUNICATIONNODEIDS_H

#include <QString>

namespace CommunicationNodeIds {
inline QString serialRequest() { return QStringLiteral("communication.serial.request"); }
inline QString tcpRequest() { return QStringLiteral("communication.tcp.request"); }
inline QString modbusRead() { return QStringLiteral("communication.modbus.read"); }
inline QString modbusWrite() { return QStringLiteral("communication.modbus.write"); }
inline QString encode() { return QStringLiteral("communication.encode"); }
inline QString decode() { return QStringLiteral("communication.decode"); }
inline QString numeric() { return QStringLiteral("communication.numeric"); }
inline QString stringOp() { return QStringLiteral("communication.string"); }
inline QString checksum() { return QStringLiteral("communication.checksum"); }
inline QString frameWrap() { return QStringLiteral("communication.frame.wrap"); }
inline QString clampScale() { return QStringLiteral("communication.clampScale"); }
inline QString registerTable() { return QStringLiteral("communication.registerTable"); }

inline bool isCommunicationType(const QString &type)
{
    return type.startsWith(QStringLiteral("communication."));
}
} // namespace CommunicationNodeIds

#endif // COMMUNICATIONNODEIDS_H
