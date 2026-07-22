#pragma once

#include "communication/types.h"

#include <QByteArray>
#include <QString>
#include <QVector>

namespace Selt {
namespace Communication {

struct CodecResult
{
    CommunicationDiagnostic diagnostic;
    QByteArray bytes;
    QString text;
    QVector<bool> coils;
    QVector<quint16> registers;
    qint64 intValue{0};
    double realValue{0.0};
};

/// Pure encode/decode helpers for serial/TCP payloads and Modbus multi-register values.
class Codec
{
public:
    static CodecResult bytesFromText(const QString &text, PayloadEncoding encoding);
    static CodecResult textFromBytes(const QByteArray &bytes, PayloadEncoding encoding);

    static CodecResult packCoils(const QVector<bool> &coils);
    static CodecResult unpackCoils(const QByteArray &bytes, int quantity);

    static CodecResult packRegisters(const QVector<quint16> &registers);
    static CodecResult unpackRegisters(const QByteArray &bytes);

    static CodecResult encodeValue(double value, RegisterValueKind kind, ModbusWordOrder order,
                                   int stringRegisterCount = 0);
    static CodecResult encodeString(const QString &text, RegisterValueKind kind, ModbusWordOrder order,
                                    int registerCount, bool padNull = true);
    static CodecResult decodeValue(const QVector<quint16> &registers, RegisterValueKind kind,
                                   ModbusWordOrder order);

    static CodecResult decimalToAscii(qint64 value);
    static CodecResult asciiToDecimal(const QString &text);
    static CodecResult hexToBytes(const QString &hex);
    static CodecResult bytesToHex(const QByteArray &bytes, bool spaced = true);

    static QByteArray reorderWords(const QByteArray &raw, ModbusWordOrder order, int wordCount);
    static QByteArray restoreWordOrder(const QByteArray &ordered, ModbusWordOrder order, int wordCount);
};

} // namespace Communication
} // namespace Selt
