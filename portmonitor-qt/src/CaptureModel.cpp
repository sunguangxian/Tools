#include "CaptureModel.h"

#include <QColor>
#include <QFont>

namespace {

QFont monoFont()
{
    QFont font(QStringLiteral("Consolas"));
    font.setStyleHint(QFont::Monospace);
    font.setPointSize(10);
    return font;
}

bool containsText(const QString &text, const QStringList &fields)
{
    if (text.isEmpty())
        return true;
    for (const QString &field : fields) {
        if (field.contains(text, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

} // namespace

QString toHexPreview(const QByteArray &data, int maxBytes)
{
    const int n = qMin(data.size(), maxBytes);
    QString s;
    s.reserve(n * 3);
    for (int i = 0; i < n; ++i) {
        if (i)
            s.append(QLatin1Char(' '));
        s.append(QStringLiteral("%1").arg(quint8(data[i]), 2, 16, QChar('0')).toUpper());
    }
    if (data.size() > n)
        s.append(QStringLiteral(" …"));
    return s;
}

QString toAsciiPreview(const QByteArray &data, int maxBytes)
{
    const int n = qMin(data.size(), maxBytes);
    QString s;
    s.reserve(n);
    for (int i = 0; i < n; ++i) {
        const uchar c = uchar(data[i]);
        s.append((c >= 32 && c < 127) ? QChar(c) : QChar('.'));
    }
    if (data.size() > n)
        s.append(QStringLiteral(" …"));
    return s;
}

QString toHexDump(const QByteArray &data)
{
    QString out;
    for (int i = 0; i < data.size(); i += 16) {
        out.append(QStringLiteral("%1  ").arg(i, 4, 16, QChar('0')).toUpper());
        QString ascii;
        for (int j = 0; j < 16; ++j) {
            if (i + j < data.size()) {
                const uchar c = uchar(data[i + j]);
                out.append(QStringLiteral("%1 ").arg(c, 2, 16, QChar('0')).toUpper());
                if (j == 7)
                    out.append(QLatin1Char(' '));
                ascii.append((c >= 32 && c < 127) ? QChar(c) : QChar('.'));
            } else {
                out.append(QStringLiteral("   "));
                if (j == 7)
                    out.append(QLatin1Char(' '));
            }
        }
        out.append(QLatin1Char(' '));
        out.append(ascii);
        out.append(QLatin1Char('\n'));
    }
    return out;
}

QString csvField(const QString &text)
{
    if (text.contains(QLatin1Char('"')) || text.contains(QLatin1Char(',')) || text.contains(QLatin1Char('\n')))
        return QLatin1Char('"') + QString(text).replace(QLatin1Char('"'), QStringLiteral("\"\"")) + QLatin1Char('"');
    return text;
}

CaptureModel::CaptureModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int CaptureModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

int CaptureModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 8;
}

QVariant CaptureModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const CaptureRow &row = m_rows.at(index.row());
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case 0: return row.seq;
        case 1: return row.timeText;
        case 2: return row.port;
        case 3: return row.direction;
        case 4: return row.length;
        case 5: return row.hexPreview;
        case 6: return row.asciiPreview;
        case 7: return row.summary;
        default: break;
        }
    } else if (role == Qt::ForegroundRole) {
        if (index.column() == 5)
            return QColor(QStringLiteral("#7fc466"));
        return QColor(row.outbound ? QStringLiteral("#f5b04a") : QStringLiteral("#4da6ff"));
    } else if (role == Qt::FontRole && (index.column() == 5 || index.column() == 6)) {
        return monoFont();
    } else if (role == Qt::ToolTipRole && index.column() == 5) {
        return toHexPreview(row.data, 256);
    }
    return {};
}

QVariant CaptureModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    static const char *headers[] = {"序号", "时间", "端口", "方向", "长度", "HEX", "ASCII", "摘要"};
    if (section >= 0 && section < 8)
        return QString::fromUtf8(headers[section]);
    return {};
}

void CaptureModel::appendMany(const QVector<CaptureRow> &rows)
{
    if (rows.isEmpty())
        return;
    const int first = m_rows.size();
    beginInsertRows({}, first, first + rows.size() - 1);
    m_rows += rows;
    endInsertRows();
    trim();
}

void CaptureModel::trim()
{
    if (m_rows.size() <= kMaxRows)
        return;
    const int drop = m_rows.size() - kMaxRows;
    beginRemoveRows({}, 0, drop - 1);
    m_rows.remove(0, drop);
    endRemoveRows();
}

void CaptureModel::clear()
{
    beginResetModel();
    m_rows.clear();
    endResetModel();
}

const CaptureRow *CaptureModel::at(int row) const
{
    if (row < 0 || row >= m_rows.size())
        return nullptr;
    return &m_rows.at(row);
}

ScsiModel::ScsiModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int ScsiModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

int ScsiModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 11;
}

QVariant ScsiModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const ScsiRow &row = m_rows.at(index.row());
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case 0: return row.seq;
        case 1: return row.timeText;
        case 2: return row.device;
        case 3: return row.tag;
        case 4: return row.lun;
        case 5: return row.name;
        case 6: return row.lba;
        case 7: return row.blocks;
        case 8: return row.status;
        case 9: return row.duration;
        case 10: return row.summary;
        default: break;
        }
    } else if (role == Qt::ForegroundRole) {
        if (index.column() == 5 || index.column() == 8) {
            if (row.statusCode == 0)
                return QColor(QStringLiteral("#58c27a"));
            if (row.statusCode == 1 || row.statusCode == 2)
                return QColor(QStringLiteral("#ef6a6a"));
            return QColor(QStringLiteral("#f5b04a"));
        }
    } else if (role == Qt::FontRole && index.column() == 3) {
        return monoFont();
    }
    return {};
}

QVariant ScsiModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    static const char *headers[] = {"序号", "时间", "设备", "Tag", "LUN", "命令", "LBA", "块数", "状态", "耗时ms", "摘要"};
    if (section >= 0 && section < 11)
        return QString::fromUtf8(headers[section]);
    return {};
}

int ScsiModel::indexOf(qint64 seq) const
{
    for (int i = m_rows.size() - 1; i >= 0; --i) {
        if (m_rows.at(i).seq == seq)
            return i;
    }
    return -1;
}

void ScsiModel::upsert(const ScsiRow &row)
{
    const int found = indexOf(row.seq);
    if (found < 0) {
        const int at = m_rows.size();
        beginInsertRows({}, at, at);
        m_rows.append(row);
        endInsertRows();
        return;
    }
    m_rows[found] = row;
    emit dataChanged(index(found, 0), index(found, columnCount() - 1));
}

void ScsiModel::clear()
{
    beginResetModel();
    m_rows.clear();
    endResetModel();
}

const ScsiRow *ScsiModel::at(int row) const
{
    if (row < 0 || row >= m_rows.size())
        return nullptr;
    return &m_rows.at(row);
}

bool CaptureFilter::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    Q_UNUSED(sourceParent);
    if (!state || !rows)
        return true;
    const CaptureRow *row = rows->at(sourceRow);
    if (!row)
        return false;
    if (state->source == 1 && row->usb)
        return false;
    if (state->source == 2 && !row->usb)
        return false;
    if (!state->port.isEmpty() && row->port != state->port)
        return false;
    return containsText(state->text, {row->port, row->direction, row->summary, row->hexPreview, row->asciiPreview});
}

bool ScsiFilter::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    Q_UNUSED(sourceParent);
    if (!state || !rows)
        return true;
    const ScsiRow *row = rows->at(sourceRow);
    if (!row)
        return false;
    if (state->source == 1)
        return false;
    if (!state->port.isEmpty() && state->port != row->device && !state->port.startsWith(row->device + QLatin1Char('.')))
        return false;
    return containsText(state->text, {row->device, row->name, row->status, row->summary, row->lba, row->tag});
}
