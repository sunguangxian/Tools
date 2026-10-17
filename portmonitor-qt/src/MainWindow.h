#pragma once

#include <QMainWindow>

class QListWidget;
class QTableWidget;
class QLineEdit;
class QComboBox;
class QPushButton;
class QLabel;

/// Minimal multi-session port monitor shell.
/// Left: session list + new-session stub; right: log table.
/// Avalonia PortMonitor/ remains the working app for now.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void buildUi();
    void addDemoRows();

    QListWidget *m_sessionList = nullptr;
    QLineEdit *m_sessionNameEdit = nullptr;
    QComboBox *m_portCombo = nullptr;
    QPushButton *m_btnAddSession = nullptr;
    QPushButton *m_btnRemoveSession = nullptr;
    QTableWidget *m_logTable = nullptr;
    QLabel *m_statusLabel = nullptr;
};
