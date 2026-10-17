#include "MainWindow.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("SSCOM Qt — 串口调试助手（骨架）"));
    resize(900, 640);
    buildUi();
    statusBar()->showMessage(tr("骨架：尚未接入串口逻辑；Avalonia 版见 ../sscom/"));
}

void MainWindow::buildUi()
{
    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *root = new QHBoxLayout(central);

    // —— Left: receive + send ——
    auto *left = new QVBoxLayout;

    auto *recvBox = new QGroupBox(tr("接收区"), central);
    auto *recvLay = new QVBoxLayout(recvBox);
    m_receiveEdit = new QPlainTextEdit(recvBox);
    m_receiveEdit->setReadOnly(true);
    m_receiveEdit->setPlaceholderText(tr("接收数据显示（占位）"));
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
    m_sendEdit->setPlaceholderText(tr("待发送数据（占位）"));
    m_sendEdit->setMaximumHeight(120);
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

    // —— Right: serial params ——
    auto *paramBox = new QGroupBox(tr("串口参数"), central);
    paramBox->setFixedWidth(220);
    auto *form = new QFormLayout(paramBox);

    m_portCombo = new QComboBox(paramBox);
    m_portCombo->addItem(tr("(刷新后列出端口)"));
    form->addRow(tr("端口"), m_portCombo);

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
    form->addRow(tr("状态"), m_statusLabel);

    root->addWidget(paramBox);
}
