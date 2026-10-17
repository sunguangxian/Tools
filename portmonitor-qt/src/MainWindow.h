#pragma once

#include <QHash>
#include <QMainWindow>
#include <QSerialPort>

class QListWidget;
class QListWidgetItem;
class QTableWidget;
class QLineEdit;
class QComboBox;
class QPushButton;
class QLabel;

struct Session {
    QString name;
    QString portName;
    QSerialPort *port = nullptr;
};

/// Multi-session port monitor: add/remove sessions, log RX from all ports.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void refreshPorts();
    void onAddSession();
    void onRemoveSession();
    void onSessionReadyRead();
    void onSessionError(QSerialPort::SerialPortError error);

private:
    void buildUi();
    void updateStatus();
    void appendLogRow(const QString &direction, const QString &session,
                      const QByteArray &data);
    Session *sessionForPort(QSerialPort *port);
    void closeAndDeleteSession(Session *session, QListWidgetItem *item);

    QListWidget *m_sessionList = nullptr;
    QLineEdit *m_sessionNameEdit = nullptr;
    QComboBox *m_portCombo = nullptr;
    QPushButton *m_btnRefresh = nullptr;
    QPushButton *m_btnAddSession = nullptr;
    QPushButton *m_btnRemoveSession = nullptr;
    QTableWidget *m_logTable = nullptr;
    QLabel *m_statusLabel = nullptr;

    QHash<QListWidgetItem *, Session *> m_sessions;
};
