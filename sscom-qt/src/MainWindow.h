#pragma once

#include <QMainWindow>
#include <QSerialPort>

class QComboBox;
class QPushButton;
class QCheckBox;
class QPlainTextEdit;
class QSpinBox;
class QLabel;

/// Serial debugger: enumerate / open / close / RX / TX.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void refreshPorts();
    void toggleOpen();
    void onReadyRead();
    void onSend();
    void onClearReceive();
    void onSerialError(QSerialPort::SerialPortError error);

private:
    void buildUi();
    void setPortParamsEnabled(bool enabled);
    void appendReceive(const QByteArray &data);
    QByteArray parseHexInput(const QString &text, QString *error) const;
    QSerialPort::DataBits dataBitsFromUi() const;
    QSerialPort::Parity parityFromUi() const;
    QSerialPort::StopBits stopBitsFromUi() const;

    QSerialPort *m_serial = nullptr;

    QPlainTextEdit *m_receiveEdit = nullptr;
    QPlainTextEdit *m_sendEdit = nullptr;
    QComboBox *m_portCombo = nullptr;
    QComboBox *m_baudCombo = nullptr;
    QComboBox *m_dataBitsCombo = nullptr;
    QComboBox *m_parityCombo = nullptr;
    QComboBox *m_stopBitsCombo = nullptr;
    QPushButton *m_btnRefresh = nullptr;
    QPushButton *m_btnOpen = nullptr;
    QPushButton *m_btnSend = nullptr;
    QPushButton *m_btnClearRecv = nullptr;
    QCheckBox *m_chkHexRecv = nullptr;
    QCheckBox *m_chkHexSend = nullptr;
    QCheckBox *m_chkTimestamp = nullptr;
    QSpinBox *m_timeoutSpin = nullptr;
    QLabel *m_statusLabel = nullptr;
};
