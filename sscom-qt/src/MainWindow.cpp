#include "MainWindow.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QFont>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIODevice>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSerialPortInfo>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_serial(new QSerialPort(this))
{
    setWindowTitle(tr("SSCOM Qt — 串口调试助手"));
    resize(900, 640);
    buildUi();

    connect(m_serial, &QSerialPort::readyRead, this, &MainWindow::onReadyRead);
    connect(m_serial, &QSerialPort::errorOccurred, this, &MainWindow::onSerialError);
    connect(m_btnRefresh, &QPushButton::clicked, this, &MainWindow::refreshPorts);
    connect(m_btnOpen, &QPushButton::clicked, this, &MainWindow::toggleOpen);
    connect(m_btnSend, &QPushButton::clicked, this, &MainWindow::onSend);
    connect(m_btnClearRecv, &QPushButton::clicked, this, &MainWindow::onClearReceive);

    refreshPorts();
    statusBar()->showMessage(tr("就绪"));
}

MainWindow::~MainWindow()
{
    if (m_serial->isOpen())
        m_serial->close();
}

void MainWindow::buildUi()
{
    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *root = new QHBoxLayout(central);

    auto *left = new QVBoxLayout;

    auto *recvBox = new QGroupBox(tr("接收区"), central);
    auto *recvLay = new QVBoxLayout(recvBox);
    m_receiveEdit = new QPlainTextEdit(recvBox);
    m_receiveEdit->setReadOnly(true);
    m_receiveEdit->setPlaceholderText(tr("接收数据将显示在这里"));
    m_receiveEdit->setFont(QFont(QStringLiteral("Consolas"), 10));
    recvLay->addWidget(m_receiveEdit);
    auto *recvOpts = new QHBoxLayout;
    m_chkHexRecv = new QCheckBox(tr("HEX 显示"), recvBox);
    m_chkTimestamp = new QCheckBox(tr("加时间戳"), recvBox);
    m_btnClearRecv = new QPushButton(tr("清空接收"), recvBox);
    recvOpts->addWidget(m_chkHexRecv);
    recvOpts->addWidget(m_chkTimestamp);
    recvOpts->addStretch();
    recvOpts->addWidget(m_btnClearRecv);
    recvLay->addLayout(recvOpts);
    left->addWidget(recvBox, 3);

    auto *sendBox = new QGroupBox(tr("发送区"), central);
    auto *sendLay = new QVBoxLayout(sendBox);
    m_sendEdit = new QPlainTextEdit(sendBox);
    m_sendEdit->setPlaceholderText(tr("待发送数据；勾选 HEX 发送时可用空格分隔字节，如 01 0A FF"));
    m_sendEdit->setMaximumHeight(120);
    m_sendEdit->setFont(QFont(QStringLiteral("Consolas"), 10));
    sendLay->addWidget(m_sendEdit);
    auto *sendOpts = new QHBoxLayout;
    m_chkHexSend = new QCheckBox(tr("HEX 发送"), sendBox);
    m_btnSend = new QPushButton(tr("发送"), sendBox);
    sendOpts->addWidget(m_chkHexSend);
    sendOpts->addStretch();
    sendOpts->addWidget(m_btnSend);
    sendLay->addLayout(sendOpts);
    left->addWidget(sendBox, 1);

    root->addLayout(left, 1);

    auto *paramBox = new QGroupBox(tr("串口参数"), central);
    paramBox->setFixedWidth(240);
    auto *form = new QFormLayout(paramBox);

    auto *portRow = new QHBoxLayout;
    m_portCombo = new QComboBox(paramBox);
    m_portCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_btnRefresh = new QPushButton(tr("刷新"), paramBox);
    m_btnRefresh->setFixedWidth(52);
    portRow->addWidget(m_portCombo, 1);
    portRow->addWidget(m_btnRefresh);
    form->addRow(tr("端口"), portRow);

    m_baudCombo = new QComboBox(paramBox);
    for (int b : {9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600})
        m_baudCombo->addItem(QString::number(b), b);
    m_baudCombo->setCurrentText(QStringLiteral("115200"));
    form->addRow(tr("波特率"), m_baudCombo);

    m_dataBitsCombo = new QComboBox(paramBox);
    m_dataBitsCombo->addItems({QStringLiteral("5"), QStringLiteral("6"),
                               QStringLiteral("7"), QStringLiteral("8")});
    m_dataBitsCombo->setCurrentText(QStringLiteral("8"));
    form->addRow(tr("数据位"), m_dataBitsCombo);

    m_parityCombo = new QComboBox(paramBox);
    m_parityCombo->addItems({tr("无"), tr("奇"), tr("偶"), tr("标记"), tr("空格")});
    form->addRow(tr("校验"), m_parityCombo);

    m_stopBitsCombo = new QComboBox(paramBox);
    m_stopBitsCombo->addItems({QStringLiteral("1"), QStringLiteral("1.5"), QStringLiteral("2")});
    form->addRow(tr("停止位"), m_stopBitsCombo);

    m_timeoutSpin = new QSpinBox(paramBox);
    m_timeoutSpin->setRange(0, 10000);
    m_timeoutSpin->setSuffix(tr(" ms"));
    m_timeoutSpin->setValue(0);
    form->addRow(tr("超时"), m_timeoutSpin);

    m_btnOpen = new QPushButton(tr("打开串口"), paramBox);
    form->addRow(m_btnOpen);

    m_statusLabel = new QLabel(tr("未连接"), paramBox);
    m_statusLabel->setWordWrap(true);
    form->addRow(tr("状态"), m_statusLabel);

    root->addWidget(paramBox);
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

void MainWindow::setPortParamsEnabled(bool enabled)
{
    m_portCombo->setEnabled(enabled);
    m_btnRefresh->setEnabled(enabled);
    m_baudCombo->setEnabled(enabled);
    m_dataBitsCombo->setEnabled(enabled);
    m_parityCombo->setEnabled(enabled);
    m_stopBitsCombo->setEnabled(enabled);
    m_timeoutSpin->setEnabled(enabled);
}

QSerialPort::DataBits MainWindow::dataBitsFromUi() const
{
    switch (m_dataBitsCombo->currentText().toInt()) {
    case 5: return QSerialPort::Data5;
    case 6: return QSerialPort::Data6;
    case 7: return QSerialPort::Data7;
    default: return QSerialPort::Data8;
    }
}

QSerialPort::Parity MainWindow::parityFromUi() const
{
    switch (m_parityCombo->currentIndex()) {
    case 1: return QSerialPort::OddParity;
    case 2: return QSerialPort::EvenParity;
    case 3: return QSerialPort::MarkParity;
    case 4: return QSerialPort::SpaceParity;
    default: return QSerialPort::NoParity;
    }
}

QSerialPort::StopBits MainWindow::stopBitsFromUi() const
{
    const QString t = m_stopBitsCombo->currentText();
    if (t == QLatin1String("1.5"))
        return QSerialPort::OneAndHalfStop;
    if (t == QLatin1String("2"))
        return QSerialPort::TwoStop;
    return QSerialPort::OneStop;
}

void MainWindow::toggleOpen()
{
    if (m_serial->isOpen()) {
        m_serial->close();
        m_btnOpen->setText(tr("打开串口"));
        m_statusLabel->setText(tr("未连接"));
        setPortParamsEnabled(true);
        statusBar()->showMessage(tr("已关闭串口"));
        return;
    }

    const QString portName = m_portCombo->currentData().toString();
    if (portName.isEmpty()) {
        statusBar()->showMessage(tr("请选择串口"));
        return;
    }

    m_serial->setPortName(portName);
    m_serial->setBaudRate(m_baudCombo->currentData().toInt());
    m_serial->setDataBits(dataBitsFromUi());
    m_serial->setParity(parityFromUi());
    m_serial->setStopBits(stopBitsFromUi());
    m_serial->setFlowControl(QSerialPort::NoFlowControl);

    if (!m_serial->open(QIODevice::ReadWrite)) {
        m_statusLabel->setText(tr("打开失败"));
        statusBar()->showMessage(tr("打开失败：%1").arg(m_serial->errorString()));
        return;
    }

    m_btnOpen->setText(tr("关闭串口"));
    m_statusLabel->setText(tr("已连接 %1 @ %2")
                               .arg(portName)
                               .arg(m_baudCombo->currentText()));
    setPortParamsEnabled(false);
    statusBar()->showMessage(tr("已打开 %1").arg(portName));
}

void MainWindow::onReadyRead()
{
    appendReceive(m_serial->readAll());
}

void MainWindow::appendReceive(const QByteArray &data)
{
    if (data.isEmpty())
        return;

    QString line;
    if (m_chkTimestamp->isChecked()) {
        line += QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz"));
        line += QLatin1Char(' ');
    }

    if (m_chkHexRecv->isChecked())
        line += QString::fromLatin1(data.toHex(' ').toUpper());
    else
        line += QString::fromUtf8(data);

    m_receiveEdit->appendPlainText(line);
}

void MainWindow::onSend()
{
    if (!m_serial->isOpen()) {
        statusBar()->showMessage(tr("请先打开串口"));
        return;
    }

    const QString text = m_sendEdit->toPlainText();
    if (text.isEmpty()) {
        statusBar()->showMessage(tr("发送内容为空"));
        return;
    }

    QByteArray payload;
    if (m_chkHexSend->isChecked()) {
        QString err;
        payload = parseHexInput(text, &err);
        if (!err.isEmpty()) {
            statusBar()->showMessage(err);
            return;
        }
        if (payload.isEmpty()) {
            statusBar()->showMessage(tr("HEX 解析结果为空"));
            return;
        }
    } else {
        payload = text.toUtf8();
    }

    const qint64 written = m_serial->write(payload);
    if (written < 0) {
        statusBar()->showMessage(tr("发送失败：%1").arg(m_serial->errorString()));
        return;
    }
    if (!m_serial->flush()) {
        statusBar()->showMessage(tr("flush 失败：%1").arg(m_serial->errorString()));
        return;
    }
    statusBar()->showMessage(tr("已发送 %1 字节").arg(written));
}

QByteArray MainWindow::parseHexInput(const QString &text, QString *error) const
{
    QString cleaned = text;
    cleaned.remove(QLatin1Char(' '));
    cleaned.remove(QLatin1Char('\t'));
    cleaned.remove(QLatin1Char('\n'));
    cleaned.remove(QLatin1Char('\r'));
    cleaned = cleaned.trimmed();

    if (cleaned.isEmpty())
        return {};

    if (cleaned.size() % 2 != 0) {
        if (error)
            *error = tr("HEX 长度必须为偶数");
        return {};
    }

    QByteArray out;
    out.reserve(cleaned.size() / 2);
    for (int i = 0; i < cleaned.size(); i += 2) {
        bool ok = false;
        const int v = cleaned.mid(i, 2).toInt(&ok, 16);
        if (!ok) {
            if (error)
                *error = tr("无效 HEX：%1").arg(cleaned.mid(i, 2));
            return {};
        }
        out.append(static_cast<char>(v));
    }
    return out;
}

void MainWindow::onClearReceive()
{
    m_receiveEdit->clear();
}

void MainWindow::onSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError)
        return;
    if (error == QSerialPort::ResourceError && m_serial->isOpen()) {
        m_serial->close();
        m_btnOpen->setText(tr("打开串口"));
        m_statusLabel->setText(tr("未连接"));
        setPortParamsEnabled(true);
    }
    statusBar()->showMessage(tr("串口错误：%1").arg(m_serial->errorString()));
}
