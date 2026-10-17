#include "PcapParser.h"
#include "UsbDecode.h"

#include <QBuffer>
#include <QDebug>
#include <QFile>
#include <QtEndian>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>

namespace {

int g_failures = 0;

void check(bool ok, const char *what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok)
        ++g_failures;
}

QByteArray cdbOf(std::initializer_list<int> bytes)
{
    QByteArray d;
    d.resize(int(bytes.size()));
    int i = 0;
    for (int b : bytes)
        d[i++] = char(b);
    return d;
}

void addFrame(QList<PcapRecord> &recs, qint64 us, quint8 info, quint8 ep, const QByteArray &data, quint64 &irp)
{
    PcapRecord rec;
    rec.unixUs = us;
    rec.linkType = 249;
    rec.data = buildUsbPcapFrame(irp++, 0, info, 1, 5, ep, UsbTransfer::Bulk, data);
    recs.append(rec);
}

QList<PcapRecord> buildSample()
{
    QList<PcapRecord> recs;
    quint64 irp = 0xFFFFA00000001000ULL;
    quint32 tag = 0x10;
    qint64 now = 1760600000LL * 1000000LL;
    const quint8 epOut = 0x02;
    const quint8 epIn = 0x81;

    auto command = [&](const QByteArray &cdb, bool dataIn, quint32 expected, const QByteArray &data, bool hasData,
                       quint8 status, quint32 residue, qint64 durUs) {
        QByteArray cbw(31, 0);
        qToLittleEndian<quint32>(0x43425355u, cbw.data());
        qToLittleEndian<quint32>(tag, cbw.data() + 4);
        qToLittleEndian<quint32>(expected, cbw.data() + 8);
        cbw[12] = char(dataIn ? 0x80 : 0x00);
        cbw[14] = char(cdb.size());
        memcpy(cbw.data() + 15, cdb.constData(), size_t(cdb.size()));
        addFrame(recs, now, 0, epOut, cbw, irp);
        addFrame(recs, now + 50, 1, epOut, {}, irp);
        if (hasData) {
            if (dataIn) {
                addFrame(recs, now + 100, 0, epIn, {}, irp);
                addFrame(recs, now + durUs * 8 / 10, 1, epIn, data, irp);
            } else {
                addFrame(recs, now + 100, 0, epOut, data, irp);
                addFrame(recs, now + durUs * 8 / 10, 1, epOut, {}, irp);
            }
        }
        QByteArray csw(13, 0);
        qToLittleEndian<quint32>(0x53425355u, csw.data());
        qToLittleEndian<quint32>(tag, csw.data() + 4);
        qToLittleEndian<quint32>(residue, csw.data() + 8);
        csw[12] = char(status);
        addFrame(recs, now + durUs * 85 / 100, 0, epIn, {}, irp);
        addFrame(recs, now + durUs, 1, epIn, csw, irp);
        ++tag;
        now += durUs + 2000;
    };

    QByteArray inq(36, 0);
    inq[0] = 0x00;
    inq[1] = char(0x80);
    inq[2] = 0x06;
    inq[3] = 0x02;
    inq[4] = 31;
    memcpy(inq.data() + 8, "SanDisk ", 8);
    memcpy(inq.data() + 16, "Ultra USB 3.0   ", 16);
    memcpy(inq.data() + 32, "1.00", 4);
    command(cdbOf({0x12, 0, 0, 0, 36, 0}), true, 36, inq, true, 0, 0, 1200);

    QByteArray cap(8, 0);
    qToBigEndian<quint32>(0x01DCDFFFu, cap.data());
    qToBigEndian<quint32>(512, cap.data() + 4);
    command(cdbOf({0x25, 0, 0, 0, 0, 0, 0, 0, 0, 0}), true, 8, cap, true, 0, 0, 900);

    command(cdbOf({0x00, 0, 0, 0, 0, 0}), false, 0, {}, false, 1, 0, 600);

    QByteArray sense(18, 0);
    sense[0] = 0x70;
    sense[2] = 0x06;
    sense[7] = 10;
    sense[12] = 0x28;
    sense[13] = 0x00;
    command(cdbOf({0x03, 0, 0, 0, 18, 0}), true, 18, sense, true, 0, 0, 700);

    QByteArray rd(4096, 0);
    for (int i = 0; i < rd.size(); ++i)
        rd[i] = char(i);
    command(cdbOf({0x28, 0, 0, 0, 0x08, 0x00, 0, 0, 8, 0}), true, 4096, rd, true, 0, 0, 3400);

    QByteArray wr(512, 0);
    memcpy(wr.data(), "PortMonitor sample write", 24);
    command(cdbOf({0x2A, 0, 0, 0, 0, 100, 0, 0, 1, 0}), false, 512, wr, true, 0, 0, 2100);

    command(cdbOf({0x35, 0, 0, 0, 0, 0, 0, 0, 0, 0}), false, 0, {}, false, 0, 0, 5000);
    return recs;
}

QList<ScsiCommand> decodeRecords(const QList<PcapRecord> &recs, int &packets)
{
    MassStorageDecoder dec;
    QList<ScsiCommand> cmds;
    packets = 0;
    qint64 index = 0;
    for (const PcapRecord &rec : recs) {
        if (rec.linkType != 249)
            continue;
        const auto pkt = parseUsbPcapFrame(rec.data, rec.unixUs, ++index);
        if (!pkt)
            continue;
        ++packets;
        const MscUpdate upd = dec.push(*pkt);
        if (upd.event == MscEvent::Completed)
            cmds.append(upd.command);
    }
    return cmds;
}

QByteArray ngBlock(quint32 type, const QByteArray &body)
{
    const int pad = (4 - body.size() % 4) % 4;
    const quint32 total = quint32(12 + body.size() + pad);
    QByteArray out(8, 0);
    qToLittleEndian<quint32>(type, out.data());
    qToLittleEndian<quint32>(total, out.data() + 4);
    out.append(body);
    out.append(QByteArray(pad, 0));
    QByteArray tail(4, 0);
    qToLittleEndian<quint32>(total, tail.data());
    out.append(tail);
    return out;
}

void checkCommands(const QList<ScsiCommand> &cmds, int packets, const char *prefix)
{
    check(packets == 38, prefix);
    char countMsg[128];
    std::snprintf(countMsg, sizeof(countMsg), "%s commands", prefix);
    check(cmds.size() == 7, countMsg);
    if (cmds.size() != 7) {
        for (const ScsiCommand &c : cmds)
            std::printf("  #%lld %s\n", static_cast<long long>(c.index), qPrintable(c.info.name));
        return;
    }
    check(cmds[0].info.name == QLatin1String("INQUIRY") && cmds[0].response.contains(QLatin1String("SanDisk"))
              && cmds[0].response.contains(QLatin1String("Ultra USB 3.0")) && cmds[0].response.contains(QLatin1String("1.00")),
          "INQUIRY vendor/product/rev");
    check(cmds[1].info.name == QLatin1String("READ CAPACITY(10)") && cmds[1].response.contains(QStringLiteral("块大小=512"))
              && cmds[1].response.contains(QStringLiteral("14.9")) && cmds[1].dataBytes == 8,
          "READ CAPACITY size");
    check(cmds[2].info.name == QLatin1String("TEST UNIT READY") && cmds[2].status == CswStatus::Failed, "TUR failed");
    check(cmds[3].info.name == QLatin1String("REQUEST SENSE") && cmds[3].response.contains(QLatin1String("UNIT ATTENTION"))
              && cmds[3].response.contains(QLatin1String("28/00")),
          "REQUEST SENSE");
    check(cmds[4].info.name == QLatin1String("READ(10)") && cmds[4].info.lba == 2048 && cmds[4].info.blocks == 8
              && cmds[4].dataBytes == 4096 && cmds[4].status == CswStatus::Passed,
          "READ(10)");
    const double ms = cmds[4].durationMs().value_or(0);
    check(std::abs(ms - 3.4) < 0.01, "READ(10) latency");
    check(cmds[5].info.name == QLatin1String("WRITE(10)") && cmds[5].info.lba == 100 && cmds[5].info.blocks == 1
              && cmds[5].dataBytes == 512 && !cmds[5].dataIn,
          "WRITE(10)");
    check(cmds[6].info.name == QLatin1String("SYNCHRONIZE CACHE(10)"), "SYNC CACHE");
    bool tags = true;
    for (int i = 0; i < 7; ++i)
        tags = tags && cmds[i].tag == quint32(0x10 + i) && cmds[i].bus == 1 && cmds[i].device == 5;
    check(tags, "tags and device 1.5");
}

} // namespace

int main(int argc, char **argv)
{
    const CdbInfo read16 = decodeCdb(cdbOf({0x88, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0x20, 0, 0}));
    check(read16.name == QLatin1String("READ(16)") && read16.lba == 0x100000000ULL && read16.blocks == 0x20, "READ(16)");
    const CdbInfo write12 = decodeCdb(cdbOf({0xAA, 0, 0, 0, 1, 0, 0, 0, 0, 4, 0, 0}));
    check(write12.name == QLatin1String("WRITE(12)") && write12.lba == 256 && write12.blocks == 4, "WRITE(12)");
    check(decodeCdb(cdbOf({0x9E, 0x10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 32, 0, 0})).name == QLatin1String("READ CAPACITY(16)"),
          "READ CAPACITY(16)");
    check(decodeCdb(cdbOf({0xC7, 1, 2})).name == QLatin1String("OP 0xC7"), "unknown opcode");

    const QList<PcapRecord> sample = buildSample();
    int packets = 0;
    const QList<ScsiCommand> memoryCmds = decodeRecords(sample, packets);
    checkCommands(memoryCmds, packets, "memory packet count = 38");

    QBuffer classic;
    classic.open(QIODevice::ReadWrite);
    writeClassicPcap(&classic, 249, sample);
    classic.seek(0);
    PcapParser parser;
    const QList<PcapRecord> round = parser.feed(classic.readAll());
    check(parser.finish().isEmpty() && !parser.hasError(), "classic pcap parse");
    int roundPk = 0;
    const QList<ScsiCommand> roundCmds = decodeRecords(round, roundPk);
    check(roundPk == 38 && roundCmds.size() == 7 && roundCmds[4].info.lba == 2048, "classic pcap round-trip");

    QByteArray ng;
    QByteArray shb(16, 0);
    qToLittleEndian<quint32>(0x1A2B3C4D, shb.data());
    qToLittleEndian<quint16>(1, shb.data() + 4);
    qToLittleEndian<qint64>(-1, shb.data() + 8);
    ng.append(ngBlock(0x0A0D0D0A, shb));
    QByteArray idb(8, 0);
    qToLittleEndian<quint16>(249, idb.data());
    ng.append(ngBlock(1, idb));
    for (const PcapRecord &rec : sample) {
        QByteArray epb(20 + rec.data.size(), 0);
        const quint64 us = quint64(rec.unixUs);
        qToLittleEndian<quint32>(quint32(us >> 32), epb.data() + 4);
        qToLittleEndian<quint32>(quint32(us), epb.data() + 8);
        qToLittleEndian<quint32>(quint32(rec.data.size()), epb.data() + 12);
        qToLittleEndian<quint32>(quint32(rec.data.size()), epb.data() + 16);
        memcpy(epb.data() + 20, rec.data.constData(), size_t(rec.data.size()));
        ng.append(ngBlock(6, epb));
    }
    PcapParser ngParser;
    const QList<PcapRecord> ngRecs = ngParser.feed(ng);
    check(ngParser.finish().isEmpty() && !ngParser.hasError(), "pcapng parse");
    int ngPk = 0;
    const QList<ScsiCommand> ngCmds = decodeRecords(ngRecs, ngPk);
    const double ngMs = ngCmds.size() > 4 ? ngCmds[4].durationMs().value_or(0) : 0;
    check(ngPk == 38 && ngCmds.size() == 7 && ngCmds.value(4).info.lba == 2048 && std::abs(ngMs - 3.4) < 0.01, "pcapng round-trip");

    if (argc > 1) {
        QFile f(QString::fromLocal8Bit(argv[1]));
        if (!f.open(QIODevice::ReadOnly)) {
            check(false, "open sample file");
        } else {
            PcapParser fileParser;
            const QList<PcapRecord> fileRecs = fileParser.feed(f.readAll());
            const QString fin = fileParser.finish();
            check(fin.isEmpty() && !fileParser.hasError(), "sample file parse");
            int filePk = 0;
            const QList<ScsiCommand> fileCmds = decodeRecords(fileRecs, filePk);
            checkCommands(fileCmds, filePk, "sample file packet count = 38");
        }
    }

    std::printf(g_failures == 0 ? "ALL TESTS PASSED\n" : "%d FAILURE(S)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
