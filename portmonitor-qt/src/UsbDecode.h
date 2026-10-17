#pragma once

#include <QByteArray>
#include <QHash>
#include <QPair>
#include <QString>
#include <optional>

enum class UsbTransfer : quint8 { Iso = 0, Interrupt = 1, Control = 2, Bulk = 3, Unknown = 0xFF };

struct UsbPacket {
    qint64 index = 0;
    qint64 unixUs = 0;
    quint16 headerLength = 0;
    quint64 irpId = 0;
    quint32 status = 0;
    quint16 function = 0;
    quint8 info = 0;
    quint16 bus = 0;
    quint16 device = 0;
    quint8 endpoint = 0;
    UsbTransfer transfer = UsbTransfer::Unknown;
    quint32 dataLength = 0;
    std::optional<quint8> stage;
    QByteArray data;

    bool isCompletion() const { return (info & 0x01) != 0; }
    bool isIn() const { return (endpoint & 0x80) != 0; }
    int endpointNumber() const { return endpoint & 0x0F; }
    QString address() const;
    QString directionLabel() const;
    QString stageLabel() const;
    QString transferLabel() const;
    QString statusLabel() const;
    bool statusOk() const { return status == 0; }
};

struct CdbInfo {
    quint8 opcode = 0;
    QString name;
    std::optional<quint64> lba;
    std::optional<quint32> blocks;
    QString detail;
};

CdbInfo decodeCdb(const QByteArray &cdb);
QString decodeResponse(quint8 opcode, const QByteArray &cdb, const QByteArray &data);
QString formatSize(quint64 bytes);
QString urbFunctionName(quint16 function);

/// USBPcap frame (LINKTYPE 249). Returns nullopt when the frame is too short.
std::optional<UsbPacket> parseUsbPcapFrame(const QByteArray &frame, qint64 unixUs, qint64 index);

QByteArray buildUsbPcapFrame(quint64 irp, quint32 status, quint8 info, quint16 bus, quint16 device,
                             quint8 endpoint, UsbTransfer transfer, const QByteArray &data,
                             quint16 function = 0x0009);

enum class CswStatus { Pending = -1, Passed = 0, Failed = 1, PhaseError = 2 };

struct ScsiCommand {
    qint64 index = 0;
    qint64 unixUs = 0;
    quint16 bus = 0;
    quint16 device = 0;
    quint32 tag = 0;
    quint8 lun = 0;
    bool dataIn = false;
    quint32 expectedLength = 0;
    QByteArray cdb;
    CdbInfo info;
    int dataBytes = 0;
    QByteArray dataSample;
    CswStatus status = CswStatus::Pending;
    quint32 residue = 0;
    qint64 completedUs = -1;
    QString response;
    QString warning;

    QString deviceLabel() const;
    QString statusLabel() const;
    QString summary() const;
    std::optional<double> durationMs() const;
};

enum class MscEvent { None, Started, Completed };

struct MscUpdate {
    MscEvent event = MscEvent::None;
    ScsiCommand command;
};

/// USB Mass Storage Bulk-Only: CBW (USBC) → data → CSW (USBS), one command per bus.device.
/// UAS is not decoded.
class MassStorageDecoder {
public:
    void reset();
    MscUpdate push(const UsbPacket &packet);

private:
    QHash<QPair<quint16, quint16>, ScsiCommand> m_pending;
    qint64 m_index = 0;
};
