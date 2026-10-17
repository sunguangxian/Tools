#pragma once

#include <QByteArray>
#include <QIODevice>
#include <QList>
#include <QString>

struct PcapRecord {
    qint64 unixUs = 0;
    int linkType = 0;
    QByteArray data;
};

/// Incremental pcap / pcapng reader. feed() keeps a partial tail until the next chunk.
class PcapParser {
public:
    void reset();
    QList<PcapRecord> feed(const QByteArray &chunk);
    /// After a file is fully fed: non-empty if a record was cut off.
    QString finish();
    QString error() const { return m_error; }
    bool hasError() const { return !m_error.isEmpty(); }

private:
    enum class Phase { Magic, ClassicGlobal, ClassicHdr, ClassicBody, NgFirstLen, NgBom, NgRest, NgHdr, NgBody };

    int available() const;
    bool take(int n, QByteArray &out);
    void setError(const QString &text);
    bool parseClassicHeader(const QByteArray &hdr);
    bool beginNgBody(const QByteArray &hdr);
    void parseNgBody(const QByteArray &body, QList<PcapRecord> &out);

    QByteArray m_buf;
    int m_pos = 0;
    Phase m_phase = Phase::Magic;
    QString m_error;
    bool m_nano = false;
    bool m_le = true;
    bool m_ngLe = true;
    int m_linkType = 0;
    quint32 m_incl = 0;
    qint64 m_recUs = 0;
    quint32 m_need = 0;
    quint32 m_blockType = 0;
    QByteArray m_magic;
    QByteArray m_hdr8;
    QList<int> m_linkTypes;
    QList<double> m_tsDiv;
    QList<PcapRecord> m_ready;
};

void writeClassicPcap(QIODevice *out, int linkType, const QList<PcapRecord> &records);
