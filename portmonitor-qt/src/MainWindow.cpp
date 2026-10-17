#include "MainWindow.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDateTime>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIODevice>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSerialPortInfo>
#include <QSplitter>
#include <QStatusBar>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("PortMonitor Qt — 端口监视"));
    resize(1000, 680);
    buildUi();

    connect(m_btnRefresh, &QPushButton::clicked, this, &MainWindow::refreshPorts);
    connect(m_btnAddSession, &QPushButton::clicked, this, &MainWindow::onAddSession);
    connect(m_btnRemoveSession, &QPushButton::clicked, this, &MainWindow::onRemoveSession);

    refreshPorts();
    updateStatus();
    statusBar()->showMessage(tr("就绪（新建会话默认 115200 8N1）"));
}

MainWindow::~MainWindow()
{
    const auto items = m_sessions.keys();
    for (QListWidgetItem *item : items) {
        Session *s = m_sessions.take(item);
        if (s) {
            if (s->port) {
                if (s->port->isOpen())
                    s->port->close();
                s->port->deleteLater();
            }
            delete s;
        }
        delete item;
    }
    m_sessionList->clear();
}

void MainWindow::buildUi()
{
    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *root = new QHBoxLayout(central);

    auto *splitter = new QSplitter(Qt::Horizontal, central);

    auto *left = new QWidget(splitter);
    auto *leftLay = new QVBoxLayout(left);
    leftLay->setContentsMargins(0, 0, 0, 0);

    auto *newBox = new QGroupBox(tr("新建会话"), left);
    auto *form = new QFormLayout(newBox);
    m_sessionNameEdit = new QLineEdit(newBox);
    m_sessionNameEdit->setPlaceholderText(tr("会话名"));
    form->addRow(tr("名称"), m_sessionNameEdit);

    auto *portRow = new QHBoxLayout;
    m_portCombo = new QComboBox(newBox);
    m_btnRefresh = new QPushButton(tr("刷新"), newBox);
    m_btnRefresh->setFixedWidth(52);
    portRow->addWidget(m_portCombo, 1);
    portRow->addWidget(m_btnRefresh);
    form->addRow(tr("端口"), portRow);

    m_btnAddSession = new QPushButton(tr("添加"), newBox);
    form->addRow(m_btnAddSession);
    leftLay->addWidget(newBox);

    auto *listBox = new QGroupBox(tr("会话列表"), left);
    auto *listLay = new QVBoxLayout(listBox);
    m_sessionList = new QListWidget(listBox);
    listLay->addWidget(m_sessionList);
    m_btnRemoveSession = new QPushButton(tr("移除所选"), listBox);
    listLay->addWidget(m_btnRemoveSession);
    leftLay->addWidget(listBox, 1);

    m_statusLabel = new QLabel(tr("会话：0 活跃"), left);
    leftLay->addWidget(m_statusLabel);

    left->setMinimumWidth(220);
    left->setMaximumWidth(320);
    splitter->addWidget(left);

    auto *right = new QWidget(splitter);
    auto *rightLay = new QVBoxLayout(right);
    rightLay->setContentsMargins(0, 0, 0, 0);

    auto *logBox = new QGroupBox(tr("日志"), right);
    auto *logLay = new QVBoxLayout(logBox);
    m_logTable = new QTableWidget(0, 4, logBox);
    m_logTable->setHorizontalHeaderLabels(
        {tr("时间"), tr("方向"), tr("会话"), tr("数据")});
    m_logTable->horizontalHeader()->setStretchLastSection(true);
    m_logTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_logTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_logTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_logTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_logTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_logTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_logTable->setAlternatingRowColors(true);
    logLay->addWidget(m_logTable);
    rightLay->addWidget(logBox);

    splitter->addWidget(right);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);

    root->addWidget(splitter);
}

void MainWindow::refreshPorts()
{
    const QString current = m_portCombo->currentData().toString();
    m_portCombo->clear();

    const auto ports = QSerialPortInfo::availablePorts();
    if (ports.isEmpty()) {
        m_portCombo->addItem(tr("(无可用端口)"), QString());
        return;
    }

    int restore = -1;
    for (const QSerialPortInfo &info : ports) {
        QString label = info.portName();
        if (!info.description().isEmpty())
            label += QStringLiteral(" — ") + info.description();
        m_portCombo->addItem(label, info.portName());
        if (!current.isEmpty() && info.portName() == current)
            restore = m_portCombo->count() - 1;
    }
    if (restore >= 0)
        m_portCombo->setCurrentIndex(restore);
}

void MainWindow::updateStatus()
{
    int active = 0;
    for (Session *s : m_sessions) {
        if (s && s->port && s->port->isOpen())
            ++active;
    }
    m_statusLabel->setText(tr("会话：%1 活跃 / 共 %2")
                               .arg(active)
                               .arg(m_sessions.size()));
}

void MainWindow::onAddSession()
{
    const QString portName = m_portCombo->currentData().toString();
    if (portName.isEmpty()) {
        statusBar()->showMessage(tr("请选择串口"));
        return;
    }

    for (Session *existing : m_sessions) {
        if (existing && existing->portName == portName) {
            statusBar()->showMessage(tr("端口 %1 已在会话中").arg(portName));
            return;
        }
    }

    QString name = m_sessionNameEdit->text().trimmed();
    if (name.isEmpty())
        name = portName;

    auto *port = new QSerialPort(this);
    port->setPortName(portName);
    port->setBaudRate(115200);
    port->setDataBits(QSerialPort::Data8);
    port->setParity(QSerialPort::NoParity);
    port->setStopBits(QSerialPort::OneStop);
    port->setFlowControl(QSerialPort::NoFlowControl);

    if (!port->open(QIODevice::ReadWrite)) {
        statusBar()->showMessage(tr("打开失败：%1 — %2")
                                     .arg(portName, port->errorString()));
        delete port;
        return;
    }

    auto *session = new Session;
    session->name = name;
    session->portName = portName;
    session->port = port;

    auto *item = new QListWidgetItem(
        tr("%1 [%2] 115200 8N1").arg(name, portName), m_sessionList);
    m_sessions.insert(item, session);

    connect(port, &QSerialPort::readyRead, this, &MainWindow::onSessionReadyRead);
    connect(port, &QSerialPort::errorOccurred, this, &MainWindow::onSessionError);

    m_sessionNameEdit->clear();
    updateStatus();
    statusBar()->showMessage(tr("已添加会话 %1 → %2").arg(name, portName));
}

void MainWindow::onRemoveSession()
{
    QListWidgetItem *item = m_sessionList->currentItem();
    if (!item) {
        statusBar()->showMessage(tr("请先选择会话"));
        return;
    }
    Session *session = m_sessions.value(item, nullptr);
    closeAndDeleteSession(session, item);
    updateStatus();
    statusBar()->showMessage(tr("已移除会话"));
}

void MainWindow::closeAndDeleteSession(Session *session, QListWidgetItem *item)
{
    if (session) {
        m_sessions.remove(item);
        if (session->port) {
            if (session->port->isOpen())
                session->port->close();
            session->port->deleteLater();
            session->port = nullptr;
        }
        delete session;
    } else if (item) {
        m_sessions.remove(item);
    }
    if (item) {
        const int row = m_sessionList->row(item);
        delete m_sessionList->takeItem(row);
    }
}

Session *MainWindow::sessionForPort(QSerialPort *port)
{
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        if (it.value() && it.value()->port == port)
            return it.value();
    }
    return nullptr;
}

void MainWindow::onSessionReadyRead()
{
    auto *port = qobject_cast<QSerialPort *>(sender());
    if (!port)
        return;
    Session *session = sessionForPort(port);
    if (!session)
        return;
    const QByteArray data = port->readAll();
    if (!data.isEmpty())
        appendLogRow(QStringLiteral("RX"), session->name, data);
}

void MainWindow::onSessionError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError)
        return;
    auto *port = qobject_cast<QSerialPort *>(sender());
    if (!port)
        return;
    Session *session = sessionForPort(port);
    const QString name = session ? session->name : port->portName();
    statusBar()->showMessage(tr("会话 %1 错误：%2").arg(name, port->errorString()));

    if (error == QSerialPort::ResourceError) {
        for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
            if (it.value() && it.value()->port == port) {
                QListWidgetItem *item = it.key();
                closeAndDeleteSession(it.value(), item);
                updateStatus();
                break;
            }
        }
    }
}

void MainWindow::appendLogRow(const QString &direction, const QString &session,
                              const QByteArray &data)
{
    const int r = m_logTable->rowCount();
    m_logTable->insertRow(r);
    m_logTable->setItem(
        r, 0,
        new QTableWidgetItem(
            QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz"))));
    m_logTable->setItem(r, 1, new QTableWidgetItem(direction));
    m_logTable->setItem(r, 2, new QTableWidgetItem(session));

    // Prefer printable ASCII; fall back to hex when binary
    bool printable = true;
    for (unsigned char c : data) {
        if (c < 0x20 && c != '\r' && c != '\n' && c != '\t') {
            printable = false;
            break;
        }
        if (c > 0x7E) {
            printable = false;
            break;
        }
    }
    const QString text = printable ? QString::fromUtf8(data)
                                   : QString::fromLatin1(data.toHex(' ').toUpper());
    m_logTable->setItem(r, 3, new QTableWidgetItem(text));
    m_logTable->scrollToBottom();
}
