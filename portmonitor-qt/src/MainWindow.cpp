#include "MainWindow.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QAbstractItemView>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSplitter>
#include <QStatusBar>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("PortMonitor Qt — 端口监视（骨架）"));
    resize(1000, 680);
    buildUi();
    addDemoRows();
    statusBar()->showMessage(tr("骨架：尚未接入旁路/会话逻辑；Avalonia 版见 ../PortMonitor/"));
}

void MainWindow::buildUi()
{
    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *root = new QHBoxLayout(central);

    auto *splitter = new QSplitter(Qt::Horizontal, central);

    // —— Left: sessions ——
    auto *left = new QWidget(splitter);
    auto *leftLay = new QVBoxLayout(left);
    leftLay->setContentsMargins(0, 0, 0, 0);

    auto *newBox = new QGroupBox(tr("新建会话"), left);
    auto *form = new QFormLayout(newBox);
    m_sessionNameEdit = new QLineEdit(newBox);
    m_sessionNameEdit->setPlaceholderText(tr("会话名"));
    form->addRow(tr("名称"), m_sessionNameEdit);
    m_portCombo = new QComboBox(newBox);
    m_portCombo->addItem(tr("(端口占位)"));
    form->addRow(tr("端口"), m_portCombo);
    m_btnAddSession = new QPushButton(tr("添加"), newBox);
    form->addRow(m_btnAddSession);
    leftLay->addWidget(newBox);

    auto *listBox = new QGroupBox(tr("会话列表"), left);
    auto *listLay = new QVBoxLayout(listBox);
    m_sessionList = new QListWidget(listBox);
    m_sessionList->addItem(tr("示例会话 COM3（占位）"));
    m_sessionList->addItem(tr("示例会话 /dev/ttyUSB0（占位）"));
    listLay->addWidget(m_sessionList);
    m_btnRemoveSession = new QPushButton(tr("移除所选"), listBox);
    listLay->addWidget(m_btnRemoveSession);
    leftLay->addWidget(listBox, 1);

    m_statusLabel = new QLabel(tr("会话：0 活跃（占位）"), left);
    leftLay->addWidget(m_statusLabel);

    left->setMinimumWidth(220);
    left->setMaximumWidth(320);
    splitter->addWidget(left);

    // —— Right: log table ——
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

void MainWindow::addDemoRows()
{
    const QList<QStringList> rows = {
        {QStringLiteral("12:00:01.001"), QStringLiteral("RX"), QStringLiteral("COM3"),
         QStringLiteral("01 03 00 00 00 0A …")},
        {QStringLiteral("12:00:01.015"), QStringLiteral("TX"), QStringLiteral("COM3"),
         QStringLiteral("01 03 14 …")},
    };
    for (const QStringList &cols : rows) {
        const int r = m_logTable->rowCount();
        m_logTable->insertRow(r);
        for (int c = 0; c < cols.size(); ++c)
            m_logTable->setItem(r, c, new QTableWidgetItem(cols.at(c)));
    }
}
