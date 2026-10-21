#pragma once
#include "Config.h"
#include "FrameBuffer.h"
#include <QVector>

struct ResampleParams {
    qint64  tEnd      = 0;        // 窗口右端（通常取最新一帧的时间）
    qint64  windowMs  = 600000;   // 时间窗口长度
    int     rows      = 300;      // 时间方向最大行数
    int     cols      = 700;      // 深度方向最大列数
    double  depthMin  = 0;
    double  depthMax  = 0;
    AggMode agg       = AggMode::Mean;
};

// 降采样后的显示网格：rows(时间) × cols(深度)
struct Grid {
    int              rows = 0, cols = 0;
    QVector<float>   z;          // rows*cols，行优先：z[r*cols + c]
    QVector<double>  depths;     // 每列的深度（m）
    QVector<qint64>  times;      // 每行的时间（毫秒，时间箱中心）
    qint64           tStart = 0, tEnd = 0;
    float            vmin = 0, vmax = 0;

    bool isEmpty() const { return rows < 2 || cols < 2; }
};

class Resampler
{
public:
    // 线程安全（FrameBuffer 内部加锁），可直接丢到 QtConcurrent 里跑。
    // 时间箱边界对齐到绝对时间网格，窗口滚动时已有的箱子不会漂移，画面不抖。
    static Grid run(const FrameBuffer *buf, ResampleParams p);
};
