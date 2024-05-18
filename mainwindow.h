#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFileSystemWatcher>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE


class MainWindow : public QMainWindow
{
    Q_OBJECT

private:
    QFileSystemWatcher fileWatcher; //用于监视文件和目录

    QString src_wav_file = NULL;
    QString dst_file = NULL;
    QString dst_dir = NULL;

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    /*
public slots:
    void do_srcfileChanged(const QString &path);
    void do_dstfileChanged(const QString &path);
    void do_beginConvert();
    */

private slots:
    void on_btn_fileopen_clicked();
    void on_btn_select_dst_clicked();
    void on_btn_transfer_clicked();

private:
    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
