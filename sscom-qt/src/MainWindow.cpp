#include "MainWindow.h"

#include "codec.h"

#include <QAbstractSpinBox>
#include <QAction>
#include <QActionGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QCursor>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFont>
#include <QFontDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSerialPortInfo>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTextCursor>
#include <QVBoxLayout>

#include <algorithm>

namespace {

QSerialPort::DataBits dataBitsFrom(int bits)
{
    switch (bits) {
    case 5: return QSerialPort::Data5;
    case 6: return QSerialPort::Data6;
    case 7: return QSerialPort::Data7;
    default: return QSerialPort::Data8;
    }
}

QSerialPort::Parity parityFrom(int index)
{
    switch (index) {
    case 1: return QSerialPort::OddParity;
    case 2: return QSerialPort::EvenParity;
    case 3: return QSerialPort::MarkParity;
    case 4: return QSerialPort::SpaceParity;
    default: return QSerialPort::NoParity;
    }
}

QSerialPort::StopBits stopFrom(const QString &text)
{
    if (text == QLatin1String("1.5"))
        return QSerialPort::OneAndHalfStop;
    if (text == QLatin1String("2"))
        return QSerialPort::TwoStop;
    return QSerialPort::OneStop;
}

QSerialPort::FlowControl flowFrom(int index)
{
    switch (index) {
    case 1: return QSerialPort::HardwareControl;
    case 2: return QSerialPort::SoftwareControl;
    default: return QSerialPort::NoFlowControl;
    }
}

bool yn(const QString &text)
{
    const QString t = text.trimmed();
    return t.compare(QLatin1String("Y"), Qt::CaseInsensitive) == 0 || t == QLatin1String("1");
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_serial(new QSerialPort(this))
{
    setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 9));
    resize(812, 567);
    setMinimumSize(720, 480);

    buildUi();
    buildMenus();
    applyLanguage();

    connect(m_serial, &QSerialPort::readyRead, this, &MainWindow::onReadyRead);
    connect(m_serial, &QSerialPort::errorOccurred, this, &MainWindow::onSerialError);
    connect(&m_autoTimer, &QTimer::timeout, this, &MainWindow::sendMain);
    connect(&m_packetTimer, &QTimer::timeout, this, &MainWindow::flushPacket);
    connect(&m_fileTimer, &QTimer::timeout, this, &MainWindow::fileTick);
    connect(&m_loopTimer, &QTimer::timeout, this, &MainWindow::loopTick);
    m_packetTimer.setSingleShot(true);
    m_loopTimer.setSingleShot(true);

    connect(m_btnOpen, &QPushButton::clicked, this, &MainWindow::toggleOpen);
    connect(m_btnSend, &QPushButton::clicked, this, &MainWindow::sendMain);
    connect(m_btnClearRecv, &QPushButton::clicked, this, &MainWindow::clearReceive);
    connect(m_btnClearSend, &QPushButton::clicked, this, [this] { m_send->clear(); });
    connect(m_btnStop, &QPushButton::clicked, this, &MainWindow::stopSending);
    connect(m_btnOpenFile, &QPushButton::clicked, this, &MainWindow::openFileToSend);
    connect(m_btnSendFile, &QPushButton::clicked, this, &MainWindow::startFileSend);
    connect(m_btnSaveData, &QPushButton::clicked, this, &MainWindow::saveReceiveText);
    connect(m_btnSaveParams, &QPushButton::clicked, this, [this] {
        saveSettings();
        statusBar()->showMessage(L(QStringLiteral("参数已保存"), QStringLiteral("Settings saved")));
    });
    connect(m_btnExt, &QPushButton::clicked, this, [this] {
        setExtensionVisible(!m_extPanel->isVisible());
    });
    connect(m_btnMore, &QPushButton::clicked, this, [this] {
        m_moreDlg->show();
        m_moreDlg->raise();
    });
    connect(m_btnLoop, &QPushButton::toggled, this, [this](bool on) {
        if (m_loading)
            return;
        if (on)
            startLoop();
        else {
            m_looping = false;
            m_loopTimer.stop();
        }
    });

    connect(m_autoSend, &QCheckBox::toggled, this, [this](bool on) {
        if (m_loading)
            return;
        if (!on) {
            m_autoTimer.stop();
            return;
        }
        if (!m_serial->isOpen()) {
            QSignalBlocker blocker(m_autoSend);
            m_autoSend->setChecked(false);
            statusBar()->showMessage(L(QStringLiteral("请先打开串口"), QStringLiteral("Open the port first")));
            return;
        }
        m_autoTimer.setInterval(qMax(1, m_interval->value()));
        m_autoTimer.start();
    });
    connect(m_interval, &QSpinBox::valueChanged, this, [this](int value) {
        if (m_autoTimer.isActive())
            m_autoTimer.setInterval(qMax(1, value));
    });
    connect(m_recvToFile, &QCheckBox::toggled, this, [this](bool on) {
        if (!m_loading)
            setRecvToFile(on);
    });
    connect(m_packetMode, &QCheckBox::toggled, this, [this](bool on) {
        if (!on)
            flushPacket();
    });
    connect(m_dtr, &QCheckBox::toggled, this, [this](bool on) {
        if (m_serial->isOpen())
            m_serial->setDataTerminalReady(on);
    });
    connect(m_rts, &QCheckBox::toggled, this, [this](bool on) {
        if (m_serial->isOpen())
            m_serial->setRequestToSend(on);
    });
    connect(m_stayOnTop, &QCheckBox::toggled, this, &MainWindow::applyStayOnTop);
    connect(m_englishBox, &QCheckBox::toggled, this, [this](bool on) {
        m_english = on;
        applyLanguage();
    });
    connect(m_portCombo, &QComboBox::currentIndexChanged, this, [this] {
        if (m_loading)
            return;
        if (m_serial->isOpen())
            openCurrentPort();
        updateStatus();
    });
    connect(m_baudCombo, &QComboBox::activated, this, [this](int) {
        if (m_loading)
            return;
        if (m_serial->isOpen())
            openCurrentPort();
        updateStatus();
    });
    if (QLineEdit *baudEdit = m_baudCombo->lineEdit()) {
        connect(baudEdit, &QLineEdit::editingFinished, this, [this] {
            if (m_loading)
                return;
            if (m_serial->isOpen())
                openCurrentPort();
            updateStatus();
        });
    }

    m_receive->installEventFilter(this);
    loadSettings();
    refreshPorts();
    if (m_receive->toPlainText().isEmpty()) {
        m_receive->setPlainText(
            L(QStringLiteral(
                  "SSCOM 串口调试助手（Qt，仅串口）\n"
                  "1. 选择端口号和波特率，点「打开串口」。数据位 / 校验 / 停止位 / 流控在「更多串口设置」。\n"
                  "2. HEX显示、保存数据、接收数据到文件、HEX发送、定时发送、加回车换行。\n"
                  "3. 「加时间戳和分包显示」按右侧超时把空闲间隔开的数据分成一行。\n"
                  "4. 「扩展」打开多字符串。序号大于 0 的项按序号循环发送，右键按钮可改名称。\n"
                  "5. 发送菜单里可设终端按键发送，以及发送文件时每 256 字节的延时。\n"),
              QStringLiteral(
                  "SSCOM serial debugger (Qt, serial only)\n"
                  "1. Pick a port and baud, then Open. Data bits / parity / stop / flow are under More.\n"
                  "2. HEX display, save text, log raw bytes, HEX send, timed send, append CRLF.\n"
                  "3. Timestamp + packet mode splits RX on the idle timeout.\n"
                  "4. Ext shows multi-strings. Order > 0 is sent in a loop. Right-click a button to rename it.\n"
                  "5. The Send menu has terminal key echo and the per-256-byte file delay.\n")));
    }
    updateStatus();
    if (m_actAutoOpen->isChecked())
        toggleOpen();
}

MainWindow::~MainWindow()
{
    m_ignoreSerialError = true;
    if (m_serial->isOpen())
        m_serial->close();
    m_recvFile.close();
    m_sendFile.close();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSettings();
    stopSending();
    m_ignoreSerialError = true;
    if (m_serial->isOpen())
        m_serial->close();
    m_recvFile.close();
    QMainWindow::closeEvent(event);
}

QString MainWindow::L(const QString &zh, const QString &en) const
{
    return m_english ? en : zh;
}

void MainWindow::buildUi()
{
    auto *central = new QWidget(this);
    auto *root = new QHBoxLayout(central);
    root->setContentsMargins(6, 4, 6, 4);
    root->setSpacing(6);

    auto *column = new QWidget(central);
    auto *col = new QVBoxLayout(column);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(4);

    m_receive = new QPlainTextEdit(column);
    m_receive->setReadOnly(true);
    m_receive->setFont(QFont(QStringLiteral("SimSun"), 9));
    m_receive->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    applyRecvStyle();
    col->addWidget(m_receive, 1);

    auto *bar = new QHBoxLayout;
    bar->setSpacing(4);
    m_btnClearRecv = new QPushButton(column);
    m_btnOpenFile = new QPushButton(column);
    m_filePath = new QLineEdit(column);
    m_btnSendFile = new QPushButton(column);
    m_btnStop = new QPushButton(column);
    m_btnClearSend = new QPushButton(column);
    m_stayOnTop = new QCheckBox(column);
    m_englishBox = new QCheckBox(QStringLiteral("English"), column);
    m_btnSaveParams = new QPushButton(column);
    m_btnExt = new QPushButton(column);
    bar->addWidget(m_btnClearRecv);
    bar->addWidget(m_btnOpenFile);
    bar->addWidget(m_filePath, 1);
    bar->addWidget(m_btnSendFile);
    bar->addWidget(m_btnStop);
    bar->addWidget(m_btnClearSend);
    bar->addWidget(m_stayOnTop);
    bar->addWidget(m_englishBox);
    bar->addWidget(m_btnSaveParams);
    bar->addWidget(m_btnExt);
    col->addLayout(bar);

    auto *bottom = new QHBoxLayout;
    bottom->setSpacing(8);

    auto *left = new QWidget(column);
    left->setFixedWidth(236);
    auto *leftLay = new QVBoxLayout(left);
    leftLay->setContentsMargins(0, 0, 0, 0);
    leftLay->setSpacing(4);

    auto *portRow = new QHBoxLayout;
    m_lbPort = new QLabel(left);
    m_portCombo = new QComboBox(left);
    m_portCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_portCombo->setMinimumContentsLength(8);
    portRow->addWidget(m_lbPort);
    portRow->addWidget(m_portCombo, 1);
    leftLay->addLayout(portRow);

    auto *openRow = new QHBoxLayout;
    m_led = new QLabel(left);
    m_led->setFixedSize(12, 12);
    m_btnOpen = new QPushButton(left);
    m_btnMore = new QPushButton(left);
    openRow->addWidget(m_led);
    openRow->addWidget(m_btnOpen, 1);
    openRow->addWidget(m_btnMore);
    leftLay->addLayout(openRow);

    auto *lineRow = new QHBoxLayout;
    m_rts = new QCheckBox(QStringLiteral("RTS"), left);
    m_dtr = new QCheckBox(QStringLiteral("DTR"), left);
    m_dtr->setChecked(true);
    m_lbBaud = new QLabel(left);
    m_baudCombo = new QComboBox(left);
    m_baudCombo->setEditable(true);
    const int bauds[] = {1200, 2400, 4800, 9600, 14400, 19200, 38400, 56000,
                         57600, 115200, 128000, 230400, 256000, 460800, 921600};
    for (int baud : bauds)
        m_baudCombo->addItem(QString::number(baud));
    m_baudCombo->setCurrentText(QStringLiteral("115200"));
    lineRow->addWidget(m_rts);
    lineRow->addWidget(m_dtr);
    lineRow->addStretch();
    lineRow->addWidget(m_lbBaud);
    lineRow->addWidget(m_baudCombo);
    leftLay->addLayout(lineRow);

    auto *sendRow = new QHBoxLayout;
    sendRow->addStretch();
    m_btnSend = new QPushButton(left);
    m_btnSend->setMinimumSize(84, 32);
    sendRow->addWidget(m_btnSend);
    leftLay->addLayout(sendRow);
    leftLay->addStretch();
    bottom->addWidget(left);

    auto *rightLay = new QVBoxLayout;
    rightLay->setSpacing(3);
    auto *opt1 = new QHBoxLayout;
    opt1->setSpacing(6);
    m_hexDisp = new QCheckBox(column);
    m_btnSaveData = new QPushButton(column);
    m_recvToFile = new QCheckBox(column);
    m_hexSend = new QCheckBox(column);
    m_autoSend = new QCheckBox(column);
    m_interval = new QSpinBox(column);
    m_interval->setRange(1, 600000);
    m_interval->setValue(100);
    m_interval->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_interval->setFixedWidth(64);
    m_lbMs = new QLabel(column);
    m_newLine = new QCheckBox(column);
    opt1->addWidget(m_hexDisp);
    opt1->addWidget(m_btnSaveData);
    opt1->addWidget(m_recvToFile);
    opt1->addWidget(m_hexSend);
    opt1->addWidget(m_autoSend);
    opt1->addWidget(m_interval);
    opt1->addWidget(m_lbMs);
    opt1->addWidget(m_newLine);
    opt1->addStretch();
    rightLay->addLayout(opt1);

    auto *opt2 = new QHBoxLayout;
    opt2->setSpacing(4);
    m_packetMode = new QCheckBox(column);
    m_lbTimeout = new QLabel(column);
    m_timeout = new QSpinBox(column);
    m_timeout->setRange(1, 5000);
    m_timeout->setValue(20);
    m_timeout->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_timeout->setFixedWidth(52);
    m_lbMs2 = new QLabel(column);
    m_lbFrom = new QLabel(column);
    m_csFrom = new QSpinBox(column);
    m_csFrom->setRange(1, 9999);
    m_csFrom->setValue(1);
    m_csFrom->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_csFrom->setFixedWidth(48);
    m_lbToEnd = new QLabel(column);
    m_csTail = new QSpinBox(column);
    m_csTail->setRange(0, 9999);
    m_csTail->setValue(0);
    m_csTail->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_csTail->setFixedWidth(40);
    m_csTail->setToolTip(QStringLiteral("0 = through the last byte; N = skip N bytes at the end"));
    m_lbChecksum = new QLabel(column);
    m_checksum = new QComboBox(column);
    m_checksum->setFixedWidth(120);
    auto *helpBtn = new QPushButton(QStringLiteral("?"), column);
    helpBtn->setFixedWidth(22);
    connect(helpBtn, &QPushButton::clicked, this, [this] {
        QMessageBox::information(
            this, L(QStringLiteral("加校验"), QStringLiteral("Checksum")),
            L(QStringLiteral("从第 n 个字节算到末尾，再去掉末尾 m 个字节（m=0 表示一直到末尾）。\n"
                             "结果追加在整帧后面：Modbus CRC16 为低字节在前；ADD / XOR 为 1 字节。\n"
                             "多字符串按框内数据原样发送，不加这里的校验。"),
              QStringLiteral("Checksum covers byte n through the end, excluding the last m bytes (m=0 keeps the end).\n"
                             "It is appended to the frame: Modbus CRC16 is little-endian; ADD / XOR is one byte.\n"
                             "Multi-string buttons send their text as stored, without this checksum.")));
    });
    opt2->addWidget(m_packetMode);
    opt2->addWidget(m_lbTimeout);
    opt2->addWidget(m_timeout);
    opt2->addWidget(m_lbMs2);
    opt2->addWidget(m_lbFrom);
    opt2->addWidget(m_csFrom);
    opt2->addWidget(m_lbToEnd);
    opt2->addWidget(m_csTail);
    opt2->addWidget(m_lbChecksum);
    opt2->addWidget(m_checksum);
    opt2->addWidget(helpBtn);
    opt2->addStretch();
    rightLay->addLayout(opt2);

    m_send = new QPlainTextEdit(column);
    m_send->setFont(QFont(QStringLiteral("SimSun"), 9));
    m_send->setFixedHeight(64);
    m_send->setStyleSheet(QStringLiteral("QPlainTextEdit { background: white; color: black; }"));
    rightLay->addWidget(m_send);
    bottom->addLayout(rightLay, 1);
    col->addLayout(bottom);

    root->addWidget(column, 1);
    buildExtension();
    root->addWidget(m_extPanel);
    setCentralWidget(central);
    buildMoreDialog();
    setLed(false);

    m_countLabel = new QLabel(this);
    m_stateLabel = new QLabel(this);
    statusBar()->addWidget(m_countLabel);
    statusBar()->addPermanentWidget(m_stateLabel, 1);
}

void MainWindow::buildExtension()
{
    m_extPanel = new QWidget(this);
    m_extPanel->setFixedWidth(540);
    m_extPanel->setVisible(false);
    auto *lay = new QVBoxLayout(m_extPanel);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(4);

    auto *head = new QHBoxLayout;
    m_extTitle = new QLabel(m_extPanel);
    m_btnLoop = new QPushButton(m_extPanel);
    m_btnLoop->setCheckable(true);
    head->addWidget(m_extTitle);
    head->addWidget(m_btnLoop);
    head->addStretch();
    lay->addLayout(head);

    auto *scroll = new QScrollArea(m_extPanel);
    scroll->setWidgetResizable(true);
    auto *inner = new QWidget(scroll);
    auto *rows = new QVBoxLayout(inner);
    rows->setContentsMargins(0, 0, 0, 0);
    rows->setSpacing(1);
    m_rows.reserve(kPresetCount);
    for (int i = 0; i < kPresetCount; ++i) {
        auto *line = new QHBoxLayout;
        line->setSpacing(2);
        PresetRow row;
        row.order = new QSpinBox(inner);
        row.order->setRange(0, 999);
        row.order->setFixedWidth(46);
        row.order->setButtonSymbols(QAbstractSpinBox::NoButtons);
        row.button = new QPushButton(QString::number(i + 1), inner);
        row.button->setFixedWidth(120);
        row.hex = new QCheckBox(QStringLiteral("H"), inner);
        row.hex->setFixedWidth(32);
        row.text = new QLineEdit(inner);
        row.delayMs = new QSpinBox(inner);
        row.delayMs->setRange(0, 600000);
        row.delayMs->setValue(100);
        row.delayMs->setFixedWidth(64);
        row.delayMs->setButtonSymbols(QAbstractSpinBox::NoButtons);
        line->addWidget(row.order);
        line->addWidget(row.button);
        line->addWidget(row.hex);
        line->addWidget(row.text, 1);
        line->addWidget(row.delayMs);
        rows->addLayout(line);

        connect(row.button, &QPushButton::clicked, this, [this, i] { sendPreset(i); });
        row.button->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(row.button, &QPushButton::customContextMenuRequested, this, [this, i](const QPoint &) {
            QMenu menu;
            auto *rename = menu.addAction(L(QStringLiteral("重命名"), QStringLiteral("Rename")));
            if (menu.exec(QCursor::pos()) != rename)
                return;
            bool ok = false;
            const QString name = QInputDialog::getText(
                this, L(QStringLiteral("按钮名称"), QStringLiteral("Button name")),
                L(QStringLiteral("名称"), QStringLiteral("Name")), QLineEdit::Normal,
                m_rows[i].button->text(), &ok);
            if (ok && !name.trimmed().isEmpty())
                m_rows[i].button->setText(name.trimmed());
        });
        m_rows.push_back(row);
    }
    rows->addStretch();
    scroll->setWidget(inner);
    lay->addWidget(scroll, 1);
}

void MainWindow::buildMoreDialog()
{
    m_moreDlg = new QDialog(this);
    m_moreDlg->setModal(false);
    auto *form = new QFormLayout(m_moreDlg);
    m_dataBits = new QComboBox(m_moreDlg);
    m_dataBits->addItems({QStringLiteral("5"), QStringLiteral("6"), QStringLiteral("7"), QStringLiteral("8")});
    m_dataBits->setCurrentText(QStringLiteral("8"));
    m_parity = new QComboBox(m_moreDlg);
    m_stopBits = new QComboBox(m_moreDlg);
    m_stopBits->addItems({QStringLiteral("1"), QStringLiteral("1.5"), QStringLiteral("2")});
    m_flow = new QComboBox(m_moreDlg);
    m_moreDataLb = new QLabel(m_moreDlg);
    m_moreParityLb = new QLabel(m_moreDlg);
    m_moreStopLb = new QLabel(m_moreDlg);
    m_moreFlowLb = new QLabel(m_moreDlg);
    form->addRow(m_moreDataLb, m_dataBits);
    form->addRow(m_moreParityLb, m_parity);
    form->addRow(m_moreStopLb, m_stopBits);
    form->addRow(m_moreFlowLb, m_flow);
    auto *box = new QDialogButtonBox(QDialogButtonBox::Close, m_moreDlg);
    connect(box, &QDialogButtonBox::rejected, m_moreDlg, &QDialog::hide);
    form->addRow(box);
    connect(m_dataBits, &QComboBox::currentIndexChanged, this, [this] {
        if (!m_loading && m_serial->isOpen())
            openCurrentPort();
    });
    connect(m_parity, &QComboBox::currentIndexChanged, this, [this] {
        if (!m_loading && m_serial->isOpen())
            openCurrentPort();
    });
    connect(m_stopBits, &QComboBox::currentIndexChanged, this, [this] {
        if (!m_loading && m_serial->isOpen())
            openCurrentPort();
    });
    connect(m_flow, &QComboBox::currentIndexChanged, this, [this] {
        if (!m_loading && m_serial->isOpen())
            openCurrentPort();
    });
}

void MainWindow::buildMenus()
{
    m_menuPort = menuBar()->addMenu(QString());
    m_actRefresh = m_menuPort->addAction(QString());
    connect(m_actRefresh, &QAction::triggered, this, &MainWindow::refreshPorts);
    m_menuPort->addSeparator();
    connect(m_menuPort, &QMenu::aboutToShow, this, &MainWindow::rebuildPortMenu);

    m_menuSerial = menuBar()->addMenu(QString());
    m_actMore = m_menuSerial->addAction(QString());
    connect(m_actMore, &QAction::triggered, this, [this] {
        m_moreDlg->show();
        m_moreDlg->raise();
    });
    m_menuSerial->addSeparator();
    m_actAutoOpen = m_menuSerial->addAction(QString());
    m_actAutoOpen->setCheckable(true);
    m_menuSerial->addSeparator();
    m_actImport = m_menuSerial->addAction(QString());
    connect(m_actImport, &QAction::triggered, this, &MainWindow::importSscomIni);

    m_menuDisplay = menuBar()->addMenu(QString());
    m_actFont = m_menuDisplay->addAction(QString());
    connect(m_actFont, &QAction::triggered, this, &MainWindow::chooseFont);
    m_actBg = m_menuDisplay->addAction(QString());
    connect(m_actBg, &QAction::triggered, this, &MainWindow::chooseBackground);
    m_menuDisplay->addSeparator();
    m_menuBuffer = m_menuDisplay->addMenu(QString());
    auto *bufferGroup = new QActionGroup(this);
    bufferGroup->setExclusive(true);
    const int limits[] = {200000, 500000, 1000000, 2000000, 5000000};
    const char *limitText[] = {"200K", "500K", "1M", "2M", "5M"};
    for (int i = 0; i < 5; ++i) {
        auto *act = m_menuBuffer->addAction(QString::fromLatin1(limitText[i]));
        act->setCheckable(true);
        bufferGroup->addAction(act);
        if (limits[i] == 1000000)
            act->setChecked(true);
        const int chars = limits[i];
        connect(act, &QAction::triggered, this, [this, chars] { setBufferLimit(chars); });
        m_bufferActs.push_back(act);
    }

    m_menuSend = menuBar()->addMenu(QString());
    m_menuTerm = m_menuSend->addMenu(QString());
    m_actLineMode = m_menuTerm->addAction(QString());
    m_actLineMode->setCheckable(true);
    m_menuTerm->addSeparator();
    m_actKeySend = m_menuTerm->addAction(QString());
    m_actKeySend->setCheckable(true);
    m_actLocalEcho = m_menuTerm->addAction(QString());
    m_actLocalEcho->setCheckable(true);
    m_menuTerm->addSeparator();
    m_actCrLf = m_menuTerm->addAction(QString());
    m_actCrLf->setCheckable(true);
    m_menuSend->addSeparator();
    m_menuFileDelay = m_menuSend->addMenu(QString());
    auto *delayGroup = new QActionGroup(this);
    delayGroup->setExclusive(true);
    const int delays[] = {0, 1, 10, 50, 100};
    for (int delay : delays) {
        auto *act = m_menuFileDelay->addAction(QString());
        act->setCheckable(true);
        act->setData(delay);
        delayGroup->addAction(act);
        if (delay == 0)
            act->setChecked(true);
        connect(act, &QAction::triggered, this, [this, delay] { setFileDelay(delay); });
        m_delayActs.push_back(act);
    }

    m_actMulti = menuBar()->addAction(QString());
    connect(m_actMulti, &QAction::triggered, this, [this] {
        setExtensionVisible(!m_extPanel->isVisible());
    });

    m_menuTools = menuBar()->addMenu(QString());
    m_actCalc = m_menuTools->addAction(QString());
    connect(m_actCalc, &QAction::triggered, this, &MainWindow::showCalculator);

    m_menuHelp = menuBar()->addMenu(QString());
    m_actHelp = m_menuHelp->addAction(QString());
    connect(m_actHelp, &QAction::triggered, this, [this] {
        QMessageBox::information(
            this, L(QStringLiteral("使用帮助"), QStringLiteral("Help")),
            L(QStringLiteral(
                  "1. 选择端口号和波特率，点「打开串口」。数据位、校验、停止位、流控在「更多串口设置」。\n"
                  "2. HEX显示 / 保存数据 / 接收数据到文件 / HEX发送 / 定时发送 / 加回车换行。\n"
                  "3. 「加时间戳和分包显示」按超时时间把一段空闲后的数据分成一行。\n"
                  "4. 「扩展」打开 99 条多字符串。序号大于 0 的项按序号循环发送，右键按钮改名称。\n"
                  "5. 发送菜单可设终端按键，以及发送文件时每 256 字节的延时。\n"
                  "6. 「导入 sscom51.ini」可读原版多字符串。不含 TCP/IP。"),
              QStringLiteral(
                  "1. Pick a port and baud, then Open. Data bits, parity, stop bits and flow are under More.\n"
                  "2. HEX display, save text, raw RX log, HEX send, repeat send, append CRLF.\n"
                  "3. Timestamp + packets splits incoming data after the idle gap.\n"
                  "4. Ext opens 99 strings. Order greater than 0 loops in that order. Right-click a button to rename it.\n"
                  "5. The Send menu sets terminal keys and the per-256-byte file delay.\n"
                  "6. Import sscom51.ini reads the original multi-strings. No TCP/IP.")));
    });
    m_actAbout = m_menuHelp->addAction(QString());
    connect(m_actAbout, &QAction::triggered, this, &MainWindow::showAbout);
}

void MainWindow::applyLanguage()
{
    setWindowTitle(L(QStringLiteral("SSCOM — 串口调试助手"), QStringLiteral("SSCOM — Serial Debugger")));
    m_menuPort->setTitle(L(QStringLiteral("通讯端口"), QStringLiteral("Port")));
    m_actRefresh->setText(L(QStringLiteral("刷新串口列表"), QStringLiteral("Refresh ports")));
    m_menuSerial->setTitle(L(QStringLiteral("串口设置"), QStringLiteral("Serial")));
    m_actMore->setText(L(QStringLiteral("打开串口设置"), QStringLiteral("Serial settings")));
    m_actAutoOpen->setText(L(QStringLiteral("启动立即打开串口"), QStringLiteral("Open port on startup")));
    m_actImport->setText(L(QStringLiteral("导入 sscom51.ini…"), QStringLiteral("Import sscom51.ini…")));
    m_menuDisplay->setTitle(L(QStringLiteral("显示"), QStringLiteral("Display")));
    m_actFont->setText(L(QStringLiteral("字体/颜色/大小…"), QStringLiteral("Font / color / size…")));
    m_actBg->setText(L(QStringLiteral("背景颜色"), QStringLiteral("Background")));
    m_menuBuffer->setTitle(L(QStringLiteral("显示缓冲上限"), QStringLiteral("Buffer limit")));
    m_menuSend->setTitle(L(QStringLiteral("发送"), QStringLiteral("Send")));
    m_menuTerm->setTitle(L(QStringLiteral("终端仿真设置"), QStringLiteral("Terminal")));
    m_actLineMode->setText(L(QStringLiteral("回车后发送输入行(带回显)"), QStringLiteral("Enter sends the line (echo)")));
    m_actKeySend->setText(L(QStringLiteral("按键立即发送该键值"), QStringLiteral("Send each key at once")));
    m_actLocalEcho->setText(L(QStringLiteral("本地回显按键值"), QStringLiteral("Local echo")));
    m_actCrLf->setText(L(QStringLiteral("发送回车 0x0D 后自动加换行 0x0A"),
                          QStringLiteral("Append 0x0A after 0x0D")));
    m_menuFileDelay->setTitle(L(QStringLiteral("发送文件延时设置"), QStringLiteral("File send delay")));
    const QStringList delayText = {
        L(QStringLiteral("连续发送"), QStringLiteral("No delay")),
        L(QStringLiteral("每次发送 256 字节延时 1ms"), QStringLiteral("1 ms per 256 bytes")),
        L(QStringLiteral("每次发送 256 字节延时 10ms"), QStringLiteral("10 ms per 256 bytes")),
        L(QStringLiteral("每次发送 256 字节延时 50ms"), QStringLiteral("50 ms per 256 bytes")),
        L(QStringLiteral("每次发送 256 字节延时 100ms"), QStringLiteral("100 ms per 256 bytes"))};
    for (int i = 0; i < m_delayActs.size() && i < delayText.size(); ++i)
        m_delayActs[i]->setText(delayText[i]);
    m_actMulti->setText(L(QStringLiteral("多字符串"), QStringLiteral("Multi-string")));
    m_menuTools->setTitle(L(QStringLiteral("小工具"), QStringLiteral("Tools")));
    m_actCalc->setText(L(QStringLiteral("转换计算器"), QStringLiteral("Converter")));
    m_menuHelp->setTitle(L(QStringLiteral("帮助"), QStringLiteral("Help")));
    m_actHelp->setText(L(QStringLiteral("使用帮助"), QStringLiteral("How to use")));
    m_actAbout->setText(L(QStringLiteral("关于"), QStringLiteral("About")));

    m_btnClearRecv->setText(L(QStringLiteral("清除窗口"), QStringLiteral("Clear")));
    m_btnOpenFile->setText(L(QStringLiteral("打开文件"), QStringLiteral("Open file")));
    m_btnSendFile->setText(L(QStringLiteral("发送文件"), QStringLiteral("Send file")));
    m_btnStop->setText(L(QStringLiteral("停止"), QStringLiteral("Stop")));
    m_btnClearSend->setText(L(QStringLiteral("清发送区"), QStringLiteral("Clear TX")));
    m_stayOnTop->setText(L(QStringLiteral("最前"), QStringLiteral("Top")));
    m_btnSaveParams->setText(L(QStringLiteral("保存参数"), QStringLiteral("Save")));
    m_btnExt->setText(m_extPanel->isVisible()
                          ? L(QStringLiteral("隐藏"), QStringLiteral("Hide"))
                          : L(QStringLiteral("扩展"), QStringLiteral("Ext")));
    m_lbPort->setText(L(QStringLiteral("端口号"), QStringLiteral("Port")));
    m_btnOpen->setText(m_serial->isOpen()
                           ? L(QStringLiteral("关闭串口"), QStringLiteral("Close"))
                           : L(QStringLiteral("打开串口"), QStringLiteral("Open")));
    m_btnMore->setText(L(QStringLiteral("更多串口设置"), QStringLiteral("More")));
    m_lbBaud->setText(L(QStringLiteral("波特率"), QStringLiteral("Baud")));
    m_btnSend->setText(L(QStringLiteral("发送"), QStringLiteral("Send")));
    m_hexDisp->setText(L(QStringLiteral("HEX显示"), QStringLiteral("HEX RX")));
    m_btnSaveData->setText(L(QStringLiteral("保存数据"), QStringLiteral("Save text")));
    m_recvToFile->setText(L(QStringLiteral("接收数据到文件"), QStringLiteral("Log RX")));
    m_hexSend->setText(L(QStringLiteral("HEX发送"), QStringLiteral("HEX TX")));
    m_autoSend->setText(L(QStringLiteral("定时发送"), QStringLiteral("Repeat")));
    m_lbMs->setText(L(QStringLiteral("ms/次"), QStringLiteral("ms")));
    m_newLine->setText(L(QStringLiteral("加回车换行"), QStringLiteral("CRLF")));
    m_packetMode->setText(L(QStringLiteral("加时间戳和分包显示"), QStringLiteral("Timestamp + packets")));
    m_lbTimeout->setText(L(QStringLiteral("超时"), QStringLiteral("Gap")));
    m_lbMs2->setText(QStringLiteral("ms"));
    m_lbFrom->setText(L(QStringLiteral("第"), QStringLiteral("From")));
    m_lbToEnd->setText(L(QStringLiteral("字节至末尾，倒数"), QStringLiteral("to end, skip")));
    m_lbChecksum->setText(L(QStringLiteral("加校验"), QStringLiteral("Checksum")));
    m_extTitle->setText(L(QStringLiteral("多字符串（序号 / 名称 / H / 内容 / 延时ms）"),
                          QStringLiteral("Multi-string (order / name / H / data / delay ms)")));
    m_btnLoop->setText(m_looping
                           ? L(QStringLiteral("停止循环"), QStringLiteral("Stop loop"))
                           : L(QStringLiteral("循环发送"), QStringLiteral("Loop")));
    m_moreDlg->setWindowTitle(L(QStringLiteral("更多串口设置"), QStringLiteral("Serial settings")));
    m_moreDataLb->setText(L(QStringLiteral("数据位"), QStringLiteral("Data bits")));
    m_moreParityLb->setText(L(QStringLiteral("校验"), QStringLiteral("Parity")));
    m_moreStopLb->setText(L(QStringLiteral("停止位"), QStringLiteral("Stop bits")));
    m_moreFlowLb->setText(L(QStringLiteral("流控"), QStringLiteral("Flow")));

    const int parityIndex = m_parity->currentIndex();
    const int flowIndex = m_flow->currentIndex();
    const int sumIndex = m_checksum->currentIndex();
    {
        QSignalBlocker p(m_parity);
        QSignalBlocker f(m_flow);
        QSignalBlocker s(m_checksum);
        m_parity->clear();
        m_flow->clear();
        m_checksum->clear();
        if (m_english) {
            m_parity->addItems({QStringLiteral("None"), QStringLiteral("Odd"), QStringLiteral("Even"),
                                QStringLiteral("Mark"), QStringLiteral("Space")});
            m_flow->addItems({QStringLiteral("None"), QStringLiteral("Hardware"), QStringLiteral("Software")});
            m_checksum->addItems({QStringLiteral("None"), QStringLiteral("Modbus CRC16"),
                                  QStringLiteral("ADD"), QStringLiteral("XOR")});
        } else {
            m_parity->addItems({QStringLiteral("无"), QStringLiteral("奇"), QStringLiteral("偶"),
                                QStringLiteral("标记"), QStringLiteral("空格")});
            m_flow->addItems({QStringLiteral("无"), QStringLiteral("硬件"), QStringLiteral("软件")});
            m_checksum->addItems({QStringLiteral("无"), QStringLiteral("Modbus CRC16"),
                                  QStringLiteral("ADD"), QStringLiteral("XOR")});
        }
        m_parity->setCurrentIndex(parityIndex < 0 ? 0 : parityIndex);
        m_flow->setCurrentIndex(flowIndex < 0 ? 0 : flowIndex);
        m_checksum->setCurrentIndex(sumIndex < 0 ? 0 : sumIndex);
    }
    for (int i = 0; i < m_rows.size(); ++i) {
        m_rows[i].order->setToolTip(L(QStringLiteral("循环序号，0 表示不参加循环"),
                                      QStringLiteral("Loop order, 0 skips this row")));
        m_rows[i].button->setToolTip(L(QStringLiteral("点击发送，右键重命名"),
                                       QStringLiteral("Click to send, right-click to rename")));
        m_rows[i].delayMs->setToolTip(L(QStringLiteral("这条发出后的等待毫秒"),
                                        QStringLiteral("Delay after this row, ms")));
    }
    updateStatus();
}

void MainWindow::applyRecvStyle()
{
    m_receive->setStyleSheet(QStringLiteral("QPlainTextEdit { background:%1; color:%2; }")
                                 .arg(m_recvBg.name(), m_recvFg.name()));
}

void MainWindow::setLed(bool open)
{
    m_led->setStyleSheet(open
                             ? QStringLiteral("background:#2E7D32; border-radius:6px;")
                             : QStringLiteral("background:#C42B1C; border-radius:6px;"));
    if (m_btnOpen) {
        m_btnOpen->setStyleSheet(open
                                     ? QStringLiteral("QPushButton { background:#2E7D32; color:white; }")
                                     : QString());
    }
}

QString MainWindow::currentPortName() const
{
    return m_portCombo->currentData().toString();
}

int MainWindow::currentBaud() const
{
    bool ok = false;
    const int baud = m_baudCombo->currentText().trimmed().toInt(&ok);
    return ok ? baud : 0;
}

QString MainWindow::parityToken() const
{
    switch (m_parity->currentIndex()) {
    case 1: return QStringLiteral("Odd");
    case 2: return QStringLiteral("Even");
    case 3: return QStringLiteral("Mark");
    case 4: return QStringLiteral("Space");
    default: return QStringLiteral("None");
    }
}

QString MainWindow::flowToken() const
{
    switch (m_flow->currentIndex()) {
    case 1: return QStringLiteral("Hardware");
    case 2: return QStringLiteral("Software");
    default: return QStringLiteral("None");
    }
}

QString MainWindow::stopToken() const
{
    return m_stopBits->currentText();
}

void MainWindow::updateStatus()
{
    m_countLabel->setText(QStringLiteral("S:%1   R:%2").arg(m_tx).arg(m_rx));
    const QString port = currentPortName().isEmpty() ? QStringLiteral("-") : currentPortName();
    const QString state = m_serial->isOpen()
                              ? L(QStringLiteral("已打开"), QStringLiteral("open"))
                              : L(QStringLiteral("已关闭"), QStringLiteral("closed"));
    m_stateLabel->setText(QStringLiteral("%1 %2  %3bps,%4,%5,%6,%7")
                              .arg(port, state)
                              .arg(currentBaud())
                              .arg(m_dataBits->currentText(), stopToken(), parityToken(), flowToken()));
}

void MainWindow::refreshPorts()
{
    const QString keep = currentPortName();
    QSignalBlocker blocker(m_portCombo);
    m_portCombo->clear();
    const auto ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : ports) {
        m_portCombo->addItem(info.portName(), info.portName());
        if (!info.description().isEmpty())
            m_portCombo->setItemData(m_portCombo->count() - 1, info.description(), Qt::ToolTipRole);
    }
    if (!keep.isEmpty()) {
        const int found = m_portCombo->findData(keep);
        if (found >= 0)
            m_portCombo->setCurrentIndex(found);
        else {
            m_portCombo->addItem(keep, keep);
            m_portCombo->setCurrentIndex(m_portCombo->count() - 1);
        }
    } else if (m_portCombo->count() == 0) {
        m_portCombo->addItem(L(QStringLiteral("(无可用端口)"), QStringLiteral("(no ports)")), QString());
    }
    updateStatus();
}

void MainWindow::rebuildPortMenu()
{
    const auto actions = m_menuPort->actions();
    bool drop = false;
    QList<QAction *> stale;
    for (QAction *action : actions) {
        if (drop)
            stale.append(action);
        else if (action->isSeparator())
            drop = true;
    }
    for (QAction *action : stale) {
        m_menuPort->removeAction(action);
        action->deleteLater();
    }
    const QString current = currentPortName();
    for (int i = 0; i < m_portCombo->count(); ++i) {
        const QString name = m_portCombo->itemData(i).toString();
        if (name.isEmpty())
            continue;
        QString label = name;
        const QString tip = m_portCombo->itemData(i, Qt::ToolTipRole).toString();
        if (!tip.isEmpty())
            label += QStringLiteral("  ") + tip;
        auto *action = m_menuPort->addAction(label);
        action->setCheckable(true);
        action->setChecked(name == current);
        connect(action, &QAction::triggered, this, [this, name] { selectPort(name); });
    }
}

void MainWindow::selectPort(const QString &name)
{
    const int index = m_portCombo->findData(name);
    if (index >= 0)
        m_portCombo->setCurrentIndex(index);
}

bool MainWindow::openCurrentPort()
{
    const QString name = currentPortName();
    const int baud = currentBaud();
    const auto dataBits = dataBitsFrom(m_dataBits->currentText().toInt());
    const auto parity = parityFrom(m_parity->currentIndex());
    const auto stopBits = stopFrom(m_stopBits->currentText());
    const auto flow = flowFrom(m_flow->currentIndex());
    if (m_serial->isOpen()
        && m_serial->portName() == name
        && m_serial->baudRate() == baud
        && m_serial->dataBits() == dataBits
        && m_serial->parity() == parity
        && m_serial->stopBits() == stopBits
        && m_serial->flowControl() == flow) {
        setLed(true);
        m_btnOpen->setText(L(QStringLiteral("关闭串口"), QStringLiteral("Close")));
        updateStatus();
        return true;
    }
    if (name.isEmpty() || baud <= 0) {
        statusBar()->showMessage(L(QStringLiteral("请选择端口和有效波特率"),
                                   QStringLiteral("Choose a port and a valid baud rate")));
        return false;
    }
    m_ignoreSerialError = true;
    if (m_serial->isOpen())
        m_serial->close();
    m_serial->setPortName(name);
    m_serial->setBaudRate(baud);
    m_serial->setDataBits(dataBits);
    m_serial->setParity(parity);
    m_serial->setStopBits(stopBits);
    m_serial->setFlowControl(flow);
    const bool ok = m_serial->open(QIODevice::ReadWrite);
    m_ignoreSerialError = false;
    if (!ok) {
        setLed(false);
        m_btnOpen->setText(L(QStringLiteral("打开串口"), QStringLiteral("Open")));
        statusBar()->showMessage(L(QStringLiteral("打开失败：%1"), QStringLiteral("Open failed: %1"))
                                     .arg(m_serial->errorString()));
        updateStatus();
        return false;
    }
    m_serial->setDataTerminalReady(m_dtr->isChecked());
    m_serial->setRequestToSend(m_rts->isChecked());
    setLed(true);
    m_btnOpen->setText(L(QStringLiteral("关闭串口"), QStringLiteral("Close")));
    statusBar()->showMessage(L(QStringLiteral("已打开 %1"), QStringLiteral("Opened %1")).arg(name));
    updateStatus();
    return true;
}

void MainWindow::closePort()
{
    stopSending();
    m_ignoreSerialError = true;
    if (m_serial->isOpen())
        m_serial->close();
    m_ignoreSerialError = false;
    setLed(false);
    if (m_btnOpen)
        m_btnOpen->setText(L(QStringLiteral("打开串口"), QStringLiteral("Open")));
    updateStatus();
}

void MainWindow::toggleOpen()
{
    if (m_serial->isOpen()) {
        closePort();
        statusBar()->showMessage(L(QStringLiteral("串口已关闭"), QStringLiteral("Port closed")));
        return;
    }
    openCurrentPort();
}

void MainWindow::onSerialError(QSerialPort::SerialPortError error)
{
    if (m_ignoreSerialError || error == QSerialPort::NoError || error == QSerialPort::TimeoutError)
        return;
    if (error == QSerialPort::ResourceError) {
        statusBar()->showMessage(L(QStringLiteral("串口已断开：%1"), QStringLiteral("Port lost: %1"))
                                     .arg(m_serial->errorString()));
        closePort();
        return;
    }
    statusBar()->showMessage(m_serial->errorString());
}

bool MainWindow::transmit(const QByteArray &data)
{
    if (!m_serial->isOpen()) {
        statusBar()->showMessage(L(QStringLiteral("请先打开串口"), QStringLiteral("Open the port first")));
        return false;
    }
    if (data.isEmpty())
        return true;
    const qint64 written = m_serial->write(data);
    if (written < 0) {
        statusBar()->showMessage(L(QStringLiteral("发送失败：%1"), QStringLiteral("Send failed: %1"))
                                     .arg(m_serial->errorString()));
        return false;
    }
    m_tx += written;
    updateStatus();
    return true;
}

QByteArray MainWindow::composeMain(QString *error) const
{
    QByteArray payload;
    if (m_hexSend->isChecked()) {
        QString hexError;
        payload = codec::parseHex(m_send->toPlainText(), &hexError);
        if (!hexError.isEmpty()) {
            if (error)
                *error = L(QStringLiteral("HEX 格式不正确"), QStringLiteral("Invalid HEX"));
            return {};
        }
    } else {
        payload = m_send->toPlainText().toLocal8Bit();
    }
    if (m_newLine->isChecked())
        payload.append("\r\n", 2);
    return codec::appendChecksum(payload, m_checksum->currentIndex(), m_csFrom->value(), m_csTail->value());
}

QByteArray MainWindow::composePreset(int row, QString *error) const
{
    const QString text = m_rows[row].text->text();
    if (!m_rows[row].hex->isChecked())
        return text.toLocal8Bit();
    QString hexError;
    const QByteArray payload = codec::parseHex(text, &hexError);
    if (!hexError.isEmpty()) {
        if (error)
            *error = L(QStringLiteral("第 %1 条 HEX 格式不正确"), QStringLiteral("Row %1 has invalid HEX")).arg(row + 1);
        return {};
    }
    return payload;
}

void MainWindow::sendMain()
{
    if (!m_serial->isOpen()) {
        statusBar()->showMessage(L(QStringLiteral("请先打开串口"), QStringLiteral("Open the port first")));
        if (m_autoSend->isChecked()) {
            QSignalBlocker blocker(m_autoSend);
            m_autoSend->setChecked(false);
            m_autoTimer.stop();
        }
        return;
    }
    QString error;
    const QByteArray payload = composeMain(&error);
    if (!error.isEmpty()) {
        statusBar()->showMessage(error);
        if (m_autoSend->isChecked()) {
            QSignalBlocker blocker(m_autoSend);
            m_autoSend->setChecked(false);
            m_autoTimer.stop();
        }
        return;
    }
    if (payload.isEmpty()) {
        statusBar()->showMessage(L(QStringLiteral("发送内容为空"), QStringLiteral("Nothing to send")));
        return;
    }
    if (transmit(payload))
        statusBar()->showMessage(L(QStringLiteral("已发送 %1 字节"), QStringLiteral("Sent %1 bytes")).arg(payload.size()));
}

void MainWindow::sendPreset(int row)
{
    QString error;
    const QByteArray payload = composePreset(row, &error);
    if (!error.isEmpty()) {
        statusBar()->showMessage(error);
        return;
    }
    if (payload.isEmpty()) {
        statusBar()->showMessage(L(QStringLiteral("这一条是空的"), QStringLiteral("This row is empty")));
        return;
    }
    if (transmit(payload))
        statusBar()->showMessage(L(QStringLiteral("已发送第 %1 条，%2 字节"),
                                   QStringLiteral("Sent row %1, %2 bytes"))
                                     .arg(row + 1)
                                     .arg(payload.size()));
}

void MainWindow::clearReceive()
{
    m_receive->clear();
    m_packet.clear();
    m_textPending.clear();
    m_lineBuf.clear();
    m_lineEchoChars = 0;
}

void MainWindow::stopSending()
{
    m_autoTimer.stop();
    if (m_autoSend->isChecked()) {
        QSignalBlocker blocker(m_autoSend);
        m_autoSend->setChecked(false);
    }
    m_fileTimer.stop();
    m_fileSending = false;
    if (m_sendFile.isOpen())
        m_sendFile.close();
    m_loopTimer.stop();
    m_looping = false;
    if (m_btnLoop->isChecked()) {
        QSignalBlocker blocker(m_btnLoop);
        m_btnLoop->setChecked(false);
    }
    if (m_btnLoop)
        m_btnLoop->setText(L(QStringLiteral("循环发送"), QStringLiteral("Loop")));
}

void MainWindow::onReadyRead()
{
    const QByteArray data = m_serial->readAll();
    if (data.isEmpty())
        return;
    m_rx += data.size();
    if (m_recvFile.isOpen())
        m_recvFile.write(data);
    if (m_packetMode->isChecked()) {
        m_packet += data;
        m_packetTimer.start(qMax(1, m_timeout->value()));
    } else {
        appendRx(data, false);
    }
    updateStatus();
}

void MainWindow::flushPacket()
{
    if (m_packet.isEmpty())
        return;
    const QByteArray data = m_packet;
    m_packet.clear();
    appendRx(data, true);
}

void MainWindow::appendRx(const QByteArray &data, bool packetLine)
{
    QString body;
    if (m_hexDisp->isChecked()) {
        body = codec::toHex(data);
    } else if (packetLine) {
        body = QString::fromLocal8Bit(data);
    } else {
        m_textPending += data;
        int ready = m_textPending.size();
        if (ready > 0) {
            const auto lead = static_cast<quint8>(m_textPending.at(ready - 1));
            if (lead >= 0x81 && lead <= 0xFE)
                --ready;
        }
        if (ready <= 0)
            return;
        body = QString::fromLocal8Bit(m_textPending.left(ready));
        m_textPending = m_textPending.mid(ready);
    }
    if (packetLine)
        insertReceive(QDateTime::currentDateTime().toString(QStringLiteral("[hh:mm:ss.zzz] ")) + body + QLatin1Char('\n'));
    else
        insertReceive(body);
}

void MainWindow::insertReceive(const QString &text)
{
    QTextCursor cursor = m_receive->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(text);
    m_receive->setTextCursor(cursor);
    m_receive->ensureCursorVisible();
    trimReceive();
}

void MainWindow::trimReceive()
{
    auto *doc = m_receive->document();
    const int extra = doc->characterCount() - m_bufferChars;
    if (extra <= 0)
        return;
    QTextCursor cursor(doc);
    cursor.setPosition(0);
    cursor.setPosition(qMin(extra, doc->characterCount() - 1), QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
}

void MainWindow::eraseLastReceiveChar()
{
    QTextCursor cursor = m_receive->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.deletePreviousChar();
    m_receive->setTextCursor(cursor);
}

void MainWindow::setRecvToFile(bool on)
{
    if (!on) {
        m_recvFile.close();
        statusBar()->showMessage(L(QStringLiteral("已停止写入接收文件"), QStringLiteral("RX log stopped")));
        return;
    }
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                        + QStringLiteral("/sscom-qt");
    QDir().mkpath(dir);
    const QString port = currentPortName().isEmpty() ? QStringLiteral("COM") : currentPortName();
    const QString name = QStringLiteral("ReceivedTofile-%1-%2.DAT")
                             .arg(port, QDateTime::currentDateTime().toString(QStringLiteral("yyyy_M_d_H-mm-ss")));
    m_recvFile.setFileName(dir + QLatin1Char('/') + name);
    if (!m_recvFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
        QSignalBlocker blocker(m_recvToFile);
        m_recvToFile->setChecked(false);
        statusBar()->showMessage(L(QStringLiteral("无法创建接收文件"), QStringLiteral("Could not create the RX log")));
        return;
    }
    statusBar()->showMessage(L(QStringLiteral("接收原始数据写入 %1"), QStringLiteral("Logging raw RX to %1"))
                                 .arg(m_recvFile.fileName()));
}

void MainWindow::saveReceiveText()
{
    const QString suggested = QStringLiteral("SaveWindows%1.TXT")
                                  .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy_M_d_H-mm-ss")));
    const QString path = QFileDialog::getSaveFileName(
        this, L(QStringLiteral("保存数据"), QStringLiteral("Save text")), suggested,
        QStringLiteral("Text (*.TXT *.txt);;All (*.*)"));
    if (path.isEmpty())
        return;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        statusBar()->showMessage(L(QStringLiteral("保存失败"), QStringLiteral("Save failed")));
        return;
    }
    file.write(m_receive->toPlainText().toLocal8Bit());
    statusBar()->showMessage(L(QStringLiteral("已保存 %1"), QStringLiteral("Saved %1")).arg(path));
}

void MainWindow::openFileToSend()
{
    const QString path = QFileDialog::getOpenFileName(
        this, L(QStringLiteral("打开文件"), QStringLiteral("Open file")));
    if (path.isEmpty())
        return;
    m_filePath->setText(path);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        statusBar()->showMessage(L(QStringLiteral("无法读取文件"), QStringLiteral("Could not read the file")));
        return;
    }
    if (file.size() > 256 * 1024) {
        statusBar()->showMessage(L(QStringLiteral("文件较大，未载入发送区，可用「发送文件」按 256 字节送出"),
                                   QStringLiteral("File is large. Use Send file to stream it in 256-byte chunks.")));
        return;
    }
    const QByteArray data = file.readAll();
    if (m_hexSend->isChecked())
        m_send->setPlainText(codec::toHex(data));
    else
        m_send->setPlainText(QString::fromLocal8Bit(data));
}

void MainWindow::startFileSend()
{
    if (m_fileSending)
        return;
    if (!m_serial->isOpen()) {
        statusBar()->showMessage(L(QStringLiteral("请先打开串口"), QStringLiteral("Open the port first")));
        return;
    }
    QString path = m_filePath->text().trimmed();
    if (path.isEmpty() || !QFile::exists(path)) {
        path = QFileDialog::getOpenFileName(this, L(QStringLiteral("发送文件"), QStringLiteral("Send file")));
        if (path.isEmpty())
            return;
        m_filePath->setText(path);
    }
    m_sendFile.setFileName(path);
    if (!m_sendFile.open(QIODevice::ReadOnly)) {
        statusBar()->showMessage(L(QStringLiteral("无法读取文件"), QStringLiteral("Could not read the file")));
        return;
    }
    m_fileSending = true;
    m_fileTimer.setInterval(qMax(0, m_fileDelayMs));
    m_fileTimer.start();
    statusBar()->showMessage(L(QStringLiteral("正在发送文件…"), QStringLiteral("Sending file…")));
}

void MainWindow::fileTick()
{
    if (!m_fileSending || !m_sendFile.isOpen()) {
        m_fileTimer.stop();
        return;
    }
    const QByteArray chunk = m_sendFile.read(256);
    if (chunk.isEmpty()) {
        m_fileTimer.stop();
        m_sendFile.close();
        m_fileSending = false;
        statusBar()->showMessage(L(QStringLiteral("文件发送完成"), QStringLiteral("File sent")));
        return;
    }
    if (!transmit(chunk)) {
        m_fileTimer.stop();
        m_sendFile.close();
        m_fileSending = false;
    }
}

void MainWindow::startLoop()
{
    m_loopItems.clear();
    m_loopPos = 0;
    struct Item {
        int order;
        int row;
        int delayMs;
    };
    QVector<Item> items;
    for (int i = 0; i < m_rows.size(); ++i) {
        const int order = m_rows[i].order->value();
        if (order <= 0)
            continue;
        items.push_back({order, i, m_rows[i].delayMs->value()});
    }
    std::sort(items.begin(), items.end(), [](const Item &a, const Item &b) {
        if (a.order != b.order)
            return a.order < b.order;
        return a.row < b.row;
    });
    if (items.isEmpty() || !m_serial->isOpen()) {
        QSignalBlocker blocker(m_btnLoop);
        m_btnLoop->setChecked(false);
        m_looping = false;
        statusBar()->showMessage(items.isEmpty()
                                     ? L(QStringLiteral("把要循环的条目序号设为 1、2、3…"),
                                         QStringLiteral("Set order 1, 2, 3… on the rows to loop"))
                                     : L(QStringLiteral("请先打开串口"), QStringLiteral("Open the port first")));
        return;
    }
    for (const Item &item : items)
        m_loopItems.push_back({item.row, item.delayMs});
    m_looping = true;
    m_btnLoop->setText(L(QStringLiteral("停止循环"), QStringLiteral("Stop loop")));
    loopTick();
}

void MainWindow::loopTick()
{
    if (!m_looping || m_loopItems.isEmpty())
        return;
    if (m_loopPos >= m_loopItems.size())
        m_loopPos = 0;
    const LoopItem item = m_loopItems[m_loopPos];
    QString error;
    const QByteArray payload = composePreset(item.row, &error);
    if (!error.isEmpty()) {
        stopSending();
        statusBar()->showMessage(error);
        return;
    }
    if (!payload.isEmpty() && !transmit(payload)) {
        stopSending();
        return;
    }
    const int delay = item.delayMs;
    m_loopPos = (m_loopPos + 1) % m_loopItems.size();
    m_loopTimer.start(qMax(1, delay));
}

void MainWindow::setExtensionVisible(bool visible)
{
    if (m_extPanel->isVisible() == visible)
        return;
    m_extPanel->setVisible(visible);
    m_btnExt->setText(visible
                          ? L(QStringLiteral("隐藏"), QStringLiteral("Hide"))
                          : L(QStringLiteral("扩展"), QStringLiteral("Ext")));
    if (!m_loading && visible && width() < m_baseWidth + m_extPanel->width())
        resize(width() + m_extPanel->width() + 8, height());
}

void MainWindow::applyStayOnTop(bool on)
{
    const bool was = windowFlags().testFlag(Qt::WindowStaysOnTopHint);
    if (was == on)
        return;
    setWindowFlag(Qt::WindowStaysOnTopHint, on);
    if (isVisible())
        show();
}

void MainWindow::setBufferLimit(int chars)
{
    m_bufferChars = qMax(10000, chars);
    trimReceive();
}

void MainWindow::setFileDelay(int ms)
{
    m_fileDelayMs = qMax(0, ms);
    if (m_fileTimer.isActive())
        m_fileTimer.setInterval(m_fileDelayMs);
}

void MainWindow::chooseFont()
{
    bool ok = false;
    const QFont font = QFontDialog::getFont(&ok, m_receive->font(), this,
                                            L(QStringLiteral("字体"), QStringLiteral("Font")));
    if (!ok)
        return;
    m_receive->setFont(font);
    const QColor color = QColorDialog::getColor(m_recvFg, this, L(QStringLiteral("字体颜色"), QStringLiteral("Text color")));
    if (color.isValid()) {
        m_recvFg = color;
        applyRecvStyle();
    }
}

void MainWindow::chooseBackground()
{
    const QColor color = QColorDialog::getColor(m_recvBg, this, L(QStringLiteral("背景颜色"), QStringLiteral("Background")));
    if (!color.isValid())
        return;
    m_recvBg = color;
    applyRecvStyle();
}

void MainWindow::showAbout()
{
    QMessageBox::about(
        this, L(QStringLiteral("关于"), QStringLiteral("About")),
        L(QStringLiteral("SSCOM 串口调试助手（Qt 版）\n\n"
                         "布局和串口功能参考 SSCOM 5.13，不含 TCP/IP、ISP 烧写和 LUA。\n"
                         "ASCII 按系统本地编码（中文 Windows 上为 GBK）收发。"),
          QStringLiteral("SSCOM serial debugger (Qt)\n\n"
                         "Layout and serial features follow SSCOM 5.13, without TCP/IP, ISP, or LUA.\n"
                         "ASCII uses the system local encoding (GBK on Chinese Windows).")));
}

void MainWindow::showCalculator()
{
    QDialog dialog(this);
    dialog.setWindowTitle(L(QStringLiteral("转换计算器"), QStringLiteral("Converter")));
    dialog.resize(420, 280);
    auto *lay = new QVBoxLayout(&dialog);
    auto *edit = new QLineEdit(&dialog);
    edit->setPlaceholderText(QStringLiteral("01 03 00 00 00 01"));
    auto *out = new QPlainTextEdit(&dialog);
    out->setReadOnly(true);
    auto *btn = new QPushButton(L(QStringLiteral("计算"), QStringLiteral("Calculate")), &dialog);
    lay->addWidget(new QLabel(L(QStringLiteral("HEX 字节"), QStringLiteral("HEX bytes")), &dialog));
    lay->addWidget(edit);
    lay->addWidget(btn);
    lay->addWidget(out, 1);
    connect(btn, &QPushButton::clicked, &dialog, [edit, out, this] {
        QString error;
        const QByteArray data = codec::parseHex(edit->text(), &error);
        if (!error.isEmpty()) {
            out->setPlainText(L(QStringLiteral("HEX 格式不正确"), QStringLiteral("Invalid HEX")));
            return;
        }
        quint64 value = 0;
        const int take = qMin(8, data.size());
        for (int i = 0; i < take; ++i)
            value = (value << 8) | static_cast<quint8>(data.at(i));
        const QByteArray sum = codec::appendChecksum(data, 2, 1, 0);
        const QByteArray xored = codec::appendChecksum(data, 3, 1, 0);
        const QByteArray crc = codec::appendChecksum(data, 1, 1, 0);
        out->setPlainText(
            QStringLiteral("len: %1\nDEC: %2\nADD: %3\nXOR: %4\nModbus CRC16: %5 %6")
                .arg(data.size())
                .arg(value)
                .arg(data.isEmpty() ? QStringLiteral("-") : QString::number(static_cast<quint8>(sum.back()), 16).toUpper())
                .arg(data.isEmpty() ? QStringLiteral("-") : QString::number(static_cast<quint8>(xored.back()), 16).toUpper())
                .arg(data.isEmpty() ? QStringLiteral("-") : QString::number(static_cast<quint8>(crc.at(crc.size() - 2)), 16).rightJustified(2, QLatin1Char('0')).toUpper())
                .arg(data.isEmpty() ? QString() : QString::number(static_cast<quint8>(crc.back()), 16).rightJustified(2, QLatin1Char('0')).toUpper()));
    });
    dialog.exec();
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_receive && event->type() == QEvent::KeyPress) {
        auto *key = static_cast<QKeyEvent *>(event);
        if (key->modifiers() & (Qt::ControlModifier | Qt::AltModifier))
            return false;
        if (m_actLineMode->isChecked() || m_actKeySend->isChecked())
            return handleTerminalKey(key);
    }
    return QMainWindow::eventFilter(watched, event);
}

bool MainWindow::handleTerminalKey(QKeyEvent *event)
{
    const bool lineMode = m_actLineMode->isChecked();
    const auto sendBytes = [this](const QByteArray &bytes, const QString &echo) {
        if (!m_serial->isOpen()) {
            statusBar()->showMessage(L(QStringLiteral("请先打开串口"), QStringLiteral("Open the port first")));
            return;
        }
        transmit(bytes);
        if (!echo.isEmpty())
            insertReceive(echo);
    };

    if (lineMode) {
        if (event->key() == Qt::Key_Backspace) {
            if (!m_lineBuf.isEmpty()) {
                m_lineBuf.chop(1);
                if (m_lineEchoChars > 0) {
                    eraseLastReceiveChar();
                    --m_lineEchoChars;
                }
            }
            return true;
        }
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            QByteArray bytes = m_lineBuf.toLocal8Bit();
            bytes.append('\r');
            if (m_actCrLf->isChecked())
                bytes.append('\n');
            sendBytes(bytes, QStringLiteral("\n"));
            m_lineBuf.clear();
            m_lineEchoChars = 0;
            return true;
        }
        const QString text = event->text();
        if (text.isEmpty() || text.at(0).isNonCharacter() || text.at(0).unicode() < 32)
            return true;
        m_lineBuf += text;
        insertReceive(text);
        m_lineEchoChars += text.size();
        return true;
    }

    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        QByteArray bytes(1, '\r');
        QString echo;
        if (m_actCrLf->isChecked())
            bytes.append('\n');
        if (m_actLocalEcho->isChecked())
            echo = QStringLiteral("\n");
        sendBytes(bytes, echo);
        return true;
    }
    if (event->key() == Qt::Key_Backspace) {
        sendBytes(QByteArray(1, '\x08'), QString());
        if (m_actLocalEcho->isChecked())
            eraseLastReceiveChar();
        return true;
    }
    if (event->key() == Qt::Key_Tab) {
        const QString echo = m_actLocalEcho->isChecked() ? QStringLiteral("\t") : QString();
        sendBytes(QByteArray(1, '\t'), echo);
        return true;
    }
    const QString text = event->text();
    if (text.isEmpty())
        return false;
    const QString echo = m_actLocalEcho->isChecked() ? text : QString();
    sendBytes(text.toLocal8Bit(), echo);
    return true;
}

void MainWindow::loadSettings()
{
    m_loading = true;
    QSettings settings;
    if (settings.contains(QStringLiteral("geometry")))
        restoreGeometry(settings.value(QStringLiteral("geometry")).toByteArray());
    m_english = settings.value(QStringLiteral("english"), false).toBool();
    m_englishBox->setChecked(m_english);
    m_stayOnTop->setChecked(settings.value(QStringLiteral("stayOnTop"), false).toBool());
    applyStayOnTop(m_stayOnTop->isChecked());
    m_actAutoOpen->setChecked(settings.value(QStringLiteral("autoOpen"), false).toBool());
    const QString port = settings.value(QStringLiteral("port")).toString();
    if (!port.isEmpty()) {
        m_portCombo->addItem(port, port);
        m_portCombo->setCurrentIndex(0);
    }
    if (settings.contains(QStringLiteral("baud")))
        m_baudCombo->setCurrentText(settings.value(QStringLiteral("baud")).toString());
    m_dataBits->setCurrentText(settings.value(QStringLiteral("dataBits"), QStringLiteral("8")).toString());
    m_stopBits->setCurrentText(settings.value(QStringLiteral("stopBits"), QStringLiteral("1")).toString());
    m_dtr->setChecked(settings.value(QStringLiteral("dtr"), true).toBool());
    m_rts->setChecked(settings.value(QStringLiteral("rts"), false).toBool());
    m_hexDisp->setChecked(settings.value(QStringLiteral("hexDisp"), false).toBool());
    m_hexSend->setChecked(settings.value(QStringLiteral("hexSend"), false).toBool());
    m_newLine->setChecked(settings.value(QStringLiteral("newLine"), false).toBool());
    m_interval->setValue(settings.value(QStringLiteral("interval"), 100).toInt());
    m_packetMode->setChecked(settings.value(QStringLiteral("packet"), false).toBool());
    m_timeout->setValue(settings.value(QStringLiteral("timeout"), 20).toInt());
    m_csFrom->setValue(settings.value(QStringLiteral("csFrom"), 1).toInt());
    m_csTail->setValue(settings.value(QStringLiteral("csTail"), 0).toInt());
    m_send->setPlainText(settings.value(QStringLiteral("sendText")).toString());
    m_filePath->setText(settings.value(QStringLiteral("filePath")).toString());
    m_fileDelayMs = settings.value(QStringLiteral("fileDelay"), 0).toInt();
    m_bufferChars = settings.value(QStringLiteral("bufferChars"), 1000000).toInt();
    m_actLineMode->setChecked(settings.value(QStringLiteral("lineMode"), false).toBool());
    m_actKeySend->setChecked(settings.value(QStringLiteral("keySend"), false).toBool());
    m_actLocalEcho->setChecked(settings.value(QStringLiteral("localEcho"), false).toBool());
    m_actCrLf->setChecked(settings.value(QStringLiteral("crlf"), false).toBool());
    if (settings.contains(QStringLiteral("font"))) {
        QFont font;
        font.fromString(settings.value(QStringLiteral("font")).toString());
        m_receive->setFont(font);
    }
    if (settings.contains(QStringLiteral("bg")))
        m_recvBg = QColor(settings.value(QStringLiteral("bg")).toString());
    if (settings.contains(QStringLiteral("fg")))
        m_recvFg = QColor(settings.value(QStringLiteral("fg")).toString());
    applyRecvStyle();

    applyLanguage();
    m_parity->setCurrentIndex(settings.value(QStringLiteral("parity"), 0).toInt());
    m_flow->setCurrentIndex(settings.value(QStringLiteral("flow"), 0).toInt());
    m_checksum->setCurrentIndex(settings.value(QStringLiteral("checksum"), 0).toInt());
    for (QAction *action : m_bufferActs) {
        const int limits[] = {200000, 500000, 1000000, 2000000, 5000000};
        const int index = m_bufferActs.indexOf(action);
        if (index >= 0 && limits[index] == m_bufferChars)
            action->setChecked(true);
    }
    for (QAction *action : m_delayActs) {
        if (action->data().toInt() == m_fileDelayMs)
            action->setChecked(true);
    }
    const int count = qMin(kPresetCount, settings.beginReadArray(QStringLiteral("preset")));
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        m_rows[i].order->setValue(settings.value(QStringLiteral("order"), 0).toInt());
        const QString name = settings.value(QStringLiteral("name")).toString();
        if (!name.isEmpty())
            m_rows[i].button->setText(name);
        m_rows[i].hex->setChecked(settings.value(QStringLiteral("hex"), false).toBool());
        m_rows[i].text->setText(settings.value(QStringLiteral("text")).toString());
        m_rows[i].delayMs->setValue(settings.value(QStringLiteral("delay"), 100).toInt());
    }
    settings.endArray();
    if (settings.value(QStringLiteral("extension"), false).toBool())
        setExtensionVisible(true);
    m_loading = false;
}

void MainWindow::saveSettings()
{
    QSettings settings;
    settings.setValue(QStringLiteral("geometry"), saveGeometry());
    settings.setValue(QStringLiteral("english"), m_english);
    settings.setValue(QStringLiteral("stayOnTop"), m_stayOnTop->isChecked());
    settings.setValue(QStringLiteral("autoOpen"), m_actAutoOpen->isChecked());
    settings.setValue(QStringLiteral("extension"), m_extPanel->isVisible());
    settings.setValue(QStringLiteral("port"), currentPortName());
    settings.setValue(QStringLiteral("baud"), m_baudCombo->currentText());
    settings.setValue(QStringLiteral("dataBits"), m_dataBits->currentText());
    settings.setValue(QStringLiteral("parity"), m_parity->currentIndex());
    settings.setValue(QStringLiteral("stopBits"), m_stopBits->currentText());
    settings.setValue(QStringLiteral("flow"), m_flow->currentIndex());
    settings.setValue(QStringLiteral("dtr"), m_dtr->isChecked());
    settings.setValue(QStringLiteral("rts"), m_rts->isChecked());
    settings.setValue(QStringLiteral("hexDisp"), m_hexDisp->isChecked());
    settings.setValue(QStringLiteral("hexSend"), m_hexSend->isChecked());
    settings.setValue(QStringLiteral("newLine"), m_newLine->isChecked());
    settings.setValue(QStringLiteral("interval"), m_interval->value());
    settings.setValue(QStringLiteral("packet"), m_packetMode->isChecked());
    settings.setValue(QStringLiteral("timeout"), m_timeout->value());
    settings.setValue(QStringLiteral("checksum"), m_checksum->currentIndex());
    settings.setValue(QStringLiteral("csFrom"), m_csFrom->value());
    settings.setValue(QStringLiteral("csTail"), m_csTail->value());
    settings.setValue(QStringLiteral("sendText"), m_send->toPlainText());
    settings.setValue(QStringLiteral("filePath"), m_filePath->text());
    settings.setValue(QStringLiteral("fileDelay"), m_fileDelayMs);
    settings.setValue(QStringLiteral("bufferChars"), m_bufferChars);
    settings.setValue(QStringLiteral("lineMode"), m_actLineMode->isChecked());
    settings.setValue(QStringLiteral("keySend"), m_actKeySend->isChecked());
    settings.setValue(QStringLiteral("localEcho"), m_actLocalEcho->isChecked());
    settings.setValue(QStringLiteral("crlf"), m_actCrLf->isChecked());
    settings.setValue(QStringLiteral("font"), m_receive->font().toString());
    settings.setValue(QStringLiteral("bg"), m_recvBg.name());
    settings.setValue(QStringLiteral("fg"), m_recvFg.name());
    settings.beginWriteArray(QStringLiteral("preset"), m_rows.size());
    for (int i = 0; i < m_rows.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue(QStringLiteral("order"), m_rows[i].order->value());
        settings.setValue(QStringLiteral("name"), m_rows[i].button->text());
        settings.setValue(QStringLiteral("hex"), m_rows[i].hex->isChecked());
        settings.setValue(QStringLiteral("text"), m_rows[i].text->text());
        settings.setValue(QStringLiteral("delay"), m_rows[i].delayMs->value());
    }
    settings.endArray();
}

void MainWindow::importSscomIni()
{
    const QString path = QFileDialog::getOpenFileName(
        this, L(QStringLiteral("导入 sscom51.ini"), QStringLiteral("Import sscom51.ini")),
        QString(), QStringLiteral("INI (*.ini);;All (*.*)"));
    if (path.isEmpty())
        return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        statusBar()->showMessage(L(QStringLiteral("无法读取 ini"), QStringLiteral("Could not read the ini")));
        return;
    }
    const QString text = QString::fromLocal8Bit(file.readAll());
    m_loading = true;
    int presetHits = 0;
    QString ascDraft;
    QString hexDraft;
    bool sawAsc = false;
    bool sawHex = false;
    bool sawMode = false;
    bool hexMode = m_hexSend->isChecked();
    const QStringList lines = text.split(QRegularExpression(QStringLiteral("[\\r\\n]+")));
    for (QString line : lines) {
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char(';')) || !line.startsWith(QLatin1Char('N')))
            continue;
        const int eq = line.indexOf(QLatin1Char('='));
        if (eq <= 1)
            continue;
        bool ok = false;
        const int id = line.mid(1, eq - 1).toInt(&ok);
        if (!ok)
            continue;
        QString value = line.mid(eq + 1);
        if (id >= 101)
            if (value.startsWith(QLatin1Char(',')))
                value = value.mid(1);

        if (id >= 1 && id <= kPresetCount) {
            const int comma = value.indexOf(QLatin1Char(','));
            const bool hex = comma >= 0 && value.left(comma).trimmed().startsWith(QLatin1Char('H'), Qt::CaseInsensitive);
            const QString payload = comma >= 0 ? value.mid(comma + 1) : QString();
            m_rows[id - 1].hex->setChecked(hex);
            m_rows[id - 1].text->setText(payload);
            ++presetHits;
        } else if (id >= 101 && id <= 100 + kPresetCount) {
            const int row = id - 101;
            const int first = value.indexOf(QLatin1Char(','));
            const int last = value.lastIndexOf(QLatin1Char(','));
            if (first <= 0 || last <= first)
                continue;
            m_rows[row].order->setValue(value.left(first).toInt());
            m_rows[row].button->setText(value.mid(first + 1, last - first - 1));
            m_rows[row].delayMs->setValue(value.mid(last + 1).toInt());
        } else if (id == 1053) {
            ascDraft = value;
            sawAsc = true;
        } else if (id == 1054) {
            hexDraft = value;
            sawHex = true;
        } else if (id == 1055) {
            hexMode = value.trimmed().startsWith(QLatin1Char('H'), Qt::CaseInsensitive);
            sawMode = true;
        } else if (id == 1056) {
            m_interval->setValue(qMax(1, value.toInt()));
        } else if (id == 1057) {
            m_newLine->setChecked(yn(value));
        } else if (id == 1059) {
            m_hexDisp->setChecked(yn(value));
        } else if (id == 1060) {
            m_checksum->setCurrentIndex(qBound(0, value.toInt(), 3));
        } else if (id == 1061) {
            m_dtr->setChecked(yn(value));
        } else if (id == 1062) {
            m_rts->setChecked(yn(value));
        } else if (id == 1063) {
            m_actAutoOpen->setChecked(yn(value));
        } else if (id == 1064) {
            m_packetMode->setChecked(yn(value));
        } else if (id == 1065) {
            m_timeout->setValue(qMax(1, value.toInt()));
        } else if (id == 1067) {
            const int n = value.toInt();
            if (n >= 10000)
                setBufferLimit(n);
        } else if (id == 1071) {
            m_actLineMode->setChecked(yn(value));
        } else if (id == 1072) {
            m_actKeySend->setChecked(yn(value));
        } else if (id == 1073) {
            m_actLocalEcho->setChecked(yn(value));
        } else if (id == 1074) {
            m_csFrom->setValue(qMax(1, value.toInt()));
        } else if (id == 1075) {
            m_csTail->setValue(qMax(0, value.toInt()));
        } else if (id == 1076) {
            m_actCrLf->setChecked(yn(value));
        } else if (id == 1080 && !value.trimmed().isEmpty() && !value.startsWith(QLatin1Char('1'))
                   && !value.startsWith(QLatin1Char('2')) && !value.startsWith(QLatin1Char('3'))) {
            const QString port = value.trimmed();
            int index = m_portCombo->findData(port);
            if (index < 0) {
                m_portCombo->addItem(port, port);
                index = m_portCombo->count() - 1;
            }
            m_portCombo->setCurrentIndex(index);
        } else if (id == 1081 && value.toInt() > 0) {
            m_baudCombo->setCurrentText(QString::number(value.toInt()));
        } else if (id == 1087) {
            const int width = value.toInt();
            if (width >= 40 && width <= 400) {
                for (PresetRow &row : m_rows)
                    row.button->setFixedWidth(width);
            }
        } else if (id == 1100) {
            const bool english = value.contains(QStringLiteral("English"), Qt::CaseInsensitive)
                                 || value.trimmed().startsWith(QLatin1Char('E'));
            m_english = english;
            m_englishBox->setChecked(english);
        } else if (id == 1051) {
            const int delay = value.toInt();
            int best = 0;
            int bestDelta = 100000;
            for (int candidate : {0, 1, 10, 50, 100}) {
                const int delta = qAbs(candidate - delay);
                if (delta < bestDelta) {
                    bestDelta = delta;
                    best = candidate;
                }
            }
            setFileDelay(best);
            for (QAction *action : m_delayActs)
                if (action->data().toInt() == best)
                    action->setChecked(true);
        }
    }
    if (sawMode)
        m_hexSend->setChecked(hexMode);
    if (m_hexSend->isChecked()) {
        if (sawHex)
            m_send->setPlainText(hexDraft);
    } else if (sawAsc) {
        m_send->setPlainText(ascDraft);
    }
    applyLanguage();
    m_loading = false;
    updateStatus();
    statusBar()->showMessage(L(QStringLiteral("已导入 %1 条多字符串"), QStringLiteral("Imported %1 multi-strings"))
                                 .arg(presetHits));
}
