#pragma once
#include "FrameBuffer.h"
#include "Resampler.h"
#include <QString>

struct RawExportJob {
    const FrameBuffer *buf = nullptr;
    qint64  t0 = 0, t1 = 0;        // 时间范围 [t0, t1]（毫秒）
    double  depthMin = 0, depthMax = 0;
    QString path;
};

// CSV 导出（UTF-8 带 BOM，Excel 直接打开中文不乱码）。
// 返回错误信息，空字符串表示成功。设计成静态函数，方便丢到 QtConcurrent 里在后台跑。
class DataExporter
{
public:
    // 当前显示网格：首行=井深，首列=时间
    static QString writeGridCsv(Grid g, QString path);
    // 当前时间/深度范围内的原始数据（全分辨率）
    static QString writeRawCsv(RawExportJob job);
};
