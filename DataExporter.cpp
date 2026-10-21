#include "DataExporter.h"
#include <QDateTime>
#include <QFile>
#include <QObject>
#include <algorithm>
#include <cmath>

namespace {
const char kBom[] = "\xEF\xBB\xBF";

QByteArray timeStr(qint64 ms)
{
    return QDateTime::fromMSecsSinceEpoch(ms)
            .toString(QLatin1String("yyyy-MM-dd HH:mm:ss.zzz")).toUtf8();
}
}

QString DataExporter::writeGridCsv(Grid g, QString path)
{
    if (g.isEmpty())
        return QObject::tr("没有可导出的数据");
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return QObject::tr("无法写入文件：%1").arg(path);

    f.write(kBom, 3);
    QByteArray line = QObject::tr("时间\\井深(m)").toUtf8();
    for (int c = 0; c < g.cols; ++c) {
        line += ',';
        line += QByteArray::number(g.depths[c], 'f', 2);
    }
    line += '\n';
    f.write(line);

    for (int r = 0; r < g.rows; ++r) {
        line = timeStr(g.times[r]);
        const float *src = g.z.constData() + r * g.cols;
        for (int c = 0; c < g.cols; ++c) {
            line += ',';
            line += QByteArray::number(double(src[c]), 'f', 3);
        }
        line += '\n';
        f.write(line);
    }
    return QString();
}

QString DataExporter::writeRawCsv(RawExportJob j)
{
    const DepthInfo di = j.buf->depth();
    const QVector<Frame> frames = j.buf->range(j.t0, j.t1 + 1);
    if (frames.isEmpty())
        return QObject::tr("当前时间范围内没有原始数据（可能已超出缓冲保留时间）");

    const int iMin = std::max(0, int(std::lround((j.depthMin - di.start) / di.step)));
    const int iMax = std::min(di.count - 1, int(std::lround((j.depthMax - di.start) / di.step)));
    if (iMax < iMin)
        return QObject::tr("深度范围无效");

    QFile f(j.path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return QObject::tr("无法写入文件：%1").arg(j.path);

    f.write(kBom, 3);
    QByteArray line = QObject::tr("时间\\井深(m)").toUtf8();
    for (int i = iMin; i <= iMax; ++i) {
        line += ',';
        line += QByteArray::number(di.start + di.step * i, 'f', 1);
    }
    line += '\n';
    f.write(line);

    for (const Frame &fr : frames) {
        if (fr.v.size() <= iMax)
            continue;
        line = timeStr(fr.tMs);
        const float *src = fr.v.constData();
        for (int i = iMin; i <= iMax; ++i) {
            line += ',';
            line += QByteArray::number(double(src[i]), 'f', 3);
        }
        line += '\n';
        f.write(line);
    }
    return QString();
}
