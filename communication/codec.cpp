#include "communication/codec.h"

#include <QChar>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace Selt {
namespace Communication {
namespace {

bool isHexDigit(QChar c)
{
    return c.isDigit() || (c >= QLatin1Char('a') && c <= QLatin1Char('f'))
           || (c >= QLatin1Char('A') && c <= QLatin1Char('F'));
}

QByteArray applyByteSwapPerWord(const QByteArray &in)
{
    QByteArray out = in;
    for (int i = 0; i + 1 < out.size(); i += 2)
        std::swap(out[i], out[i + 1]);
    return out;
}

} // namespace

QByteArray Codec::reorderWords(const QByteArray &raw, ModbusWordOrder order, int wordCount)
{
    Q_UNUSED(wordCount);
    switch (order) {
    case ModbusWordOrder::ABCD:
        return raw;
    case ModbusWordOrder::BADC:
        return applyByteSwapPerWord(raw);
    case ModbusWordOrder::CDAB: {
        QByteArray out = raw;
        if (out.size() >= 4) {
            for (int i = 0; i + 3 < out.size(); i += 4) {
                std::swap(out[i], out[i + 2]);
                std::swap(out[i + 1], out[i + 3]);
            }
        }
        return out;
    }
    case ModbusWordOrder::DCBA: {
        QByteArray out = raw;
        std::reverse(out.begin(), out.end());
        return out;
    }
    }
    return raw;
}

QByteArray Codec::restoreWordOrder(const QByteArray &ordered, ModbusWordOrder order, int wordCount)
{
    // All defined orders are involutions for 2/4/8-byte blocks used here.
    return reorderWords(ordered, order, wordCount);
}

CodecResult Codec::bytesFromText(const QString &text, PayloadEncoding encoding)
{
    CodecResult r;
    switch (encoding) {
    case PayloadEncoding::Raw:
    case PayloadEncoding::Utf8:
        r.bytes = text.toUtf8();
        return r;
    case PayloadEncoding::Ascii:
        r.bytes = text.toLatin1();
        return r;
    case PayloadEncoding::Hex:
        return hexToBytes(text);
    }
    r.diagnostic = CommunicationDiagnostic::fail(Codes::encodeError(), QStringLiteral("未知编码"));
    return r;
}

CodecResult Codec::textFromBytes(const QByteArray &bytes, PayloadEncoding encoding)
{
    CodecResult r;
    r.bytes = bytes;
    switch (encoding) {
    case PayloadEncoding::Raw:
    case PayloadEncoding::Utf8:
        r.text = QString::fromUtf8(bytes);
        return r;
    case PayloadEncoding::Ascii:
        r.text = QString::fromLatin1(bytes);
        return r;
    case PayloadEncoding::Hex:
        return bytesToHex(bytes, true);
    }
    r.diagnostic = CommunicationDiagnostic::fail(Codes::decodeError(), QStringLiteral("未知编码"));
    return r;
}

CodecResult Codec::hexToBytes(const QString &hex)
{
    CodecResult r;
    QString cleaned;
    cleaned.reserve(hex.size());
    for (QChar c : hex) {
        if (c.isSpace() || c == QLatin1Char('-') || c == QLatin1Char(':') || c == QLatin1Char(','))
            continue;
        if (!isHexDigit(c)) {
            r.diagnostic = CommunicationDiagnostic::fail(
                Codes::encodeError(), QStringLiteral("非法十六进制字符: %1").arg(c),
                {QStringLiteral("仅允许 0-9 A-F 及空格/分隔符")});
            return r;
        }
        cleaned.append(c);
    }
    if (cleaned.size() % 2 != 0) {
        r.diagnostic = CommunicationDiagnostic::fail(Codes::encodeError(),
                                                     QStringLiteral("十六进制长度必须为偶数"));
        return r;
    }
    r.bytes = QByteArray::fromHex(cleaned.toLatin1());
    r.text = cleaned.toUpper();
    return r;
}

CodecResult Codec::bytesToHex(const QByteArray &bytes, bool spaced)
{
    CodecResult r;
    r.bytes = bytes;
    const QByteArray hex = bytes.toHex().toUpper();
    if (!spaced) {
        r.text = QString::fromLatin1(hex);
        return r;
    }
    QString out;
    out.reserve(hex.size() + hex.size() / 2);
    for (int i = 0; i < hex.size(); i += 2) {
        if (i > 0)
            out.append(QLatin1Char(' '));
        out.append(QLatin1Char(hex.at(i)));
        if (i + 1 < hex.size())
            out.append(QLatin1Char(hex.at(i + 1)));
    }
    r.text = out;
    return r;
}

CodecResult Codec::decimalToAscii(qint64 value)
{
    CodecResult r;
    r.text = QString::number(value);
    r.bytes = r.text.toLatin1();
    r.intValue = value;
    return r;
}

CodecResult Codec::asciiToDecimal(const QString &text)
{
    CodecResult r;
    const QString trimmed = text.trimmed();
    bool ok = false;
    const qint64 value = trimmed.toLongLong(&ok);
    if (!ok) {
        r.diagnostic = CommunicationDiagnostic::fail(Codes::decodeError(),
                                                     QStringLiteral("无法解析十进制: %1").arg(text));
        return r;
    }
    r.intValue = value;
    r.text = trimmed;
    r.bytes = trimmed.toLatin1();
    return r;
}

CodecResult Codec::packCoils(const QVector<bool> &coils)
{
    CodecResult r;
    r.coils = coils;
    const int byteCount = (coils.size() + 7) / 8;
    r.bytes.resize(byteCount);
    r.bytes.fill(0);
    for (int i = 0; i < coils.size(); ++i) {
        if (coils.at(i))
            r.bytes[i / 8] = char(uchar(r.bytes.at(i / 8)) | (1u << (i % 8)));
    }
    return r;
}

CodecResult Codec::unpackCoils(const QByteArray &bytes, int quantity)
{
    CodecResult r;
    if (quantity < 0 || quantity > 2000) {
        r.diagnostic = CommunicationDiagnostic::fail(Codes::invalidParameter(),
                                                     QStringLiteral("线圈数量越界: %1").arg(quantity));
        return r;
    }
    const int need = (quantity + 7) / 8;
    if (bytes.size() < need) {
        r.diagnostic = CommunicationDiagnostic::fail(
            Codes::decodeError(),
            QStringLiteral("线圈字节不足: need=%1 have=%2").arg(need).arg(bytes.size()));
        return r;
    }
    r.coils.resize(quantity);
    for (int i = 0; i < quantity; ++i)
        r.coils[i] = (uchar(bytes.at(i / 8)) & (1u << (i % 8))) != 0;
    r.bytes = bytes.left(need);
    return r;
}

CodecResult Codec::packRegisters(const QVector<quint16> &registers)
{
    CodecResult r;
    r.registers = registers;
    r.bytes.resize(registers.size() * 2);
    for (int i = 0; i < registers.size(); ++i) {
        const quint16 v = registers.at(i);
        r.bytes[i * 2] = char((v >> 8) & 0xFF);
        r.bytes[i * 2 + 1] = char(v & 0xFF);
    }
    return r;
}

CodecResult Codec::unpackRegisters(const QByteArray &bytes)
{
    CodecResult r;
    if (bytes.size() % 2 != 0) {
        r.diagnostic = CommunicationDiagnostic::fail(Codes::decodeError(),
                                                     QStringLiteral("寄存器字节长度必须为偶数"));
        return r;
    }
    r.registers.resize(bytes.size() / 2);
    for (int i = 0; i < r.registers.size(); ++i) {
        const auto hi = uchar(bytes.at(i * 2));
        const auto lo = uchar(bytes.at(i * 2 + 1));
        r.registers[i] = quint16((hi << 8) | lo);
    }
    r.bytes = bytes;
    return r;
}

CodecResult Codec::encodeValue(double value, RegisterValueKind kind, ModbusWordOrder order,
                               int stringRegisterCount)
{
    Q_UNUSED(stringRegisterCount);
    CodecResult r;
    QByteArray raw;
    int words = 0;
    switch (kind) {
    case RegisterValueKind::UInt16: {
        if (value < 0 || value > 65535.0) {
            r.diagnostic = CommunicationDiagnostic::fail(Codes::encodeError(),
                                                         QStringLiteral("UInt16 越界"));
            return r;
        }
        const quint16 v = quint16(qRound64(value));
        raw.resize(2);
        raw[0] = char((v >> 8) & 0xFF);
        raw[1] = char(v & 0xFF);
        words = 1;
        break;
    }
    case RegisterValueKind::Int16: {
        if (value < -32768.0 || value > 32767.0) {
            r.diagnostic = CommunicationDiagnostic::fail(Codes::encodeError(),
                                                         QStringLiteral("Int16 越界"));
            return r;
        }
        const qint16 v = qint16(qRound64(value));
        const quint16 u = quint16(v);
        raw.resize(2);
        raw[0] = char((u >> 8) & 0xFF);
        raw[1] = char(u & 0xFF);
        words = 1;
        break;
    }
    case RegisterValueKind::UInt32: {
        if (value < 0 || value > 4294967295.0) {
            r.diagnostic = CommunicationDiagnostic::fail(Codes::encodeError(),
                                                         QStringLiteral("UInt32 越界"));
            return r;
        }
        const quint32 v = quint32(qRound64(value));
        raw.resize(4);
        qToBigEndian(v, reinterpret_cast<uchar *>(raw.data()));
        words = 2;
        break;
    }
    case RegisterValueKind::Int32: {
        if (value < -2147483648.0 || value > 2147483647.0) {
            r.diagnostic = CommunicationDiagnostic::fail(Codes::encodeError(),
                                                         QStringLiteral("Int32 越界"));
            return r;
        }
        const qint32 v = qint32(qRound64(value));
        raw.resize(4);
        qToBigEndian(v, reinterpret_cast<uchar *>(raw.data()));
        words = 2;
        break;
    }
    case RegisterValueKind::Float32: {
        const float f = float(value);
        raw.resize(4);
        quint32 bits = 0;
        static_assert(sizeof(float) == 4, "float must be 4 bytes");
        std::memcpy(&bits, &f, 4);
        qToBigEndian(bits, reinterpret_cast<uchar *>(raw.data()));
        words = 2;
        break;
    }
    case RegisterValueKind::Float64: {
        const double d = value;
        raw.resize(8);
        quint64 bits = 0;
        static_assert(sizeof(double) == 8, "double must be 8 bytes");
        std::memcpy(&bits, &d, 8);
        qToBigEndian(bits, reinterpret_cast<uchar *>(raw.data()));
        words = 4;
        break;
    }
    case RegisterValueKind::AsciiString:
    case RegisterValueKind::HexString:
        r.diagnostic = CommunicationDiagnostic::fail(
            Codes::encodeError(), QStringLiteral("字符串请使用 encodeString"));
        return r;
    }
    r.bytes = reorderWords(raw, order, words);
    auto unpacked = unpackRegisters(r.bytes);
    if (!unpacked.diagnostic.ok)
        return unpacked;
    r.registers = unpacked.registers;
    r.realValue = value;
    r.intValue = qint64(qRound64(value));
    return r;
}

CodecResult Codec::encodeString(const QString &text, RegisterValueKind kind, ModbusWordOrder order,
                                int registerCount, bool padNull)
{
    CodecResult r;
    if (registerCount <= 0 || registerCount > 123) {
        r.diagnostic = CommunicationDiagnostic::fail(Codes::invalidParameter(),
                                                     QStringLiteral("字符串寄存器数量无效"));
        return r;
    }
    QByteArray raw;
    if (kind == RegisterValueKind::HexString) {
        auto hex = hexToBytes(text);
        if (!hex.diagnostic.ok)
            return hex;
        raw = hex.bytes;
    } else {
        raw = text.toLatin1();
    }
    const int need = registerCount * 2;
    if (raw.size() > need) {
        r.diagnostic = CommunicationDiagnostic::fail(
            Codes::encodeError(),
            QStringLiteral("字符串过长: %1 > %2 字节").arg(raw.size()).arg(need));
        return r;
    }
    if (padNull)
        raw.append(QByteArray(need - raw.size(), '\0'));
    else if (raw.size() < need)
        raw.append(QByteArray(need - raw.size(), ' '));
    r.bytes = reorderWords(raw, order, registerCount);
    auto unpacked = unpackRegisters(r.bytes);
    if (!unpacked.diagnostic.ok)
        return unpacked;
    r.registers = unpacked.registers;
    r.text = text;
    return r;
}

CodecResult Codec::decodeValue(const QVector<quint16> &registers, RegisterValueKind kind,
                               ModbusWordOrder order)
{
    CodecResult r;
    r.registers = registers;
    auto packed = packRegisters(registers);
    if (!packed.diagnostic.ok)
        return packed;
    int words = 0;
    switch (kind) {
    case RegisterValueKind::UInt16:
    case RegisterValueKind::Int16:
        words = 1;
        break;
    case RegisterValueKind::UInt32:
    case RegisterValueKind::Int32:
    case RegisterValueKind::Float32:
        words = 2;
        break;
    case RegisterValueKind::Float64:
        words = 4;
        break;
    case RegisterValueKind::AsciiString:
    case RegisterValueKind::HexString:
        words = registers.size();
        break;
    }
    if (registers.size() < words) {
        r.diagnostic = CommunicationDiagnostic::fail(
            Codes::decodeError(),
            QStringLiteral("寄存器数量不足: need=%1 have=%2").arg(words).arg(registers.size()));
        return r;
    }
    QByteArray ordered = packed.bytes.left(words * 2);
    QByteArray raw = restoreWordOrder(ordered, order, words);
    r.bytes = raw;

    switch (kind) {
    case RegisterValueKind::UInt16: {
        const quint16 v = quint16((uchar(raw[0]) << 8) | uchar(raw[1]));
        r.intValue = v;
        r.realValue = v;
        r.text = QString::number(v);
        break;
    }
    case RegisterValueKind::Int16: {
        const qint16 v = qint16((uchar(raw[0]) << 8) | uchar(raw[1]));
        r.intValue = v;
        r.realValue = v;
        r.text = QString::number(v);
        break;
    }
    case RegisterValueKind::UInt32: {
        const quint32 v = qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(raw.constData()));
        r.intValue = qint64(v);
        r.realValue = double(v);
        r.text = QString::number(v);
        break;
    }
    case RegisterValueKind::Int32: {
        const qint32 v = qFromBigEndian<qint32>(reinterpret_cast<const uchar *>(raw.constData()));
        r.intValue = v;
        r.realValue = v;
        r.text = QString::number(v);
        break;
    }
    case RegisterValueKind::Float32: {
        const quint32 bits =
            qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(raw.constData()));
        float f = 0.0f;
        std::memcpy(&f, &bits, 4);
        r.realValue = double(f);
        r.intValue = qint64(qRound64(r.realValue));
        r.text = QString::number(r.realValue, 'g', 9);
        break;
    }
    case RegisterValueKind::Float64: {
        const quint64 bits =
            qFromBigEndian<quint64>(reinterpret_cast<const uchar *>(raw.constData()));
        double d = 0.0;
        std::memcpy(&d, &bits, 8);
        r.realValue = d;
        r.intValue = qint64(qRound64(d));
        r.text = QString::number(d, 'g', 17);
        break;
    }
    case RegisterValueKind::AsciiString: {
        int end = raw.size();
        while (end > 0 && (raw.at(end - 1) == '\0' || raw.at(end - 1) == ' '))
            --end;
        r.text = QString::fromLatin1(raw.constData(), end);
        break;
    }
    case RegisterValueKind::HexString: {
        auto hex = bytesToHex(raw, true);
        r.text = hex.text;
        break;
    }
    }
    return r;
}

} // namespace Communication
} // namespace Selt
