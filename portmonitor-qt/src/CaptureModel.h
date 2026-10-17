#pragma once

#include <QAbstractTableModel>
#include <QByteArray>
#include <QSortFilterProxyModel>
#include <QString>
#include <QVector>

struct ViewFilter {
    int source = 0; ///< 0 全部, 1 串口, 2 USB
    QString port;
    QString text;
};

struct CaptureRow {
    qint64 seq = 0;
    QString timeText;
    QString port;
    QString direction;
    QString summary;
    int length = 0;
    QByteArray data;
    QString hexPreview;
    QString asciiPreview;
    bool outbound = false;
    bool usb = false;
};

struct ScsiRow {
    qint64 seq = 0;
    QString timeText;
    QString device;
    QString tag;
    QString lun;
    QString name;
    QString lba;
    QString blocks;
    QString status;
    QString duration;
    QString summary;
    QByteArray cdb;
    QByteArray sample;
    int statusCode = -1;
};

class CaptureModel : public QAbstractTableModel {
public:
    explicit CaptureModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    void appendMany(const QVector<CaptureRow> &rows);
    void clear();
    const CaptureRow *at(int row) const;

private:
    void trim();
    QVector<CaptureRow> m_rows;
    static constexpr int kMaxRows = 20000;
};

class ScsiModel : public QAbstractTableModel {
public:
    explicit ScsiModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    void upsert(const ScsiRow &row);
    void clear();
    const ScsiRow *at(int row) const;

private:
    int indexOf(qint64 seq) const;
    QVector<ScsiRow> m_rows;
};

class CaptureFilter : public QSortFilterProxyModel {
public:
    explicit CaptureFilter(QObject *parent = nullptr)
        : QSortFilterProxyModel(parent)
    {
    }
    ViewFilter *state = nullptr;
    CaptureModel *rows = nullptr;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
};

class ScsiFilter : public QSortFilterProxyModel {
public:
    explicit ScsiFilter(QObject *parent = nullptr)
        : QSortFilterProxyModel(parent)
    {
    }
    ViewFilter *state = nullptr;
    ScsiModel *rows = nullptr;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
};

QString toHexPreview(const QByteArray &data, int maxBytes = 24);
QString toAsciiPreview(const QByteArray &data, int maxBytes = 48);
QString toHexDump(const QByteArray &data);
QString csvField(const QString &text);
