#pragma once

#include "communication/types.h"

#include <QByteArray>
#include <cstdint>

namespace Selt {
namespace Communication {

/// Modbus RTU CRC16 (poly 0xA001, init 0xFFFF). Little-endian append order: low then high.
class ModbusCrc
{
public:
    static quint16 compute(const QByteArray &data);
    static QByteArray append(const QByteArray &pduWithoutCrc);
    static bool validate(const QByteArray &frameWithCrc, QString *error = nullptr);
};

/// Generic checksum helpers for frame operators.
class Checksum
{
public:
    static quint16 crc16Modbus(const QByteArray &data);
    static quint8 lrc(const QByteArray &data);
    static quint8 xorSum(const QByteArray &data);
    static quint16 sum16(const QByteArray &data);
};

} // namespace Communication
} // namespace Selt
