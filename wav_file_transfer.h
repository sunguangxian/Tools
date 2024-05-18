#ifndef WAV_FILE_TRANSFER_H
#define WAV_FILE_TRANSFER_H

#include <QtGlobal>

#pragma pack(push, 1) // 以字节对齐方式1进行存储
struct WavHeader {
    char riff[4];          // RIFF 标志（4 字节）
    quint32 fileSize;     // 文件大小（4 字节）
    char wave[4];          // WAVE 标志（4 字节）
    char fmt[4];           // fmt 标志（4 字节）
    quint32 fmtSize;      // fmt 大小（4 字节）
    quint16 audioFormat;  // 音频格式（2 字节）
    quint16 numChannels;  // 声道数（2 字节）
    quint32 sampleRate;   // 采样率（4 字节）
    quint32 byteRate;     // 数据传输速率（4 字节）
    quint16 blockAlign;   // 数据块对齐单位（2 字节）
    quint16 bitsPerSample;// 每个样本的位数（2 字节）
    char data[4];          // data 标志（4 字节）
    quint32 dataSize;     // 数据大小（4 字节）
};
#pragma pack(pop) // 恢复默认字节对齐方式



#endif // WAV_FILE_TRANSFER_H
