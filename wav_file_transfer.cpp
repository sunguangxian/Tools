#include <QFileInfo>

bool isWaveFile(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    QString fileExtension = fileInfo.suffix().toLower();

    return fileExtension == "wav";
}
