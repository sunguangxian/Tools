#pragma once

#include "CaptureModel.h"
#include "UsbDecode.h"

#include <QElapsedTimer>
#include <QFont>
#include <QHash>
#include <QMainWindow>
#include <QSet>

class QTabWidget;

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QProcess;
class QPushButton;
class QSerialPort;
class QSplitter;
class QTableView;
class QTextEdit;
class QTimer;
class QTreeWidget;
class QTreeWidgetItem;
class PcapParser;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
    void openPcap(const QString &path);

private:
    void buildUi();
    void refreshSerialPorts();
    void refreshUsbInterfaces();
    void startMonitor();
    void stopMonitor();
    void clearCapture();
    void togglePause();
    void exportCaptureCsv();
    void exportScsiCsv();
    void sendToPort();
    void showAbout();
    void showError(const QString &text);
    void setRunning(bool running);
    void ingestRows(const QVector<CaptureRow> &rows);
    void addSerial(const QString &port, bool isWrite, const QByteArray &data);
    void handleUsbPacket(const UsbPacket &packet);
    void upsertScsi(const ScsiCommand &command);
    void appendTextRow(const CaptureRow &row);
    void rebuildTextViews();
    void showCaptureDetail(const CaptureRow *row);
    void showScsiDetail(const ScsiRow *row);
    void refreshStats();
    void notePort(const QString &port);
    void rememberDevice(quint16 bus, quint16 device);
    void maybeScroll() const;
    QStringList checkedNames(const QString &kind) const;
    QString selectedCom() const;

    CaptureModel *m_model = nullptr;
    ScsiModel *m_scsi = nullptr;
    CaptureFilter *m_proxy = nullptr;
    ScsiFilter *m_scsiProxy = nullptr;
    ViewFilter m_filter;
    MassStorageDecoder m_scsiDecoder;

    QTreeWidget *m_tree = nullptr;
    QTreeWidgetItem *m_serialRoot = nullptr;
    QTreeWidgetItem *m_usbRoot = nullptr;
    QTreeWidgetItem *m_devRoot = nullptr;
    QComboBox *m_baud = nullptr;
    QComboBox *m_dataBits = nullptr;
    QComboBox *m_parity = nullptr;
    QComboBox *m_stopBits = nullptr;
    QLineEdit *m_sendEdit = nullptr;
    QCheckBox *m_sendHex = nullptr;
    QLineEdit *m_usbCmd = nullptr;
    QLineEdit *m_usbAddr = nullptr;
    QComboBox *m_sourceFilter = nullptr;
    QComboBox *m_portFilter = nullptr;
    QLineEdit *m_search = nullptr;
    QCheckBox *m_autoScroll = nullptr;
    QPushButton *m_btnStart = nullptr;
    QPushButton *m_btnStop = nullptr;
    QLabel *m_error = nullptr;
    QTabWidget *m_tabs = nullptr;
    QTableView *m_table = nullptr;
    QTableView *m_scsiTable = nullptr;
    QPlainTextEdit *m_line = nullptr;
    QTextEdit *m_terminal = nullptr;
    QPlainTextEdit *m_dump = nullptr;
    QLabel *m_meta = nullptr;
    QPlainTextEdit *m_hex = nullptr;
    QPlainTextEdit *m_ascii = nullptr;
    QLabel *m_sbState = nullptr;
    QLabel *m_sbPkts = nullptr;
    QLabel *m_sbBytes = nullptr;
    QLabel *m_sbRead = nullptr;
    QLabel *m_sbWrite = nullptr;
    QLabel *m_sbRate = nullptr;
    QTimer *m_textTimer = nullptr;
    QTimer *m_rateTimer = nullptr;
    QFont m_mono;

    QHash<QSerialPort *, QString> m_portNames;
    QHash<QProcess *, PcapParser *> m_usbParsers;
    QSet<QString> m_seenPorts;
    QSet<QString> m_seenDevices;
    QElapsedTimer m_rateClock;
    qint64 m_seq = 0;
    qint64 m_packets = 0;
    qint64 m_bytes = 0;
    qint64 m_reads = 0;
    qint64 m_writes = 0;
    qint64 m_pktsMark = 0;
    bool m_running = false;
    bool m_paused = false;
    bool m_stopping = false;
};
