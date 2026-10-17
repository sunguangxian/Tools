#include "PcapParser.h"

#include <QtEndian>

#include <cmath>

namespace {

constexpr quint32 kMaxRecord = 64u * 1024u * 1024u;

quint32 u32at(const char *p, bool le)
{
    const auto *b = reinterpret_cast<const uchar *>(p);
    if (le)
        return quint32(b[0]) | (quint32(b[1]) << 8) | (quint32(b[2]) << 16) | (quint32(b[3]) << 24);
    return quint32(b[3]) | (quint32(b[2]) << 8) | (quint32(b[1]) << 16) | (quint32(b[0]) << 24);
}

quint16 u16at(const char *p, bool le)
{
    const auto *b = reinterpret_cast<const uchar *>(p);
    if (le)
        return quint16(b[0]) | (quint16(b[1]) << 8);
    return quint16(b[1]) | (quint16(b[0]) << 8);
}

qint64 classicUs(quint32 sec, quint32 frac, bool nano)
{
    const qint64 base = qint64(sec) * 1000000LL;
    return nano ? base + qint64(frac) / 1000 : base + qint64(frac);
}

} // namespace

void PcapParser::reset()
{
    m_buf.clear();
    m_pos = 0;
    m_phase = Phase::Magic;
    m_error.clear();
    m_nano = false;
    m_le = true;
    m_ngLe = true;
    m_linkType = 0;
    m_incl = 0;
    m_recUs = 0;
    m_need = 0;
    m_blockType = 0;
    m_magic.clear();
    m_hdr8.clear();
    m_linkTypes.clear();
    m_tsDiv.clear();
    m_ready.clear();
}

int PcapParser::available() const
{
    return m_buf.size() - m_pos;
}

bool PcapParser::take(int n, QByteArray &out)
{
    if (available() < n)
        return false;
    out = m_buf.mid(m_pos, n);
    m_pos += n;
    if (m_pos > 65536) {
        m_buf.remove(0, m_pos);
        m_pos = 0;
    }
    return true;
}

void PcapParser::setError(const QString &text)
{
    if (m_error.isEmpty())
        m_error = text;
}

bool PcapParser::parseClassicHeader(const QByteArray &hdr)
{
    const quint32 sec = u32at(hdr.constData(), m_le);
    const quint32 frac = u32at(hdr.constData() + 4, m_le);
    m_incl = u32at(hdr.constData() + 8, m_le);
    if (m_incl > kMaxRecord) {
        setError(QStringLiteral("记录长度异常，文件可能损坏"));
        return false;
    }
    m_recUs = classicUs(sec, frac, m_nano);
    return true;
}

bool PcapParser::beginNgBody(const QByteArray &hdr)
{
    m_blockType = u32at(hdr.constData(), m_ngLe);
    const quint32 total = u32at(hdr.constData() + 4, m_ngLe);
    if (total < 12 || total > kMaxRecord) {
        setError(QStringLiteral("pcapng 块长度异常"));
        return false;
    }
    m_need = total - 8;
    return true;
}

void PcapParser::parseNgBody(const QByteArray &body, QList<PcapRecord> &out)
{
    if (body.size() < 4)
        return;
    const QByteArray b = body.left(body.size() - 4);
    const bool le = m_ngLe;
    switch (m_blockType) {
    case 1: { // Interface Description Block
        if (b.size() < 2)
            break;
        m_linkTypes.append(u16at(b.constData(), le));
        double div = 1000000.0;
        int o = 8;
        while (o + 4 <= b.size()) {
            const int code = u16at(b.constData() + o, le);
            const int len = u16at(b.constData() + o + 2, le);
            if (code == 0)
                break;
            if (code == 9 && len >= 1 && o + 4 < b.size()) {
                const int r = uchar(b[o + 4]);
                const int exp = r & 0x7F;
                div = (r & 0x80) ? std::ldexp(1.0, qMin(exp, 62)) : std::pow(10.0, exp);
            }
            o += 4 + ((len + 3) & ~3);
        }
        m_tsDiv.append(div);
        break;
    }
    case 6: { // Enhanced Packet Block
        if (b.size() < 20)
            break;
        const int iface = int(u32at(b.constData(), le));
        const quint64 ts = (quint64(u32at(b.constData() + 4, le)) << 32) | u32at(b.constData() + 8, le);
        const int cap = int(u32at(b.constData() + 12, le));
        if (20 + cap > b.size())
            break;
        const double div = iface < m_tsDiv.size() ? m_tsDiv.at(iface) : 1000000.0;
        const int lt = iface < m_linkTypes.size() ? m_linkTypes.at(iface) : 0;
        qint64 us = 0;
        if (qAbs(div - 1000000.0) < 0.5)
            us = qint64(ts);
        else if (qAbs(div - 1000000000.0) < 0.5)
            us = qint64(ts / 1000);
        else {
            const double seconds = double(ts) / div;
            const qint64 whole = qint64(std::floor(seconds));
            us = whole * 1000000LL + qint64((seconds - double(whole)) * 1000000.0);
        }
        PcapRecord rec;
        rec.unixUs = us;
        rec.linkType = lt;
        rec.data = b.mid(20, cap);
        out.append(rec);
        break;
    }
    case 3: { // Simple Packet Block
        if (b.size() < 4)
            break;
        const int orig = int(u32at(b.constData(), le));
        const int cap = qMin(orig, b.size() - 4);
        PcapRecord rec;
        rec.unixUs = 0;
        rec.linkType = m_linkTypes.isEmpty() ? 0 : m_linkTypes.at(0);
        rec.data = b.mid(4, cap);
        out.append(rec);
        break;
    }
    default:
        break;
    }
}

QList<PcapRecord> PcapParser::feed(const QByteArray &chunk)
{
    QList<PcapRecord> out;
    if (hasError())
        return out;
    m_buf.append(chunk);

    while (!hasError()) {
        QByteArray got;
        switch (m_phase) {
        case Phase::Magic: {
            if (!take(4, m_magic))
                return out;
            const quint32 m = u32at(m_magic.constData(), true);
            if (m == 0x0A0D0D0A) {
                m_phase = Phase::NgFirstLen;
            } else if (m == 0xA1B2C3D4) {
                m_le = true;
                m_nano = false;
                m_phase = Phase::ClassicGlobal;
            } else if (m == 0xA1B23C4D) {
                m_le = true;
                m_nano = true;
                m_phase = Phase::ClassicGlobal;
            } else if (m == 0xD4C3B2A1) {
                m_le = false;
                m_nano = false;
                m_phase = Phase::ClassicGlobal;
            } else if (m == 0x4D3CB2A1) {
                m_le = false;
                m_nano = true;
                m_phase = Phase::ClassicGlobal;
            } else {
                setError(QStringLiteral("不是 pcap/pcapng 文件（magic=0x%1）").arg(m, 8, 16, QChar('0')));
            }
            break;
        }
        case Phase::ClassicGlobal: {
            if (!take(20, got))
                return out;
            m_linkType = int(u32at(got.constData() + 16, m_le) & 0x0FFFFFFF);
            m_phase = Phase::ClassicHdr;
            break;
        }
        case Phase::ClassicHdr: {
            if (!take(16, got))
                return out;
            if (!parseClassicHeader(got))
                return out;
            if (m_incl == 0) {
                PcapRecord rec;
                rec.unixUs = m_recUs;
                rec.linkType = m_linkType;
                out.append(rec);
                m_phase = Phase::ClassicHdr;
            } else {
                m_phase = Phase::ClassicBody;
            }
            break;
        }
        case Phase::ClassicBody: {
            if (!take(int(m_incl), got))
                return out;
            PcapRecord rec;
            rec.unixUs = m_recUs;
            rec.linkType = m_linkType;
            rec.data = got;
            out.append(rec);
            m_phase = Phase::ClassicHdr;
            break;
        }
        case Phase::NgFirstLen: {
            if (!take(4, got))
                return out;
            m_hdr8 = m_magic + got;
            const quint32 total = u32at(m_hdr8.constData() + 4, true);
            // Endianness is decided by the byte-order magic that follows.
            m_need = 4;
            m_blockType = total; // temporary; real total known after BOM
            m_phase = Phase::NgBom;
            break;
        }
        case Phase::NgBom: {
            if (!take(4, got))
                return out;
            m_ngLe = u32at(got.constData(), true) == 0x1A2B3C4D;
            const quint32 total = u32at(m_hdr8.constData() + 4, m_ngLe);
            if (total < 16 || total > kMaxRecord) {
                setError(QStringLiteral("pcapng 段头长度异常"));
                return out;
            }
            m_need = total - 12;
            m_phase = Phase::NgRest;
            break;
        }
        case Phase::NgRest: {
            if (!take(int(m_need), got))
                return out;
            m_linkTypes.clear();
            m_tsDiv.clear();
            m_phase = Phase::NgHdr;
            break;
        }
        case Phase::NgHdr: {
            if (available() < 8)
                return out;
            // Section header can appear again. Its type bytes are 0A 0D 0D 0A either way.
            if (u32at(m_buf.constData() + m_pos, true) == 0x0A0D0D0A) {
                take(4, m_magic);
                m_phase = Phase::NgFirstLen;
                break;
            }
            if (!take(8, m_hdr8))
                return out;
            if (!beginNgBody(m_hdr8))
                return out;
            m_phase = Phase::NgBody;
            break;
        }
        case Phase::NgBody: {
            if (!take(int(m_need), got))
                return out;
            parseNgBody(got, out);
            m_phase = Phase::NgHdr;
            break;
        }
        }
    }
    return out;
}

QString PcapParser::finish()
{
    if (hasError())
        return m_error;
    const bool idle = m_phase == Phase::Magic || m_phase == Phase::ClassicHdr || m_phase == Phase::NgHdr;
    if (!idle && available() > 0)
        return QStringLiteral("pcap 数据被截断");
    return {};
}

void writeClassicPcap(QIODevice *out, int linkType, const QList<PcapRecord> &records)
{
    QByteArray gh(24, 0);
    qToLittleEndian<quint32>(0xA1B2C3D4, gh.data());
    qToLittleEndian<quint16>(2, gh.data() + 4);
    qToLittleEndian<quint16>(4, gh.data() + 6);
    qToLittleEndian<quint32>(65535, gh.data() + 16);
    qToLittleEndian<quint32>(quint32(linkType), gh.data() + 20);
    out->write(gh);
    for (const PcapRecord &rec : records) {
        const qint64 us = rec.unixUs;
        QByteArray rh(16, 0);
        qToLittleEndian<quint32>(quint32(us / 1000000LL), rh.data());
        qToLittleEndian<quint32>(quint32(us % 1000000LL), rh.data() + 4);
        qToLittleEndian<quint32>(quint32(rec.data.size()), rh.data() + 8);
        qToLittleEndian<quint32>(quint32(rec.data.size()), rh.data() + 12);
        out->write(rh);
        out->write(rec.data);
    }
}
