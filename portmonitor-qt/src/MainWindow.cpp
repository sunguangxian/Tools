#include "MainWindow.h"

#include "PcapParser.h"
#include "Style.h"

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableView>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextEdit>
#include <QTimer>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

constexpr int kKindRole = Qt::UserRole;
constexpr int kNameRole = Qt::UserRole + 1;
constexpr int kStoreCap = 1024;

QString formatUs(qint64 unixUs)
{
    return QDateTime::fromMSecsSinceEpoch(unixUs / 1000, Qt::LocalTime).toString(QStringLiteral("HH:mm:ss.zzz"));
}

QByteArray parseHex(const QString &text, QString *error)
{
    QString compact = text;
    compact.remove(QRegularExpression(QStringLiteral("[\\s,]+")));
    if (compact.isEmpty()) {
        *error = QStringLiteral("没有要发送的数据");
        return {};
    }
    if (compact.size() % 2) {
        *error = QStringLiteral("HEX 需要偶数个字符");
        return {};
    }
    QByteArray out;
    out.reserve(compact.size() / 2);
    for (int i = 0; i < compact.size(); i += 2) {
        bool ok = false;
        const int value = compact.mid(i, 2).toInt(&ok, 16);
        if (!ok) {
            *error = QStringLiteral("HEX 含有非法字符");
            return {};
        }
        out.append(char(value));
    }
    return out;
}

int comNumber(const QString &name)
{
    if (name.startsWith(QLatin1String("COM"), Qt::CaseInsensitive))
        return name.mid(3).toInt();
    return 0;
}

#ifdef Q_OS_WIN
bool isWindowsAdmin()
{
    BOOL member = FALSE;
    SID_IDENTIFIER_AUTHORITY nt = SECURITY_NT_AUTHORITY;
    PSID sid = nullptr;
    if (!AllocateAndInitializeSid(&nt, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &sid))
        return false;
    CheckTokenMembership(nullptr, sid, &member);
    FreeSid(sid);
    return member == TRUE;
}
#endif

QString usbLiveBlocker(const QString &cmdPath)
{
#ifndef Q_OS_WIN
    Q_UNUSED(cmdPath);
    return QStringLiteral("实时 USB 抓包仅支持 Windows（需安装 USBPcap）。可以打开 pcap 文件做离线分析。");
#else
    if (!QFileInfo::exists(cmdPath)) {
        return QStringLiteral("未找到 USBPcapCMD.exe：%1\n请安装 USBPcap（https://desowin.org/usbpcap/）后重启，或修改路径。")
            .arg(cmdPath);
    }
    if (!isWindowsAdmin())
        return QStringLiteral("USBPcap 需要管理员权限。请右键本程序，选择“以管理员身份运行”。");
    return {};
#endif
}

QTreeWidgetItem *addLeaf(QTreeWidgetItem *parent, const QString &kind, const QString &name, const QString &label)
{
    auto *item = new QTreeWidgetItem(parent, {label});
    item->setData(0, kKindRole, kind);
    item->setData(0, kNameRole, name);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
    item->setCheckState(0, Qt::Unchecked);
    return item;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("PortMonitor — 串口 / USB / SCSI"));
    resize(1440, 820);
    m_mono = QFont(QStringLiteral("Consolas"));
    m_mono.setStyleHint(QFont::Monospace);
    m_mono.setPointSize(10);

    m_model = new CaptureModel(this);
    m_scsi = new ScsiModel(this);
    m_proxy = new CaptureFilter(this);
    m_scsiProxy = new ScsiFilter(this);
    m_proxy->state = &m_filter;
    m_proxy->rows = m_model;
    m_proxy->setSourceModel(m_model);
    m_scsiProxy->state = &m_filter;
    m_scsiProxy->rows = m_scsi;
    m_scsiProxy->setSourceModel(m_scsi);

    buildUi();
    setStyleSheet(portMonitorStyle());

    m_textTimer = new QTimer(this);
    m_textTimer->setSingleShot(true);
    connect(m_textTimer, &QTimer::timeout, this, &MainWindow::rebuildTextViews);
    m_rateTimer = new QTimer(this);
    connect(m_rateTimer, &QTimer::timeout, this, &MainWindow::refreshStats);
    m_rateTimer->start(500);
    m_rateClock.start();

    refreshSerialPorts();
    refreshUsbInterfaces();
    refreshStats();
    statusBar()->showMessage(QStringLiteral("勾选串口或 USB 接口后点「开始监视」。无需注册或登录。"));
}

MainWindow::~MainWindow()
{
    stopMonitor();
}

void MainWindow::buildUi()
{
    auto *file = menuBar()->addMenu(QStringLiteral("文件(&F)"));
    file->addAction(QStringLiteral("打开 pcap…"), this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("打开 pcap"), {},
                                                           QStringLiteral("抓包 (*.pcap *.pcapng);;所有文件 (*.*)"));
        if (!path.isEmpty())
            openPcap(path);
    });
    file->addAction(QStringLiteral("导出数据 CSV…"), this, &MainWindow::exportCaptureCsv);
    file->addAction(QStringLiteral("导出 SCSI CSV…"), this, &MainWindow::exportScsiCsv);
    file->addSeparator();
    file->addAction(QStringLiteral("退出"), this, &QWidget::close);

    auto *mon = menuBar()->addMenu(QStringLiteral("监视(&M)"));
    mon->addAction(QStringLiteral("开始"), this, &MainWindow::startMonitor);
    mon->addAction(QStringLiteral("停止"), this, &MainWindow::stopMonitor);
    mon->addAction(QStringLiteral("暂停 / 继续"), this, &MainWindow::togglePause);
    mon->addAction(QStringLiteral("清空"), this, &MainWindow::clearCapture);
    mon->addSeparator();
    mon->addAction(QStringLiteral("刷新设备"), this, [this] {
        refreshSerialPorts();
        refreshUsbInterfaces();
    });

    auto *view = menuBar()->addMenu(QStringLiteral("视图(&V)"));
    view->addAction(QStringLiteral("数据表"), this, [this] { m_tabs->setCurrentIndex(0); });
    view->addAction(QStringLiteral("行视图"), this, [this] { m_tabs->setCurrentIndex(1); });
    view->addAction(QStringLiteral("终端"), this, [this] { m_tabs->setCurrentIndex(2); });
    view->addAction(QStringLiteral("转储"), this, [this] { m_tabs->setCurrentIndex(3); });
    view->addAction(QStringLiteral("SCSI"), this, [this] { m_tabs->setCurrentIndex(4); });
    menuBar()->addMenu(QStringLiteral("帮助(&H)"))->addAction(QStringLiteral("关于"), this, &MainWindow::showAbout);

    auto *bar = addToolBar(QStringLiteral("主工具栏"));
    bar->setMovable(false);
    m_btnStart = new QPushButton(QStringLiteral("开始监视"), bar);
    m_btnStart->setObjectName(QStringLiteral("startButton"));
    m_btnStop = new QPushButton(QStringLiteral("停止"), bar);
    auto *btnPause = new QPushButton(QStringLiteral("暂停"), bar);
    auto *btnClear = new QPushButton(QStringLiteral("清空"), bar);
    auto *btnExport = new QPushButton(QStringLiteral("导出"), bar);
    auto *btnPcap = new QPushButton(QStringLiteral("打开 pcap"), bar);
    bar->addWidget(m_btnStart);
    bar->addWidget(m_btnStop);
    bar->addWidget(btnPause);
    bar->addWidget(btnClear);
    bar->addWidget(btnExport);
    bar->addWidget(btnPcap);
    connect(m_btnStart, &QPushButton::clicked, this, &MainWindow::startMonitor);
    connect(m_btnStop, &QPushButton::clicked, this, &MainWindow::stopMonitor);
    connect(btnPause, &QPushButton::clicked, this, &MainWindow::togglePause);
    connect(btnClear, &QPushButton::clicked, this, &MainWindow::clearCapture);
    connect(btnExport, &QPushButton::clicked, this, &MainWindow::exportCaptureCsv);
    connect(btnPcap, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("打开 pcap"), {},
                                                           QStringLiteral("抓包 (*.pcap *.pcapng);;所有文件 (*.*)"));
        if (!path.isEmpty())
            openPcap(path);
    });
    m_btnStop->setEnabled(false);

    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *split = new QSplitter(Qt::Horizontal, central);
    root->addWidget(split, 1);

    auto *left = new QWidget(split);
    auto *leftLay = new QVBoxLayout(left);
    leftLay->setContentsMargins(8, 8, 8, 8);
    leftLay->setSpacing(6);
    auto *section = new QLabel(QStringLiteral("设备"), left);
    section->setObjectName(QStringLiteral("sectionLabel"));
    leftLay->addWidget(section);
    m_tree = new QTreeWidget(left);
    m_tree->setHeaderHidden(true);
    m_tree->setIndentation(16);
    m_serialRoot = new QTreeWidgetItem(m_tree, {QStringLiteral("串口")});
    m_usbRoot = new QTreeWidgetItem(m_tree, {QStringLiteral("USB")});
    m_devRoot = new QTreeWidgetItem(m_tree, {QStringLiteral("USB 设备")});
    for (QTreeWidgetItem *group : {m_serialRoot, m_usbRoot, m_devRoot}) {
        group->setFlags(Qt::ItemIsEnabled);
        group->setExpanded(true);
    }
    leftLay->addWidget(m_tree, 1);

    auto *serialBox = new QGroupBox(QStringLiteral("串口参数"), left);
    auto *serialForm = new QFormLayout(serialBox);
    m_baud = new QComboBox(serialBox);
    for (int baud : {1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600, 1000000})
        m_baud->addItem(QString::number(baud), baud);
    m_baud->setCurrentText(QStringLiteral("115200"));
    m_dataBits = new QComboBox(serialBox);
    for (int bits : {8, 7, 6, 5})
        m_dataBits->addItem(QString::number(bits), bits);
    m_parity = new QComboBox(serialBox);
    m_parity->addItem(QStringLiteral("无"), int(QSerialPort::NoParity));
    m_parity->addItem(QStringLiteral("偶"), int(QSerialPort::EvenParity));
    m_parity->addItem(QStringLiteral("奇"), int(QSerialPort::OddParity));
    m_parity->addItem(QStringLiteral("标记"), int(QSerialPort::MarkParity));
    m_parity->addItem(QStringLiteral("空格"), int(QSerialPort::SpaceParity));
    m_stopBits = new QComboBox(serialBox);
    m_stopBits->addItem(QStringLiteral("1"), int(QSerialPort::OneStop));
    m_stopBits->addItem(QStringLiteral("1.5"), int(QSerialPort::OneAndHalfStop));
    m_stopBits->addItem(QStringLiteral("2"), int(QSerialPort::TwoStop));
    auto *lineBox = new QWidget(serialBox);
    auto *lineLay = new QHBoxLayout(lineBox);
    lineLay->setContentsMargins(0, 0, 0, 0);
    lineLay->addWidget(m_dataBits);
    lineLay->addWidget(m_parity);
    lineLay->addWidget(m_stopBits);
    serialForm->addRow(QStringLiteral("波特率"), m_baud);
    serialForm->addRow(QStringLiteral("帧"), lineBox);
    auto *btnRefresh = new QPushButton(QStringLiteral("刷新端口"), serialBox);
    serialForm->addRow(btnRefresh);
    connect(btnRefresh, &QPushButton::clicked, this, &MainWindow::refreshSerialPorts);
    leftLay->addWidget(serialBox);

    auto *sendBox = new QGroupBox(QStringLiteral("向已打开的串口发送"), left);
    auto *sendLay = new QVBoxLayout(sendBox);
    m_sendEdit = new QLineEdit(sendBox);
    m_sendEdit->setPlaceholderText(QStringLiteral("HEX：01 03 00 00 00 01"));
    m_sendHex = new QCheckBox(QStringLiteral("HEX"), sendBox);
    m_sendHex->setChecked(true);
    auto *btnSend = new QPushButton(QStringLiteral("发送"), sendBox);
    auto *sendRow = new QHBoxLayout;
    sendRow->addWidget(m_sendHex);
    sendRow->addWidget(btnSend);
    sendLay->addWidget(m_sendEdit);
    sendLay->addLayout(sendRow);
    connect(btnSend, &QPushButton::clicked, this, &MainWindow::sendToPort);
    connect(m_sendEdit, &QLineEdit::returnPressed, this, &MainWindow::sendToPort);
    leftLay->addWidget(sendBox);

    auto *usbBox = new QGroupBox(QStringLiteral("USBPcap"), left);
    auto *usbForm = new QFormLayout(usbBox);
    m_usbCmd = new QLineEdit(QStringLiteral("C:/Program Files/USBPcap/USBPcapCMD.exe"), usbBox);
    m_usbAddr = new QLineEdit(usbBox);
    m_usbAddr->setPlaceholderText(QStringLiteral("空 = 该接口下全部设备"));
    auto *usbPathRow = new QWidget(usbBox);
    auto *usbPathLay = new QHBoxLayout(usbPathRow);
    usbPathLay->setContentsMargins(0, 0, 0, 0);
    auto *btnBrowse = new QPushButton(QStringLiteral("…"), usbPathRow);
    btnBrowse->setFixedWidth(28);
    usbPathLay->addWidget(m_usbCmd);
    usbPathLay->addWidget(btnBrowse);
    auto *btnUsbRefresh = new QPushButton(QStringLiteral("刷新 USB 接口"), usbBox);
    usbForm->addRow(QStringLiteral("命令"), usbPathRow);
    usbForm->addRow(QStringLiteral("设备地址"), m_usbAddr);
    usbForm->addRow(btnUsbRefresh);
    connect(btnBrowse, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("USBPcapCMD.exe"), m_usbCmd->text(),
                                                           QStringLiteral("程序 (*.exe)"));
        if (!path.isEmpty())
            m_usbCmd->setText(path);
    });
    connect(btnUsbRefresh, &QPushButton::clicked, this, &MainWindow::refreshUsbInterfaces);
    leftLay->addWidget(usbBox);

    auto *hint = new QLabel(QStringLiteral("串口为独占打开。端口被其它程序占用时无法监视。USB 实时抓包依赖本机 USBPcap，且需要管理员权限。无注册、无登录。"), left);
    hint->setObjectName(QStringLiteral("hintLabel"));
    hint->setWordWrap(true);
    leftLay->addWidget(hint);
    left->setMinimumWidth(280);
    left->setMaximumWidth(420);

    auto *right = new QWidget(split);
    auto *rightLay = new QVBoxLayout(right);
    rightLay->setContentsMargins(0, 0, 0, 0);
    rightLay->setSpacing(0);

    auto *filter = new QWidget(right);
    auto *filterLay = new QHBoxLayout(filter);
    filterLay->setContentsMargins(8, 6, 8, 6);
    filterLay->addWidget(new QLabel(QStringLiteral("来源"), filter));
    m_sourceFilter = new QComboBox(filter);
    m_sourceFilter->addItem(QStringLiteral("全部"), 0);
    m_sourceFilter->addItem(QStringLiteral("串口"), 1);
    m_sourceFilter->addItem(QStringLiteral("USB"), 2);
    filterLay->addWidget(m_sourceFilter);
    filterLay->addWidget(new QLabel(QStringLiteral("端口"), filter));
    m_portFilter = new QComboBox(filter);
    m_portFilter->addItem(QStringLiteral("全部"));
    m_portFilter->setMinimumWidth(120);
    filterLay->addWidget(m_portFilter);
    m_search = new QLineEdit(filter);
    m_search->setPlaceholderText(QStringLiteral("搜索 HEX / 文本 / 摘要"));
    m_search->setClearButtonEnabled(true);
    filterLay->addWidget(m_search, 1);
    m_autoScroll = new QCheckBox(QStringLiteral("自动滚动"), filter);
    m_autoScroll->setChecked(true);
    filterLay->addWidget(m_autoScroll);
    rightLay->addWidget(filter);

    m_error = new QLabel(right);
    m_error->setObjectName(QStringLiteral("errorLabel"));
    m_error->setWordWrap(true);
    m_error->setVisible(false);
    m_error->setContentsMargins(8, 0, 8, 4);
    rightLay->addWidget(m_error);

    auto *vertical = new QSplitter(Qt::Vertical, right);
    rightLay->addWidget(vertical, 1);
    m_tabs = new QTabWidget(vertical);
    m_table = new QTableView(m_tabs);
    m_table->setModel(m_proxy);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setWordWrap(false);
    m_table->setShowGrid(false);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(22);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setColumnWidth(0, 68);
    m_table->setColumnWidth(1, 110);
    m_table->setColumnWidth(2, 88);
    m_table->setColumnWidth(3, 56);
    m_table->setColumnWidth(4, 64);
    m_table->setColumnWidth(5, 280);
    m_table->setColumnWidth(6, 140);
    m_tabs->addTab(m_table, QStringLiteral("数据表"));

    m_line = new QPlainTextEdit(m_tabs);
    m_line->setReadOnly(true);
    m_line->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_line->setFont(m_mono);
    m_tabs->addTab(m_line, QStringLiteral("行视图"));

    m_terminal = new QTextEdit(m_tabs);
    m_terminal->setReadOnly(true);
    m_terminal->setFont(m_mono);
    m_tabs->addTab(m_terminal, QStringLiteral("终端"));

    m_dump = new QPlainTextEdit(m_tabs);
    m_dump->setReadOnly(true);
    m_dump->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_dump->setFont(m_mono);
    m_tabs->addTab(m_dump, QStringLiteral("转储"));

    m_scsiTable = new QTableView(m_tabs);
    m_scsiTable->setModel(m_scsiProxy);
    m_scsiTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_scsiTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_scsiTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_scsiTable->setAlternatingRowColors(true);
    m_scsiTable->setWordWrap(false);
    m_scsiTable->setShowGrid(false);
    m_scsiTable->verticalHeader()->setVisible(false);
    m_scsiTable->verticalHeader()->setDefaultSectionSize(22);
    m_scsiTable->horizontalHeader()->setStretchLastSection(true);
    m_scsiTable->setColumnWidth(0, 56);
    m_scsiTable->setColumnWidth(1, 110);
    m_scsiTable->setColumnWidth(2, 64);
    m_scsiTable->setColumnWidth(3, 90);
    m_scsiTable->setColumnWidth(5, 180);
    m_tabs->addTab(m_scsiTable, QStringLiteral("SCSI"));
    vertical->addWidget(m_tabs);

    auto *detail = new QWidget(vertical);
    auto *detailLay = new QHBoxLayout(detail);
    detailLay->setContentsMargins(8, 6, 8, 6);
    m_hex = new QPlainTextEdit(detail);
    m_hex->setReadOnly(true);
    m_hex->setFont(m_mono);
    m_hex->setPlaceholderText(QStringLiteral("HEX"));
    auto *side = new QVBoxLayout;
    m_meta = new QLabel(QStringLiteral("选中一行查看详情"), detail);
    m_meta->setWordWrap(true);
    m_ascii = new QPlainTextEdit(detail);
    m_ascii->setReadOnly(true);
    m_ascii->setFont(m_mono);
    m_ascii->setPlaceholderText(QStringLiteral("ASCII"));
    side->addWidget(m_meta);
    side->addWidget(m_ascii, 1);
    detailLay->addWidget(m_hex, 2);
    detailLay->addLayout(side, 1);
    vertical->addWidget(detail);
    vertical->setStretchFactor(0, 3);
    vertical->setStretchFactor(1, 1);
    split->addWidget(left);
    split->addWidget(right);
    split->setStretchFactor(1, 1);
    split->setSizes({320, 1100});

    const auto applyFilter = [this] {
        m_filter.source = m_sourceFilter->currentData().toInt();
        m_filter.port = m_portFilter->currentIndex() <= 0 ? QString() : m_portFilter->currentText();
        m_filter.text = m_search->text().trimmed();
        m_proxy->invalidate();
        m_scsiProxy->invalidate();
        m_textTimer->start(80);
    };
    connect(m_sourceFilter, &QComboBox::currentIndexChanged, this, applyFilter);
    connect(m_portFilter, &QComboBox::currentIndexChanged, this, applyFilter);
    connect(m_search, &QLineEdit::textChanged, this, applyFilter);
    connect(m_table->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex &current) {
        if (!current.isValid())
            return;
        const QModelIndex src = m_proxy->mapToSource(current);
        showCaptureDetail(m_model->at(src.row()));
    });
    connect(m_scsiTable->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex &current) {
        if (!current.isValid())
            return;
        const QModelIndex src = m_scsiProxy->mapToSource(current);
        showScsiDetail(m_scsi->at(src.row()));
    });

    m_sbState = new QLabel(QStringLiteral("空闲"), this);
    m_sbPkts = new QLabel(this);
    m_sbBytes = new QLabel(this);
    m_sbRead = new QLabel(this);
    m_sbWrite = new QLabel(this);
    m_sbRate = new QLabel(this);
    for (QLabel *label : {m_sbState, m_sbPkts, m_sbBytes, m_sbRead, m_sbWrite, m_sbRate})
        statusBar()->addWidget(label);
}

void MainWindow::refreshSerialPorts()
{
    QSet<QString> checked;
    for (const QString &name : checkedNames(QStringLiteral("com")))
        checked.insert(name);
    const QString selected = selectedCom();
    qDeleteAll(m_serialRoot->takeChildren());
    QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    std::sort(ports.begin(), ports.end(), [](const QSerialPortInfo &a, const QSerialPortInfo &b) {
        return comNumber(a.portName()) < comNumber(b.portName());
    });
    for (const QSerialPortInfo &info : ports) {
        const QString name = info.portName();
        QString label = name;
        if (!info.description().isEmpty())
            label += QStringLiteral("  ") + info.description();
        auto *item = addLeaf(m_serialRoot, QStringLiteral("com"), name, label);
        if (checked.contains(name) || name == selected)
            item->setCheckState(0, Qt::Checked);
    }
    m_serialRoot->setExpanded(true);
    if (ports.isEmpty()) {
        auto *empty = new QTreeWidgetItem(m_serialRoot, {QStringLiteral("（无串口）")});
        empty->setFlags(Qt::ItemIsEnabled);
    }
}

void MainWindow::refreshUsbInterfaces()
{
    QSet<QString> checked;
    for (const QString &name : checkedNames(QStringLiteral("usb")))
        checked.insert(name);
    const QString cmd = m_usbCmd->text().trimmed();
    const auto apply = [this, checked](const QStringList &ifaces) {
        qDeleteAll(m_usbRoot->takeChildren());
        QStringList list = ifaces;
        if (list.isEmpty()) {
            for (int i = 1; i <= 8; ++i)
                list.append(QStringLiteral("\\\\.\\USBPcap%1").arg(i));
        }
        for (const QString &iface : list) {
            auto *item = addLeaf(m_usbRoot, QStringLiteral("usb"), iface, iface);
            if (checked.contains(iface))
                item->setCheckState(0, Qt::Checked);
        }
        m_usbRoot->setExpanded(true);
    };

    if (!QFileInfo::exists(cmd)) {
        apply({});
        return;
    }
    auto *proc = new QProcess(this);
    connect(proc, &QProcess::finished, this, [this, proc, apply](int, QProcess::ExitStatus) {
        const QString text = QString::fromLocal8Bit(proc->readAllStandardOutput());
        QStringList ifaces;
        static const QRegularExpression re(QStringLiteral(R"(value=(\\+\.?\\USBPcap\d+))"));
        auto it = re.globalMatch(text);
        while (it.hasNext()) {
            const QString value = it.next().captured(1);
            if (!ifaces.contains(value))
                ifaces.append(value);
        }
        apply(ifaces);
        proc->deleteLater();
    });
    proc->start(cmd, {QStringLiteral("--extcap-interfaces")});
    if (!proc->waitForStarted(1500))
        apply({});
}

void MainWindow::startMonitor()
{
    if (m_running)
        return;
    showError({});
    const QStringList coms = checkedNames(QStringLiteral("com"));
    const QStringList usbs = checkedNames(QStringLiteral("usb"));
    if (coms.isEmpty() && usbs.isEmpty()) {
        showError(QStringLiteral("请先在左侧勾选要监视的串口或 USB 接口。"));
        return;
    }

    int opened = 0;
    for (const QString &name : coms) {
        auto *port = new QSerialPort(this);
        port->setPortName(name);
        port->setBaudRate(m_baud->currentData().toInt());
        port->setDataBits(QSerialPort::DataBits(m_dataBits->currentData().toInt()));
        port->setParity(QSerialPort::Parity(m_parity->currentData().toInt()));
        port->setStopBits(QSerialPort::StopBits(m_stopBits->currentData().toInt()));
        port->setFlowControl(QSerialPort::NoFlowControl);
        if (!port->open(QIODevice::ReadWrite)) {
            showError(QStringLiteral("无法打开 %1：%2。用户态监视不能旁路其它程序已经打开的串口。").arg(name, port->errorString()));
            port->deleteLater();
            continue;
        }
        m_portNames.insert(port, name);
        connect(port, &QSerialPort::readyRead, this, [this, port] {
            if (!m_portNames.contains(port))
                return;
            const QByteArray data = port->readAll();
            if (data.isEmpty() || m_paused)
                return;
            addSerial(m_portNames.value(port), false, data);
        });
        connect(port, &QSerialPort::errorOccurred, this, [this, port](QSerialPort::SerialPortError error) {
            if (m_stopping || error == QSerialPort::NoError || !m_portNames.contains(port))
                return;
            if (error == QSerialPort::ResourceError)
                showError(QStringLiteral("%1 已断开：%2").arg(m_portNames.value(port), port->errorString()));
        });
        ++opened;
    }

    int usbStarted = 0;
    if (!usbs.isEmpty()) {
        const QString blocker = usbLiveBlocker(m_usbCmd->text().trimmed());
        if (!blocker.isEmpty()) {
            showError(blocker);
        } else {
            const QString addr = m_usbAddr->text().trimmed();
            bool addrOk = true;
            if (!addr.isEmpty()) {
                bool parsed = false;
                const int n = addr.toInt(&parsed);
                addrOk = parsed && n >= 0;
            }
            if (!addrOk) {
                showError(QStringLiteral("USB 设备地址需要是非负整数，或留空以监视该接口下的全部设备。"));
            } else {
                m_scsiDecoder.reset();
                for (const QString &iface : usbs) {
                    auto *proc = new QProcess(this);
                    proc->setProcessChannelMode(QProcess::SeparateChannels);
                    auto *parser = new PcapParser;
                    m_usbParsers.insert(proc, parser);
                    connect(proc, &QProcess::readyReadStandardOutput, this, [this, proc] {
                        PcapParser *parser = m_usbParsers.value(proc);
                        if (!parser)
                            return;
                        const QList<PcapRecord> recs = parser->feed(proc->readAllStandardOutput());
                        if (parser->hasError())
                            showError(parser->error());
                        for (const PcapRecord &rec : recs) {
                            if (rec.linkType != 249)
                                continue;
                            const auto packet = parseUsbPcapFrame(rec.data, rec.unixUs, ++m_seq);
                            if (!packet || m_paused)
                                continue;
                            handleUsbPacket(*packet);
                        }
                    });
                    connect(proc, &QProcess::readyReadStandardError, this, [this, proc] {
                        const QString err = QString::fromLocal8Bit(proc->readAllStandardError()).trimmed();
                        if (!err.isEmpty())
                            showError(QStringLiteral("USBPcap：") + err);
                    });
                    QStringList args{QStringLiteral("-d"), iface, QStringLiteral("-o"), QStringLiteral("-")};
                    if (addr.isEmpty())
                        args << QStringLiteral("-A");
                    else
                        args << QStringLiteral("--devices") << addr;
                    proc->start(m_usbCmd->text().trimmed(), args);
                    if (!proc->waitForStarted(2000)) {
                        showError(QStringLiteral("无法启动 USBPcapCMD：%1").arg(iface));
                        m_usbParsers.remove(proc);
                        delete parser;
                        proc->deleteLater();
                        continue;
                    }
                    ++usbStarted;
                }
            }
        }
    }

    if (opened == 0 && usbStarted == 0) {
        stopMonitor();
        if (m_error->text().isEmpty())
            showError(QStringLiteral("没有成功打开任何串口或 USB 接口。"));
        return;
    }
    setRunning(true);
    QStringList names = coms;
    names.append(usbs);
    statusBar()->showMessage(QStringLiteral("正在监视 %1").arg(names.join(QStringLiteral("，"))));
}

void MainWindow::stopMonitor()
{
    m_stopping = true;
    const auto ports = m_portNames.keys();
    for (QSerialPort *port : ports) {
        m_portNames.remove(port);
        port->close();
        port->deleteLater();
    }
    const auto procs = m_usbParsers.keys();
    for (QProcess *proc : procs) {
        delete m_usbParsers.take(proc);
        proc->kill();
        proc->deleteLater();
    }
    m_stopping = false;
    setRunning(false);
}

void MainWindow::clearCapture()
{
    m_model->clear();
    m_scsi->clear();
    m_scsiDecoder.reset();
    m_line->clear();
    m_terminal->clear();
    m_dump->clear();
    m_hex->clear();
    m_ascii->clear();
    m_meta->setText(QStringLiteral("选中一行查看详情"));
    m_seq = 0;
    m_packets = 0;
    m_bytes = 0;
    m_reads = 0;
    m_writes = 0;
    m_pktsMark = 0;
    m_rateClock.restart();
    m_seenPorts.clear();
    m_portFilter->blockSignals(true);
    m_portFilter->clear();
    m_portFilter->addItem(QStringLiteral("全部"));
    m_portFilter->blockSignals(false);
    m_filter.port.clear();
    qDeleteAll(m_devRoot->takeChildren());
    m_seenDevices.clear();
    refreshStats();
}

void MainWindow::exportCaptureCsv()
{
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("导出数据 CSV"),
                                                       QStringLiteral("portmonitor.csv"), QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty())
        return;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        showError(QStringLiteral("无法写入 %1").arg(path));
        return;
    }
    file.write("\xEF\xBB\xBF");
    file.write("seq,time,port,direction,length,hex,ascii,summary\n");
    for (int i = 0; i < m_model->rowCount(); ++i) {
        const CaptureRow *row = m_model->at(i);
        if (!row)
            continue;
        const QString hex = QString::fromLatin1(row->data.toHex(' ').toUpper());
        const QString line = QStringList{csvField(QString::number(row->seq)), csvField(row->timeText), csvField(row->port),
                                          csvField(row->direction), csvField(QString::number(row->length)), csvField(hex),
                                          csvField(row->asciiPreview), csvField(row->summary)}
                                  .join(QLatin1Char(','))
                              + QLatin1Char('\n');
        file.write(line.toUtf8());
    }
    statusBar()->showMessage(QStringLiteral("已导出 %1").arg(path));
}

void MainWindow::exportScsiCsv()
{
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("导出 SCSI CSV"),
                                                       QStringLiteral("scsi.csv"), QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty())
        return;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        showError(QStringLiteral("无法写入 %1").arg(path));
        return;
    }
    file.write("\xEF\xBB\xBF");
    file.write("seq,time,device,tag,lun,command,lba,blocks,status,duration_ms,summary\n");
    for (int i = 0; i < m_scsi->rowCount(); ++i) {
        const ScsiRow *row = m_scsi->at(i);
        if (!row)
            continue;
        const QString line = QStringList{csvField(QString::number(row->seq)), csvField(row->timeText), csvField(row->device),
                                          csvField(row->tag), csvField(row->lun), csvField(row->name), csvField(row->lba),
                                          csvField(row->blocks), csvField(row->status), csvField(row->duration),
                                          csvField(row->summary)}
                                  .join(QLatin1Char(','))
                              + QLatin1Char('\n');
        file.write(line.toUtf8());
    }
    statusBar()->showMessage(QStringLiteral("已导出 %1").arg(path));
}

void MainWindow::sendToPort()
{
    const QString name = selectedCom();
    QSerialPort *port = nullptr;
    for (auto it = m_portNames.cbegin(); it != m_portNames.cend(); ++it) {
        if (it.value() == name || (name.isEmpty() && m_portNames.size() == 1))
            port = it.key();
    }
    if (!port && m_portNames.size() == 1)
        port = m_portNames.constBegin().key();
    if (!port) {
        showError(QStringLiteral("请在左侧选中一个已经打开的串口，再发送。"));
        return;
    }
    QString error;
    const QByteArray payload = m_sendHex->isChecked() ? parseHex(m_sendEdit->text(), &error)
                                                       : m_sendEdit->text().toUtf8();
    if (m_sendHex->isChecked() && !error.isEmpty()) {
        showError(error);
        return;
    }
    if (payload.isEmpty()) {
        showError(QStringLiteral("没有要发送的数据"));
        return;
    }
    const qint64 n = port->write(payload);
    if (n < 0) {
        showError(QStringLiteral("发送失败：%1").arg(port->errorString()));
        return;
    }
    addSerial(m_portNames.value(port), true, payload.left(int(n)));
    showError({});
}

void MainWindow::showAbout()
{
    QMessageBox::information(
        this, QStringLiteral("关于 PortMonitor"),
        QStringLiteral("PortMonitor 监视串口、USB 和 USB 大容量存储的 SCSI 命令。\n\n"
                       "不需要注册，也不需要登录。\n\n"
                       "串口会独占打开（默认 115200 8N1）。端口已被其它程序打开时，"
                       "用户态无法旁路监听。\n\n"
                       "USB 实时抓包使用本机已安装的 USBPcap，需要管理员权限。"
                       "本程序不附带、不安装内核驱动。\n"
                       "SCSI 只解码 Bulk-Only (BOT)。走 UAS 的设备只显示原始 URB。"));
}

void MainWindow::showError(const QString &text)
{
    m_error->setText(text);
    m_error->setVisible(!text.isEmpty());
    if (!text.isEmpty())
        statusBar()->showMessage(text, 8000);
}

void MainWindow::setRunning(bool running)
{
    m_running = running;
    m_btnStart->setEnabled(!running);
    m_btnStop->setEnabled(running);
    m_sbState->setText(running ? (m_paused ? QStringLiteral("已暂停") : QStringLiteral("监视中")) : QStringLiteral("空闲"));
    m_sbState->setStyleSheet(running ? QStringLiteral("color:#58c27a") : QStringLiteral("color:#9aa7b3"));
}

void MainWindow::ingestRows(const QVector<CaptureRow> &rows)
{
    if (rows.isEmpty())
        return;
    m_model->appendMany(rows);
    if (rows.size() > 40 || m_textTimer->isActive())
        m_textTimer->start(60);
    else {
        for (const CaptureRow &row : rows)
            appendTextRow(row);
    }
    if (m_autoScroll->isChecked() && !m_paused)
        maybeScroll();
    refreshStats();
}

void MainWindow::addSerial(const QString &port, bool isWrite, const QByteArray &data)
{
    CaptureRow row;
    row.seq = ++m_seq;
    row.timeText = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"));
    row.port = port;
    row.direction = isWrite ? QStringLiteral("写") : QStringLiteral("读");
    row.summary = isWrite ? QStringLiteral("WRITE") : QStringLiteral("READ");
    row.length = data.size();
    row.data = data.left(4096);
    row.hexPreview = toHexPreview(row.data);
    row.asciiPreview = toAsciiPreview(row.data);
    row.outbound = isWrite;
    row.usb = false;
    ++m_packets;
    m_bytes += data.size();
    if (isWrite)
        ++m_writes;
    else
        ++m_reads;
    notePort(port);
    ingestRows({row});
}

void MainWindow::handleUsbPacket(const UsbPacket &packet)
{
    CaptureRow row;
    row.seq = m_seq;
    row.timeText = formatUs(packet.unixUs);
    row.port = packet.address();
    row.direction = packet.directionLabel();
    row.summary = QStringLiteral("%1 %2  %3  %4")
                      .arg(packet.transferLabel(), packet.stageLabel(), urbFunctionName(packet.function), packet.statusLabel());
    row.length = packet.data.size();
    row.data = packet.data.left(kStoreCap);
    row.hexPreview = toHexPreview(row.data);
    row.asciiPreview = toAsciiPreview(row.data);
    row.outbound = !packet.isIn();
    row.usb = true;
    ++m_packets;
    m_bytes += packet.data.size();
    if (packet.isIn())
        ++m_reads;
    else
        ++m_writes;
    notePort(row.port);
    rememberDevice(packet.bus, packet.device);
    ingestRows({row});

    const MscUpdate update = m_scsiDecoder.push(packet);
    if (update.event != MscEvent::None)
        upsertScsi(update.command);
}

void MainWindow::upsertScsi(const ScsiCommand &command)
{
    ScsiRow row;
    row.seq = command.index;
    row.timeText = formatUs(command.unixUs);
    row.device = command.deviceLabel();
    row.tag = QStringLiteral("0x") + QString::number(command.tag, 16).rightJustified(8, QLatin1Char('0')).toUpper();
    row.lun = QString::number(command.lun);
    row.name = command.info.name;
    row.lba = command.info.lba ? QString::number(*command.info.lba) : QStringLiteral("-");
    row.blocks = command.info.blocks ? QString::number(*command.info.blocks) : QStringLiteral("-");
    row.status = command.statusLabel();
    row.duration = command.durationMs() ? QString::number(*command.durationMs(), 'f', 3) : QString();
    row.summary = command.summary();
    row.cdb = command.cdb;
    row.sample = command.dataSample;
    row.statusCode = int(command.status);
    m_scsi->upsert(row);
    notePort(row.device);
}

void MainWindow::appendTextRow(const CaptureRow &row)
{
    const bool sourceOk = m_filter.source == 0 || (m_filter.source == 1 && !row.usb) || (m_filter.source == 2 && row.usb);
    const bool portOk = m_filter.port.isEmpty() || m_filter.port == row.port;
    if (!sourceOk || !portOk)
        return;
    if (!m_filter.text.isEmpty() && !row.port.contains(m_filter.text, Qt::CaseInsensitive)
        && !row.direction.contains(m_filter.text, Qt::CaseInsensitive)
        && !row.summary.contains(m_filter.text, Qt::CaseInsensitive)
        && !row.hexPreview.contains(m_filter.text, Qt::CaseInsensitive)
        && !row.asciiPreview.contains(m_filter.text, Qt::CaseInsensitive))
        return;

    m_line->appendPlainText(QStringLiteral("%1  %2  %3  %4  %5  %6")
                                 .arg(row.seq, 6, 10, QChar('0'))
                                 .arg(row.timeText, -12)
                                 .arg(row.port, -10)
                                 .arg(row.direction, -3)
                                 .arg(row.length, 6)
                                 .arg(row.hexPreview));
    if (m_line->blockCount() > 2500) {
        QTextCursor cursor(m_line->document());
        cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, 400);
        cursor.removeSelectedText();
    }

    QTextCursor cursor(m_terminal->document());
    cursor.movePosition(QTextCursor::End);
    QTextCharFormat fmt;
    fmt.setForeground(QColor(row.outbound ? QStringLiteral("#f5b04a") : QStringLiteral("#4da6ff")));
    fmt.setFont(m_mono);
    cursor.setCharFormat(fmt);
    cursor.insertText(QStringLiteral("[%1 %2] %3\n").arg(row.direction, row.port, row.asciiPreview));
}

void MainWindow::rebuildTextViews()
{
    m_line->clear();
    m_terminal->clear();
    const int total = m_proxy->rowCount();
    const int start = qMax(0, total - 2000);
    if (start > 0)
        m_line->appendPlainText(QStringLiteral("… 仅显示最后 2000 行"));
    for (int i = start; i < total; ++i) {
        const QModelIndex src = m_proxy->mapToSource(m_proxy->index(i, 0));
        const CaptureRow *row = m_model->at(src.row());
        if (row)
            appendTextRow(*row);
    }
    maybeScroll();
}

void MainWindow::showCaptureDetail(const CaptureRow *row)
{
    if (!row)
        return;
    const QString note = row->length > row->data.size()
        ? QStringLiteral("（显示前 %1 / %2 字节）\n").arg(row->data.size()).arg(row->length)
        : QString();
    const QString dump = note + toHexDump(row->data);
    m_meta->setText(QStringLiteral("#%1  %2  %3  %4  %5 字节\n%6")
                        .arg(QString::number(row->seq), row->timeText, row->port, row->direction,
                             QString::number(row->length), row->summary));
    m_hex->setPlainText(dump);
    m_ascii->setPlainText(toAsciiPreview(row->data, row->data.size()));
    m_dump->setPlainText(dump);
}

void MainWindow::showScsiDetail(const ScsiRow *row)
{
    if (!row)
        return;
    m_meta->setText(QStringLiteral("#%1  %2  %3  %4  %5\n%6")
                        .arg(QString::number(row->seq), row->timeText, row->device, row->name, row->status, row->summary));
    const QString dump = QStringLiteral("CDB\n") + toHexDump(row->cdb) + QStringLiteral("\nDATA\n") + toHexDump(row->sample);
    m_hex->setPlainText(dump);
    m_ascii->setPlainText(toAsciiPreview(row->sample, row->sample.size()));
    m_dump->setPlainText(dump);
}

void MainWindow::refreshStats()
{
    const qint64 ms = m_rateClock.elapsed();
    double rate = 0;
    if (ms >= 400) {
        rate = double(m_packets - m_pktsMark) * 1000.0 / double(ms);
        m_pktsMark = m_packets;
        m_rateClock.restart();
    }
    m_sbPkts->setText(QStringLiteral("包 %1").arg(m_packets));
    m_sbBytes->setText(QStringLiteral("字节 %1").arg(m_bytes));
    m_sbRead->setText(QStringLiteral("读 %1").arg(m_reads));
    m_sbRead->setStyleSheet(QStringLiteral("color:#4da6ff"));
    m_sbWrite->setText(QStringLiteral("写 %1").arg(m_writes));
    m_sbWrite->setStyleSheet(QStringLiteral("color:#f5b04a"));
    m_sbRate->setText(QStringLiteral("速率 %1 pkt/s").arg(QString::number(rate, 'f', 1)));
    if (m_running)
        m_sbState->setText(m_paused ? QStringLiteral("已暂停") : QStringLiteral("监视中"));
}

void MainWindow::notePort(const QString &port)
{
    if (port.isEmpty() || m_seenPorts.contains(port))
        return;
    m_seenPorts.insert(port);
    m_portFilter->addItem(port);
}

void MainWindow::rememberDevice(quint16 bus, quint16 device)
{
    const QString label = QStringLiteral("%1.%2").arg(bus).arg(device);
    if (m_seenDevices.contains(label))
        return;
    m_seenDevices.insert(label);
    auto *item = new QTreeWidgetItem(m_devRoot, {QStringLiteral("设备 ") + label});
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    m_devRoot->setExpanded(true);
}

void MainWindow::maybeScroll() const
{
    if (!m_autoScroll->isChecked())
        return;
    m_table->scrollToBottom();
    m_line->moveCursor(QTextCursor::End);
    m_terminal->moveCursor(QTextCursor::End);
    m_scsiTable->scrollToBottom();
}

QStringList MainWindow::checkedNames(const QString &kind) const
{
    QTreeWidgetItem *root = kind == QLatin1String("com") ? m_serialRoot : m_usbRoot;
    QStringList names;
    if (!root)
        return names;
    for (int i = 0; i < root->childCount(); ++i) {
        QTreeWidgetItem *item = root->child(i);
        if (item->checkState(0) == Qt::Checked && item->data(0, kKindRole).toString() == kind)
            names.append(item->data(0, kNameRole).toString());
    }
    return names;
}

QString MainWindow::selectedCom() const
{
    QTreeWidgetItem *item = m_tree->currentItem();
    if (item && item->data(0, kKindRole).toString() == QLatin1String("com"))
        return item->data(0, kNameRole).toString();
    return {};
}

void MainWindow::openPcap(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        showError(QStringLiteral("无法打开 %1").arg(path));
        return;
    }
    PcapParser parser;
    int packets = 0;
    int skipped = 0;
    const qint64 scsiBefore = m_scsi->rowCount();
    while (!file.atEnd()) {
        const QList<PcapRecord> recs = parser.feed(file.read(256 * 1024));
        for (const PcapRecord &rec : recs) {
            if (rec.linkType != 249) {
                ++skipped;
                continue;
            }
            const auto packet = parseUsbPcapFrame(rec.data, rec.unixUs, ++m_seq);
            if (!packet) {
                ++skipped;
                continue;
            }
            handleUsbPacket(*packet);
            ++packets;
        }
    }
    const QString tail = parser.finish();
    if (parser.hasError())
        showError(parser.error());
    else if (!tail.isEmpty() && packets == 0)
        showError(tail);
    else if (packets == 0 && skipped > 0)
        showError(QStringLiteral("文件里没有 USBPcap 记录（链路类型不是 249）。"));
    else
        showError({});
    m_tabs->setCurrentIndex(m_scsi->rowCount() > scsiBefore ? 4 : 0);
    statusBar()->showMessage(QStringLiteral("已载入 %1：USB %2 条，SCSI %3 条").arg(path).arg(packets).arg(m_scsi->rowCount() - scsiBefore));
}

void MainWindow::togglePause()
{
    m_paused = !m_paused;
    if (!m_paused)
        m_scsiDecoder.reset();
    if (m_running)
        m_sbState->setText(m_paused ? QStringLiteral("已暂停") : QStringLiteral("监视中"));
    statusBar()->showMessage(m_paused ? QStringLiteral("已暂停，新数据不记入列表") : QStringLiteral("继续记录"));
}
