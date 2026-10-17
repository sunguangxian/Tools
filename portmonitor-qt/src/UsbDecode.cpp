#include "UsbDecode.h"

#include <QtEndian>

#include <cstring>

namespace {

quint16 r16(const QByteArray &b, int o)
{
    return quint16(quint8(b[o])) | (quint16(quint8(b[o + 1])) << 8);
}

quint32 r32(const QByteArray &b, int o)
{
    return quint32(quint8(b[o])) | (quint32(quint8(b[o + 1])) << 8) | (quint32(quint8(b[o + 2])) << 16)
        | (quint32(quint8(b[o + 3])) << 24);
}

quint64 r64(const QByteArray &b, int o)
{
    quint64 v = 0;
    for (int i = 7; i >= 0; --i)
        v = (v << 8) | quint8(b[o + i]);
    return v;
}

quint32 be16(const QByteArray &s, int o)
{
    if (s.size() < o + 2)
        return 0;
    return (quint32(quint8(s[o])) << 8) | quint8(s[o + 1]);
}

quint32 be32(const QByteArray &s, int o)
{
    if (s.size() < o + 4)
        return 0;
    return (quint32(quint8(s[o])) << 24) | (quint32(quint8(s[o + 1])) << 16) | (quint32(quint8(s[o + 2])) << 8)
        | quint8(s[o + 3]);
}

quint64 be64(const QByteArray &s, int o)
{
    if (s.size() < o + 8)
        return 0;
    quint64 v = 0;
    for (int i = 0; i < 8; ++i)
        v = (v << 8) | quint8(s[o + i]);
    return v;
}

QString asciiField(const QByteArray &s)
{
    QString out;
    out.reserve(s.size());
    for (char c : s)
        out.append((uchar(c) >= 0x20 && uchar(c) < 0x7F) ? QChar(c) : QChar(' '));
    return out.trimmed();
}

QString hexBytes(const QByteArray &s)
{
    return QString::fromLatin1(s.toHex().toUpper());
}

QString hexN(quint64 v, int width)
{
    return QString::number(v, 16).rightJustified(width, QLatin1Char('0')).toUpper();
}

QString deviceType(int t)
{
    switch (t) {
    case 0x00: return QStringLiteral("直接访问(磁盘)");
    case 0x05: return QStringLiteral("CD/DVD");
    case 0x07: return QStringLiteral("光存储");
    case 0x0E: return QStringLiteral("简化直接访问");
    default: return QStringLiteral("0x") + hexN(quint64(t), 2);
    }
}

QString capacityText(quint64 lastLba, quint32 blockSize)
{
    const quint64 bytes = (lastLba + 1) * quint64(blockSize);
    return QStringLiteral("最后 LBA=%1 块大小=%2 容量=%3").arg(lastLba).arg(blockSize).arg(formatSize(bytes));
}

QString senseKeyText(int key)
{
    switch (key) {
    case 0x0: return QStringLiteral("NO SENSE");
    case 0x1: return QStringLiteral("RECOVERED ERROR");
    case 0x2: return QStringLiteral("NOT READY");
    case 0x3: return QStringLiteral("MEDIUM ERROR");
    case 0x4: return QStringLiteral("HARDWARE ERROR");
    case 0x5: return QStringLiteral("ILLEGAL REQUEST");
    case 0x6: return QStringLiteral("UNIT ATTENTION");
    case 0x7: return QStringLiteral("DATA PROTECT");
    case 0x8: return QStringLiteral("BLANK CHECK");
    case 0xB: return QStringLiteral("ABORTED COMMAND");
    case 0xE: return QStringLiteral("MISCOMPARE");
    default: return QStringLiteral("KEY 0x") + hexN(quint64(key), 1);
    }
}

QString ascText(int asc, int ascq)
{
    if (asc == 0x04 && ascq == 0x01) return QStringLiteral("(正在就绪)");
    if (asc == 0x04) return QStringLiteral("(逻辑单元未就绪)");
    if (asc == 0x11 && ascq == 0x00) return QStringLiteral("(不可恢复读错误)");
    if (asc == 0x1A && ascq == 0x00) return QStringLiteral("(参数列表长度错误)");
    if (asc == 0x20 && ascq == 0x00) return QStringLiteral("(无效操作码)");
    if (asc == 0x21 && ascq == 0x00) return QStringLiteral("(LBA 越界)");
    if (asc == 0x24 && ascq == 0x00) return QStringLiteral("(CDB 字段无效)");
    if (asc == 0x25 && ascq == 0x00) return QStringLiteral("(LUN 不支持)");
    if (asc == 0x27 && ascq == 0x00) return QStringLiteral("(写保护)");
    if (asc == 0x28 && ascq == 0x00) return QStringLiteral("(介质可能已更换)");
    if (asc == 0x29) return QStringLiteral("(上电/复位)");
    if (asc == 0x3A) return QStringLiteral("(无介质)");
    return {};
}

QString decodeSense(const QByteArray &d)
{
    if (d.size() < 4)
        return {};
    const int code = quint8(d[0]) & 0x7F;
    int key = 0, asc = 0, ascq = 0;
    if (code == 0x70 || code == 0x71) {
        if (d.size() < 14)
            return {};
        key = quint8(d[2]) & 0x0F;
        asc = quint8(d[12]);
        ascq = quint8(d[13]);
    } else if (code == 0x72 || code == 0x73) {
        key = quint8(d[1]) & 0x0F;
        asc = quint8(d[2]);
        ascq = quint8(d[3]);
    } else {
        return {};
    }
    return QStringLiteral("Sense: %1 ASC/ASCQ=%2/%3 %4")
        .arg(senseKeyText(key))
        .arg(hexN(quint64(asc), 2))
        .arg(hexN(quint64(ascq), 2))
        .arg(ascText(asc, ascq))
        .trimmed();
}

} // namespace

QString formatSize(quint64 bytes)
{
    static const char *units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
    double v = double(bytes);
    int i = 0;
    while (v >= 1024.0 && i < 5) {
        v /= 1024.0;
        ++i;
    }
    QString s = QString::number(v, 'f', 2);
    while (s.contains(QLatin1Char('.')) && (s.endsWith(QLatin1Char('0')) || s.endsWith(QLatin1Char('.'))))
        s.chop(1);
    return s + QLatin1Char(' ') + QLatin1String(units[i]);
}

QString UsbPacket::address() const
{
    return QStringLiteral("%1.%2.%3").arg(bus).arg(device).arg(endpointNumber());
}

QString UsbPacket::directionLabel() const
{
    return isIn() ? QStringLiteral("IN") : QStringLiteral("OUT");
}

QString UsbPacket::stageLabel() const
{
    return isCompletion() ? QStringLiteral("完成") : QStringLiteral("提交");
}

QString UsbPacket::transferLabel() const
{
    switch (transfer) {
    case UsbTransfer::Iso: return QStringLiteral("ISO");
    case UsbTransfer::Interrupt: return QStringLiteral("INT");
    case UsbTransfer::Control: return QStringLiteral("CTRL");
    case UsbTransfer::Bulk: return QStringLiteral("BULK");
    default: return QStringLiteral("?");
    }
}

QString UsbPacket::statusLabel() const
{
    return status == 0 ? QStringLiteral("OK") : QStringLiteral("0x") + hexN(status, 8);
}

QString urbFunctionName(quint16 function)
{
    switch (function) {
    case 0x0000: return QStringLiteral("SELECT_CONFIGURATION");
    case 0x0001: return QStringLiteral("SELECT_INTERFACE");
    case 0x0008: return QStringLiteral("CONTROL_TRANSFER");
    case 0x0009: return QStringLiteral("BULK_OR_INTERRUPT_TRANSFER");
    case 0x000A: return QStringLiteral("ISOCH_TRANSFER");
    case 0x000B: return QStringLiteral("GET_DESCRIPTOR_FROM_DEVICE");
    default: return QStringLiteral("URB 0x") + hexN(function, 4);
    }
}

std::optional<UsbPacket> parseUsbPcapFrame(const QByteArray &frame, qint64 unixUs, qint64 index)
{
    constexpr int kBase = 27;
    if (frame.size() < kBase)
        return std::nullopt;
    const quint16 headerLen = r16(frame, 0);
    if (headerLen < kBase || headerLen > frame.size())
        return std::nullopt;

    UsbPacket p;
    p.index = index;
    p.unixUs = unixUs;
    p.headerLength = headerLen;
    p.irpId = r64(frame, 2);
    p.status = r32(frame, 10);
    p.function = r16(frame, 14);
    p.info = quint8(frame[16]);
    p.bus = r16(frame, 17);
    p.device = r16(frame, 19);
    p.endpoint = quint8(frame[21]);
    const quint8 transfer = quint8(frame[22]);
    p.transfer = transfer <= 3 ? UsbTransfer(transfer) : UsbTransfer::Unknown;
    p.dataLength = r32(frame, 23);
    if (p.transfer == UsbTransfer::Control && headerLen >= 28)
        p.stage = quint8(frame[27]);
    QByteArray payload = frame.mid(headerLen);
    if (quint32(payload.size()) > p.dataLength)
        payload.truncate(int(p.dataLength));
    p.data = payload;
    return p;
}

QByteArray buildUsbPcapFrame(quint64 irp, quint32 status, quint8 info, quint16 bus, quint16 device,
                             quint8 endpoint, UsbTransfer transfer, const QByteArray &data, quint16 function)
{
    QByteArray buf(27 + data.size(), 0);
    qToLittleEndian<quint16>(27, buf.data());
    qToLittleEndian<quint64>(irp, buf.data() + 2);
    qToLittleEndian<quint32>(status, buf.data() + 10);
    qToLittleEndian<quint16>(function, buf.data() + 14);
    buf[16] = char(info);
    qToLittleEndian<quint16>(bus, buf.data() + 17);
    qToLittleEndian<quint16>(device, buf.data() + 19);
    buf[21] = char(endpoint);
    buf[22] = char(transfer);
    qToLittleEndian<quint32>(quint32(data.size()), buf.data() + 23);
    if (!data.isEmpty())
        memcpy(buf.data() + 27, data.constData(), size_t(data.size()));
    return buf;
}

CdbInfo decodeCdb(const QByteArray &cdb)
{
    CdbInfo info;
    if (cdb.isEmpty()) {
        info.name = QStringLiteral("(空 CDB)");
        return info;
    }
    const quint8 op = quint8(cdb[0]);
    info.opcode = op;
    switch (op) {
    case 0x00:
        info.name = QStringLiteral("TEST UNIT READY");
        break;
    case 0x03:
        info.name = QStringLiteral("REQUEST SENSE");
        info.detail = QStringLiteral("分配长度=%1").arg(cdb.size() > 4 ? quint8(cdb[4]) : 0);
        break;
    case 0x12: {
        const bool evpd = cdb.size() > 1 && (quint8(cdb[1]) & 1) != 0;
        const QString d = evpd ? QStringLiteral("VPD 页=0x") + hexN(cdb.size() > 2 ? quint8(cdb[2]) : 0, 2)
                               : QStringLiteral("标准");
        info.name = QStringLiteral("INQUIRY");
        info.detail = QStringLiteral("%1 分配长度=%2").arg(d).arg(be16(cdb, 3));
        break;
    }
    case 0x1A:
        info.name = QStringLiteral("MODE SENSE(6)");
        info.detail = QStringLiteral("页=0x%1 分配长度=%2")
                          .arg(hexN(cdb.size() > 2 ? (quint8(cdb[2]) & 0x3F) : 0, 2))
                          .arg(cdb.size() > 4 ? quint8(cdb[4]) : 0);
        break;
    case 0x5A:
        info.name = QStringLiteral("MODE SENSE(10)");
        info.detail = QStringLiteral("页=0x%1 分配长度=%2")
                          .arg(hexN(cdb.size() > 2 ? (quint8(cdb[2]) & 0x3F) : 0, 2))
                          .arg(be16(cdb, 7));
        break;
    case 0x1B: {
        const quint8 b4 = cdb.size() > 4 ? quint8(cdb[4]) : 0;
        const bool start = (b4 & 1) != 0;
        const bool loej = (b4 & 2) != 0;
        QString act = loej ? (start ? QStringLiteral("装载介质") : QStringLiteral("弹出介质"))
                           : (start ? QStringLiteral("启动") : QStringLiteral("停止"));
        info.name = QStringLiteral("START STOP UNIT");
        info.detail = act;
        break;
    }
    case 0x1E:
        info.name = QStringLiteral("PREVENT ALLOW MEDIUM REMOVAL");
        info.detail = (cdb.size() > 4 && (quint8(cdb[4]) & 3) != 0) ? QStringLiteral("禁止移除")
                                                                     : QStringLiteral("允许移除");
        break;
    case 0x23:
        info.name = QStringLiteral("READ FORMAT CAPACITIES");
        info.detail = QStringLiteral("分配长度=%1").arg(be16(cdb, 7));
        break;
    case 0x25:
        info.name = QStringLiteral("READ CAPACITY(10)");
        break;
    case 0x9E:
        if (cdb.size() > 1 && (quint8(cdb[1]) & 0x1F) == 0x10) {
            info.name = QStringLiteral("READ CAPACITY(16)");
            info.detail = QStringLiteral("分配长度=%1").arg(be32(cdb, 10));
            break;
        }
        info.name = QStringLiteral("OP 0x9E");
        info.detail = QStringLiteral("CDB=") + hexBytes(cdb);
        break;
    case 0x28:
        info.name = QStringLiteral("READ(10)");
        info.lba = be32(cdb, 2);
        info.blocks = be16(cdb, 7);
        break;
    case 0x2A:
        info.name = QStringLiteral("WRITE(10)");
        info.lba = be32(cdb, 2);
        info.blocks = be16(cdb, 7);
        break;
    case 0xA8:
        info.name = QStringLiteral("READ(12)");
        info.lba = be32(cdb, 2);
        info.blocks = be32(cdb, 6);
        break;
    case 0xAA:
        info.name = QStringLiteral("WRITE(12)");
        info.lba = be32(cdb, 2);
        info.blocks = be32(cdb, 6);
        break;
    case 0x88:
        info.name = QStringLiteral("READ(16)");
        info.lba = be64(cdb, 2);
        info.blocks = be32(cdb, 10);
        break;
    case 0x8A:
        info.name = QStringLiteral("WRITE(16)");
        info.lba = be64(cdb, 2);
        info.blocks = be32(cdb, 10);
        break;
    case 0x35:
        info.name = QStringLiteral("SYNCHRONIZE CACHE(10)");
        info.lba = be32(cdb, 2);
        info.blocks = be16(cdb, 7);
        break;
    case 0x91:
        info.name = QStringLiteral("SYNCHRONIZE CACHE(16)");
        info.lba = be64(cdb, 2);
        info.blocks = be32(cdb, 10);
        break;
    default:
        info.name = QStringLiteral("OP 0x") + hexN(op, 2);
        info.detail = QStringLiteral("CDB=") + hexBytes(cdb);
        break;
    }
    return info;
}

QString decodeResponse(quint8 opcode, const QByteArray &cdb, const QByteArray &data)
{
    if (data.isEmpty())
        return {};
    switch (opcode) {
    case 0x12:
        if (data.size() >= 36 && !(cdb.size() > 1 && (quint8(cdb[1]) & 1) != 0)) {
            const int type = quint8(data[0]) & 0x1F;
            const bool rmb = (quint8(data[1]) & 0x80) != 0;
            return QStringLiteral("厂商=\"%1\" 产品=\"%2\" 版本=\"%3\" 类型=%4%5")
                .arg(asciiField(data.mid(8, 8)))
                .arg(asciiField(data.mid(16, 16)))
                .arg(asciiField(data.mid(32, 4)))
                .arg(deviceType(type))
                .arg(rmb ? QStringLiteral(" 可移动") : QString());
        }
        break;
    case 0x25:
        if (data.size() >= 8)
            return capacityText(be32(data, 0), be32(data, 4));
        break;
    case 0x9E:
        if (data.size() >= 12)
            return capacityText(be64(data, 0), be32(data, 8));
        break;
    case 0x03:
        return decodeSense(data);
    case 0x23:
        if (data.size() >= 12) {
            const quint32 blocks = be32(data, 4);
            const int kind = quint8(data[8]) & 3;
            const quint32 bl = (quint32(quint8(data[9])) << 16) | (quint32(quint8(data[10])) << 8) | quint8(data[11]);
            const QString k = kind == 1 ? QStringLiteral("未格式化")
                             : kind == 2 ? QStringLiteral("已格式化")
                             : kind == 3 ? QStringLiteral("无介质")
                                         : QStringLiteral("保留");
            return QStringLiteral("块数=%1 块大小=%2 (%3) %4").arg(blocks).arg(bl).arg(formatSize(quint64(blocks) * bl)).arg(k);
        }
        break;
    case 0x1A:
        if (data.size() >= 4)
            return (quint8(data[2]) & 0x80) ? QStringLiteral("写保护(WP)") : QStringLiteral("可写");
        break;
    case 0x5A:
        if (data.size() >= 8)
            return (quint8(data[3]) & 0x80) ? QStringLiteral("写保护(WP)") : QStringLiteral("可写");
        break;
    default:
        break;
    }
    return {};
}

QString ScsiCommand::deviceLabel() const
{
    return QStringLiteral("%1.%2").arg(bus).arg(device);
}

QString ScsiCommand::statusLabel() const
{
    switch (status) {
    case CswStatus::Passed: return QStringLiteral("Passed");
    case CswStatus::Failed: return QStringLiteral("Failed");
    case CswStatus::PhaseError: return QStringLiteral("Phase Error");
    default: return QStringLiteral("未完成");
    }
}

QString ScsiCommand::summary() const
{
    QStringList parts;
    if (!info.detail.isEmpty())
        parts.append(info.detail);
    if (!response.isEmpty())
        parts.append(response);
    if (!warning.isEmpty())
        parts.append(QStringLiteral("⚠ ") + warning);
    parts.append(QStringLiteral("%1 期望 %2 B / 实际 %3 B").arg(dataIn ? QStringLiteral("IN") : QStringLiteral("OUT")).arg(expectedLength).arg(dataBytes));
    return parts.join(QStringLiteral(" | "));
}

std::optional<double> ScsiCommand::durationMs() const
{
    if (completedUs < 0)
        return std::nullopt;
    return double(completedUs - unixUs) / 1000.0;
}

void MassStorageDecoder::reset()
{
    m_pending.clear();
    m_index = 0;
}

MscUpdate MassStorageDecoder::push(const UsbPacket &p)
{
    MscUpdate upd;
    if (p.transfer != UsbTransfer::Bulk || p.data.isEmpty())
        return upd;

    // Bulk OUT payload is on the submission, bulk IN payload on the completion.
    const bool carriesData = p.isIn() ? p.isCompletion() : !p.isCompletion();
    if (!carriesData)
        return upd;

    const QPair<quint16, quint16> key(p.bus, p.device);
    const QByteArray &d = p.data;

    if (!p.isIn() && d.size() == 31 && r32(d, 0) == 0x43425355) {
        int cbLen = d[14] & 0x1F;
        if (cbLen < 1)
            cbLen = 1;
        if (cbLen > 16)
            cbLen = 16;
        if (15 + cbLen > d.size())
            cbLen = d.size() - 15;
        ScsiCommand cmd;
        cmd.index = ++m_index;
        cmd.unixUs = p.unixUs;
        cmd.bus = p.bus;
        cmd.device = p.device;
        cmd.tag = r32(d, 4);
        cmd.expectedLength = r32(d, 8);
        cmd.dataIn = (quint8(d[12]) & 0x80) != 0;
        cmd.lun = quint8(d[13]) & 0x0F;
        cmd.cdb = d.mid(15, cbLen);
        cmd.info = decodeCdb(cmd.cdb);
        if (m_pending.contains(key) && m_pending.value(key).status == CswStatus::Pending)
            cmd.warning.clear();
        m_pending.insert(key, cmd);
        upd.event = MscEvent::Started;
        upd.command = cmd;
        return upd;
    }

    if (p.isIn() && d.size() == 13 && r32(d, 0) == 0x53425355 && m_pending.contains(key)) {
        ScsiCommand cur = m_pending.value(key);
        const quint32 tag = r32(d, 4);
        if (tag != cur.tag)
            cur.warning = QStringLiteral("CSW 标签不匹配 (0x") + hexN(tag, 8) + QLatin1Char(')');
        cur.residue = r32(d, 8);
        const quint8 st = quint8(d[12]);
        cur.status = st == 0 ? CswStatus::Passed : st == 1 ? CswStatus::Failed : CswStatus::PhaseError;
        cur.completedUs = p.unixUs;
        cur.response = decodeResponse(cur.info.opcode, cur.cdb, cur.dataSample);
        m_pending.remove(key);
        upd.event = MscEvent::Completed;
        upd.command = cur;
        return upd;
    }

    if (m_pending.contains(key)) {
        ScsiCommand c = m_pending.value(key);
        if (c.status == CswStatus::Pending && p.isIn() == c.dataIn) {
            if (c.dataSample.size() < 512) {
                const int take = qMin(512 - c.dataSample.size(), d.size());
                c.dataSample.append(d.left(take));
            }
            c.dataBytes += d.size();
            m_pending.insert(key, c);
        }
    }
    return upd;
}
