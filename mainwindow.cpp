#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDir>
#include <QFileDialog>
#include <wav_file_transfer.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_btn_fileopen_clicked()
{
    //打开文件
    //QString curDir = QDir::currentPath();
    QString curDir = "C:/Users/sungu/Desktop/wav";
    QString aFile = QFileDialog::getOpenFileName(this, "选择wav文件", curDir, "*.wav");
    ui->label_src->setText(aFile);
    this->src_wav_file = aFile;
    if (this->dst_dir.isNull())
    {
        // 获取文件所在文件夹的路径
        QFileInfo fileInfo(aFile);
        QString filePath = fileInfo.absolutePath();

        this->dst_dir = filePath;
    }
}

void MainWindow::on_btn_select_dst_clicked()
{
    //选择目的目录
    QString curDir = QDir::currentPath();
    QString aDir = QFileDialog::getExistingDirectory(this, "选择目的文件夹", curDir, QFileDialog::ShowDirsOnly);
    ui->label_dst->setText(aDir);
    this->dst_dir = aDir;
}
void MainWindow::on_btn_transfer_clicked()
{
    ui->text_state->clear();

    try {
        // 打开文件
        QFile file(this->src_wav_file);
        if (!file.open(QIODevice::ReadOnly))
        {
            throw QString("Failed to open file for reading: ") + file.errorString();
        }

        WavHeader header;
        // 读取文件头部信息
        qint16 header_len = file.read(reinterpret_cast<char *>(&header), sizeof(WavHeader)); // WAV 文件头部通常为 44 个字节
        if (header_len != sizeof(WavHeader))
        {
            throw QString("Incomplete header: ") + QString::number(header_len) + " bytes read";
        }

        // 检查文件的魔术数字
        if (strncmp(header.riff, "RIFF", 4) != 0 || strncmp(header.wave, "WAVE", 4) != 0)
        {
            throw QString("Invalid WAV file: ") + QString::fromLatin1(header.riff, 4) + "/" + QString::fromLatin1(header.wave, 4);
        }

        QFileInfo fileInfo(this->src_wav_file);
        QString fileNameWithoutExtension = fileInfo.baseName();

        this->dst_file = this->dst_dir + "/" + fileNameWithoutExtension + ".c";

        ui->text_state->append("文件信息：" + this->src_wav_file);
        // 将 header 的信息转换为 QString
        QString headerInfo = QString("RIFF: %1\n"
                                     "FileSize: %2\n"
                                     "WAVE: %3\n"
                                     "fmt: %4\n"
                                     "fmtSize: %5\n"
                                     "AudioFormat: %6\n"
                                     "NumChannels: %7\n"
                                     "SampleRate: %8\n"
                                     "ByteRate: %9\n"
                                     "BlockAlign: %10\n"
                                     "BitsPerSample: %11\n"
                                     "data: %12\n"
                                     "DataSize: %13")
                                 .arg(QString::fromLatin1(header.riff, sizeof(header.riff)))
                                 .arg(header.fileSize)
                                 .arg(QString::fromLatin1(header.wave, sizeof(header.wave)))
                                 .arg(QString::fromLatin1(header.fmt, sizeof(header.fmt)))
                                 .arg(header.fmtSize)
                                 .arg(header.audioFormat)
                                 .arg(header.numChannels)
                                 .arg(header.sampleRate)
                                 .arg(header.byteRate)
                                 .arg(header.blockAlign)
                                 .arg(header.bitsPerSample)
                                 .arg(QString::fromLatin1(header.data, sizeof(header.data)))
                                 .arg(header.dataSize);


        // 将 header 的信息附加到 ui->text_state 上
        ui->text_state->append(headerInfo);

        QByteArray wavData = file.readAll();

        QString dataInfo;
        for (int i = 0; i < wavData.size(); ++i) {
            if (i % 16 == 0) {
                // 每行开始添加换行符
                if (i != 0) {
                    dataInfo += "\n";
                }
            } else {
                // 每个数据前添加空格
                dataInfo += " ";
            }
            // 添加 "0x" 前缀和两位十六进制数
            dataInfo += "0x";
            dataInfo += QString("%1").arg(wavData.at(i) & 0xFF, 2, 16, QLatin1Char('0')).toUpper();
            dataInfo += ",";
        }

        // 将最后一行补足到16个数据
        while (dataInfo.size() % 48 != 0) {
            dataInfo += " ";
        }

        file.close();

        QFile dst_file(this->dst_file);
        if (!dst_file.open(QIODevice::WriteOnly)) {
            throw QString("Failed to open file for writing: ") + dst_file.errorString();
        }

        QString transfer_str = "const uint8_t pcm_data[] = {\n" + dataInfo + "\n};";

        QByteArray byteArray = transfer_str.toUtf8();

        dst_file.write(byteArray);

        dst_file.close();

        ui->text_state->append("文件转换完成: " + this->dst_file);
    } catch (const QString &errorMessage) {
        ui->text_state->append("Error: " + errorMessage);
    }
}


