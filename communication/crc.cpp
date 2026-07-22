#include "communication/crc.h"

namespace Selt {
namespace Communication {

quint16 ModbusCrc::compute(const QByteArray &data)
{
    quint16 crc = 0xFFFF;
    for (unsigned char b : data) {
        crc ^= quint16(b);
        for (int i = 0; i < 8; ++i) {
            if (crc & 0x0001)
                crc = quint16((crc >> 1) ^ 0xA001);
            else
                crc = quint16(crc >> 1);
        }
    }
    return crc;
}

QByteArray ModbusCrc::append(const QByteArray &pduWithoutCrc)
{
    const quint16 crc = compute(pduWithoutCrc);
    QByteArray out = pduWithoutCrc;
    out.append(char(crc & 0xFF));
    out.append(char((crc >> 8) & 0xFF));
    return out;
}

bool ModbusCrc::validate(const QByteArray &frameWithCrc, QString *error)
{
    if (frameWithCrc.size() < 3) {
        if (error)
            *error = QStringLiteral("RTU 帧过短");
        return false;
    }
    const QByteArray body = frameWithCrc.left(frameWithCrc.size() - 2);
    const quint16 expected = compute(body);
    const quint16 actual = quint16(uchar(frameWithCrc.at(frameWithCrc.size() - 2)))
                           | (quint16(uchar(frameWithCrc.at(frameWithCrc.size() - 1))) << 8);
    if (expected != actual) {
        if (error) {
            *error = QStringLiteral("CRC 校验失败: expect=0x%1 actual=0x%2")
                         .arg(expected, 4, 16, QLatin1Char('0'))
                         .arg(actual, 4, 16, QLatin1Char('0'));
        }
        return false;
    }
    return true;
}

quint16 Checksum::crc16Modbus(const QByteArray &data)
{
    return ModbusCrc::compute(data);
}

quint8 Checksum::lrc(const QByteArray &data)
{
    int sum = 0;
    for (unsigned char b : data)
        sum = (sum + b) & 0xFF;
    return quint8(((-sum) & 0xFF));
}

quint8 Checksum::xorSum(const QByteArray &data)
{
    quint8 x = 0;
    for (unsigned char b : data)
        x ^= b;
    return x;
}

quint16 Checksum::sum16(const QByteArray &data)
{
    quint32 sum = 0;
    for (unsigned char b : data)
        sum += b;
    return quint16(sum & 0xFFFF);
}

} // namespace Communication
} // namespace Selt
