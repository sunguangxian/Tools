#pragma once

#include <QMainWindow>

class QComboBox;
class QPushButton;
class QCheckBox;
class QPlainTextEdit;
class QLineEdit;
class QSpinBox;
class QLabel;

/// Minimal serial-debugger shell (receive / send / port params).
/// Logic stubs only — Avalonia sscom/ remains the working app for now.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void buildUi();

    QPlainTextEdit *m_receiveEdit = nullptr;
    QPlainTextEdit *m_sendEdit = nullptr;
    QComboBox *m_portCombo = nullptr;
    QComboBox *m_baudCombo = nullptr;
    QComboBox *m_dataBitsCombo = nullptr;
    QComboBox *m_parityCombo = nullptr;
    QComboBox *m_stopBitsCombo = nullptr;
    QPushButton *m_btnOpen = nullptr;
    QPushButton *m_btnSend = nullptr;
    QPushButton *m_btnClearRecv = nullptr;
    QCheckBox *m_chkHexRecv = nullptr;
    QCheckBox *m_chkHexSend = nullptr;
    QCheckBox *m_chkTimestamp = nullptr;
    QSpinBox *m_timeoutSpin = nullptr;
    QLabel *m_statusLabel = nullptr;
};
