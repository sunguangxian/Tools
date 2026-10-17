#pragma once

#include <QByteArray>
#include <QString>

namespace codec {

inline quint16 modbusCrc(const char *data, int length)
{
    quint16 crc = 0xFFFF;
    for (int i = 0; i < length; ++i) {
        crc ^= static_cast<quint8>(data[i]);
        for (int bit = 0; bit < 8; ++bit) {
            if (crc & 0x0001)
                crc = static_cast<quint16>((crc >> 1) ^ 0xA001);
            else
                crc = static_cast<quint16>(crc >> 1);
        }
    }
    return crc;
}

inline QByteArray parseHex(const QString &text, QString *error)
{
    QString cleaned;
    cleaned.reserve(text.size());
    for (const QChar ch : text) {
        if (ch.isSpace() || ch == QLatin1Char(',') || ch == QLatin1Char('-'))
            continue;
        cleaned.append(ch);
    }
    if (cleaned.isEmpty())
        return {};
    if (cleaned.size() % 2 != 0) {
        if (error)
            *error = QStringLiteral("HEX 长度必须为偶数");
        return {};
    }
    QByteArray out;
    out.reserve(cleaned.size() / 2);
    for (int i = 0; i < cleaned.size(); i += 2) {
        bool ok = false;
        const int value = cleaned.mid(i, 2).toInt(&ok, 16);
        if (!ok || value < 0 || value > 255) {
            if (error)
                *error = QStringLiteral("无效 HEX：%1").arg(cleaned.mid(i, 2));
            return {};
        }
        out.append(static_cast<char>(value));
    }
    return out;
}

inline QString toHex(const QByteArray &data)
{
    return QString::fromLatin1(data.toHex(' ').toUpper());
}

// mode: 0 none, 1 Modbus CRC16, 2 ADD, 3 XOR.
// from1 is 1-based. tail is how many trailing bytes are excluded from the sum.
inline QByteArray appendChecksum(const QByteArray &data, int mode, int from1, int tail)
{
    if (mode <= 0 || data.isEmpty())
        return data;
    const int begin = qBound(0, from1 - 1, data.size());
    int end = data.size() - qMax(0, tail);
    if (end < begin)
        end = begin;
    const int length = end - begin;
    QByteArray out = data;
    if (mode == 1) {
        const quint16 crc = modbusCrc(data.constData() + begin, length);
        out.append(static_cast<char>(crc & 0xFF));
        out.append(static_cast<char>((crc >> 8) & 0xFF));
        return out;
    }
    unsigned sum = 0;
    for (int i = begin; i < end; ++i) {
        const auto byte = static_cast<quint8>(data.at(i));
        if (mode == 3)
            sum ^= byte;
        else
            sum = (sum + byte) & 0xFF;
    }
    out.append(static_cast<char>(sum & 0xFF));
    return out;
}

inline int selfTest(QString *error)
{
    QString hexError;
    const QByteArray parsed = parseHex(QStringLiteral("01 03 00 00 00 01"), &hexError);
    if (!hexError.isEmpty() || parsed.size() != 6) {
        if (error)
            *error = QStringLiteral("parseHex failed");
        return 1;
    }
    const QByteArray withCrc = appendChecksum(parsed, 1, 1, 0);
    if (withCrc.size() != 8 || static_cast<quint8>(withCrc.at(6)) != 0x84
        || static_cast<quint8>(withCrc.at(7)) != 0x0A) {
        if (error)
            *error = QStringLiteral("modbus crc mismatch: %1").arg(toHex(withCrc));
        return 2;
    }
    const QByteArray sample = QByteArray::fromHex("010203");
    const QByteArray sum = appendChecksum(sample, 2, 1, 0);
    const QByteArray xored = appendChecksum(sample, 3, 2, 0);
    if (static_cast<quint8>(sum.back()) != 0x06) {
        if (error)
            *error = QStringLiteral("add mismatch");
        return 3;
    }
    // bytes from index 2: 02 ^ 03 = 01
    if (static_cast<quint8>(xored.back()) != 0x01) {
        if (error)
            *error = QStringLiteral("xor mismatch");
        return 4;
    }
    QString bad;
    parseHex(QStringLiteral("ZZ"), &bad);
    if (bad.isEmpty()) {
        if (error)
            *error = QStringLiteral("invalid hex accepted");
        return 5;
    }
    return 0;
}

} // namespace codec
