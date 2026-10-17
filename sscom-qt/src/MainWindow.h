#pragma once

#include <QByteArray>
#include <QColor>
#include <QFile>
#include <QMainWindow>
#include <QSerialPort>
#include <QTimer>
#include <QVector>

class QAction;
class QCheckBox;
class QComboBox;
class QDialog;
class QLabel;
class QLineEdit;
class QMenu;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QWidget;

struct PresetRow {
    QSpinBox *order = nullptr;
    QPushButton *button = nullptr;
    QLineEdit *text = nullptr;
    QCheckBox *hex = nullptr;
    QSpinBox *delayMs = nullptr;
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    static constexpr int kPresetCount = 99;

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void refreshPorts();
    void toggleOpen();
    void onReadyRead();
    void onSerialError(QSerialPort::SerialPortError error);
    void sendMain();
    void clearReceive();
    void stopSending();

private:
    struct LoopItem {
        int row = 0;
        int delayMs = 0;
    };

    void buildUi();
    void buildMenus();
    void buildExtension();
    void buildMoreDialog();
    void applyLanguage();
    void loadSettings();
    void saveSettings();
    void importSscomIni();
    void showCalculator();
    void showAbout();
    void chooseFont();
    void chooseBackground();
    void applyRecvStyle();
    void setBufferLimit(int chars);
    void setFileDelay(int ms);
    void setExtensionVisible(bool visible);
    void applyStayOnTop(bool on);
    void setLed(bool open);
    void updateStatus();
    void rebuildPortMenu();
    void selectPort(const QString &name);

    QString L(const QString &zh, const QString &en) const;
    QString currentPortName() const;
    int currentBaud() const;
    QString parityToken() const;
    QString flowToken() const;
    QString stopToken() const;

    bool openCurrentPort();
    void closePort();
    bool transmit(const QByteArray &data);
    QByteArray composeMain(QString *error) const;
    QByteArray composePreset(int row, QString *error) const;
    void sendPreset(int row);
    void startLoop();
    void loopTick();
    void startFileSend();
    void fileTick();
    void flushPacket();
    void appendRx(const QByteArray &data, bool packetLine);
    void insertReceive(const QString &text);
    void trimReceive();
    void setRecvToFile(bool on);
    void saveReceiveText();
    void openFileToSend();
    bool handleTerminalKey(QKeyEvent *event);
    void eraseLastReceiveChar();

    bool m_english = false;
    bool m_loading = false;
    bool m_ignoreSerialError = false;
    qint64 m_rx = 0;
    qint64 m_tx = 0;
    int m_bufferChars = 1000000;
    int m_fileDelayMs = 0;
    int m_baseWidth = 812;

    QSerialPort *m_serial = nullptr;
    QTimer m_autoTimer;
    QTimer m_packetTimer;
    QTimer m_fileTimer;
    QTimer m_loopTimer;
    QByteArray m_packet;
    QByteArray m_textPending;
    QFile m_recvFile;
    QFile m_sendFile;
    bool m_fileSending = false;
    bool m_looping = false;
    QVector<LoopItem> m_loopItems;
    int m_loopPos = 0;
    QString m_lineBuf;
    int m_lineEchoChars = 0;

    QPlainTextEdit *m_receive = nullptr;
    QPlainTextEdit *m_send = nullptr;
    QLineEdit *m_filePath = nullptr;
    QComboBox *m_portCombo = nullptr;
    QComboBox *m_baudCombo = nullptr;
    QComboBox *m_dataBits = nullptr;
    QComboBox *m_parity = nullptr;
    QComboBox *m_stopBits = nullptr;
    QComboBox *m_flow = nullptr;
    QComboBox *m_checksum = nullptr;
    QDialog *m_moreDlg = nullptr;

    QPushButton *m_btnOpen = nullptr;
    QPushButton *m_btnMore = nullptr;
    QPushButton *m_btnSend = nullptr;
    QPushButton *m_btnClearRecv = nullptr;
    QPushButton *m_btnOpenFile = nullptr;
    QPushButton *m_btnSendFile = nullptr;
    QPushButton *m_btnStop = nullptr;
    QPushButton *m_btnClearSend = nullptr;
    QPushButton *m_btnSaveParams = nullptr;
    QPushButton *m_btnSaveData = nullptr;
    QPushButton *m_btnExt = nullptr;
    QPushButton *m_btnLoop = nullptr;
    QLabel *m_led = nullptr;
    QLabel *m_lbPort = nullptr;
    QLabel *m_lbBaud = nullptr;
    QLabel *m_lbMs = nullptr;
    QLabel *m_lbTimeout = nullptr;
    QLabel *m_lbMs2 = nullptr;
    QLabel *m_lbFrom = nullptr;
    QLabel *m_lbToEnd = nullptr;
    QLabel *m_lbChecksum = nullptr;
    QLabel *m_countLabel = nullptr;
    QLabel *m_stateLabel = nullptr;
    QLabel *m_extTitle = nullptr;
    QLabel *m_moreDataLb = nullptr;
    QLabel *m_moreParityLb = nullptr;
    QLabel *m_moreStopLb = nullptr;
    QLabel *m_moreFlowLb = nullptr;
    QColor m_recvBg {Qt::white};
    QColor m_recvFg {Qt::black};

    QCheckBox *m_dtr = nullptr;
    QCheckBox *m_rts = nullptr;
    QCheckBox *m_hexDisp = nullptr;
    QCheckBox *m_recvToFile = nullptr;
    QCheckBox *m_hexSend = nullptr;
    QCheckBox *m_autoSend = nullptr;
    QCheckBox *m_newLine = nullptr;
    QCheckBox *m_packetMode = nullptr;
    QCheckBox *m_stayOnTop = nullptr;
    QCheckBox *m_englishBox = nullptr;
    QSpinBox *m_interval = nullptr;
    QSpinBox *m_timeout = nullptr;
    QSpinBox *m_csFrom = nullptr;
    QSpinBox *m_csTail = nullptr;

    QWidget *m_extPanel = nullptr;
    QVector<PresetRow> m_rows;

    QMenu *m_menuPort = nullptr;
    QMenu *m_menuSerial = nullptr;
    QMenu *m_menuDisplay = nullptr;
    QMenu *m_menuSend = nullptr;
    QMenu *m_menuTerm = nullptr;
    QMenu *m_menuFileDelay = nullptr;
    QMenu *m_menuBuffer = nullptr;
    QMenu *m_menuTools = nullptr;
    QMenu *m_menuHelp = nullptr;
    QAction *m_actRefresh = nullptr;
    QAction *m_actMore = nullptr;
    QAction *m_actAutoOpen = nullptr;
    QAction *m_actImport = nullptr;
    QAction *m_actFont = nullptr;
    QAction *m_actBg = nullptr;
    QAction *m_actLineMode = nullptr;
    QAction *m_actKeySend = nullptr;
    QAction *m_actLocalEcho = nullptr;
    QAction *m_actCrLf = nullptr;
    QAction *m_actMulti = nullptr;
    QAction *m_actCalc = nullptr;
    QAction *m_actAbout = nullptr;
    QAction *m_actHelp = nullptr;
    QVector<QAction *> m_bufferActs;
    QVector<QAction *> m_delayActs;
};
