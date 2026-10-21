#include "Resampler.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

Grid Resampler::run(const FrameBuffer *buf, ResampleParams p)
{
    Grid g;
    const DepthInfo di = buf->depth();
    if (p.rows < 2 || p.cols < 2 || p.windowMs <= 0 || di.count < 2)
        return g;

    // ---- 时间箱：对齐到绝对时间网格 ----
    // 时间箱宽度取帧间隔的整数倍（至少 1 个帧间隔）：每个箱里恰好有整数帧，
    // 时间范围放得很大或很小都不会出现“空箱”。窗口很短时行数自动减少。
    qint64 binMs = std::max<qint64>(1, p.windowMs / p.rows);
    const qint64 dt = buf->frameIntervalMs();
    if (dt > 0)
        binMs = std::max<qint64>(dt, ((binMs + dt / 2) / dt) * dt);
    const int maxRows = int(std::max<qint64>(2, p.windowMs / binMs));
    const qint64 tEndA  = (p.tEnd / binMs + 1) * binMs;
    qint64       tStart = tEndA - qint64(maxRows) * binMs;

    const QVector<Frame> frames = buf->range(tStart, tEndA);
    if (frames.isEmpty())
        return g;

    // 数据还没填满窗口时，只显示有数据的那部分（不做假数据填充）
    const int firstBin = int((frames.first().tMs - tStart) / binMs);
    const int rows     = maxRows - firstBin;
    if (rows < 2)
        return g;
    tStart += qint64(firstBin) * binMs;

    // ---- 深度列：把 [iMin, iMax] 均分成 cols 段 ----
    auto toIdx = [&](double d) { return int(std::lround((d - di.start) / di.step)); };
    const int iMin = std::max(0, toIdx(p.depthMin));
    const int iMax = std::min(di.count - 1, toIdx(p.depthMax));
    const int n    = iMax - iMin + 1;
    if (n < 2)
        return g;
    const int cols = std::min(p.cols, n);

    QVector<int> i0(cols), i1(cols);
    QVector<double> depths(cols);
    for (int c = 0; c < cols; ++c) {
        i0[c] = iMin + int(qint64(c) * n / cols);
        i1[c] = iMin + int(qint64(c + 1) * n / cols);          // 开区间，至少含 1 个点
        depths[c] = di.start + di.step * 0.5 * (i0[c] + i1[c] - 1);
    }

    // ---- 逐帧聚合：先在深度方向聚合，再累积到所属时间箱 ----
    QVector<float> acc(rows * cols);
    QVector<int>   cnt(rows, 0);
    float *accp = acc.data();

    for (const Frame &f : frames) {
        if (f.v.size() <= iMax)
            continue;                                           // 帧长度不对，丢弃
        int r = int((f.tMs - tStart) / binMs);
        r = std::min(std::max(r, 0), rows - 1);
        const float *src = f.v.constData();
        float *dst = accp + r * cols;
        const bool first = (cnt[r] == 0);

        for (int c = 0; c < cols; ++c) {
            const int a = i0[c], b = i1[c];
            float val;
            switch (p.agg) {
            case AggMode::Max: {
                val = src[a];
                for (int i = a + 1; i < b; ++i) val = std::max(val, src[i]);
                break;
            }
            case AggMode::Min: {
                val = src[a];
                for (int i = a + 1; i < b; ++i) val = std::min(val, src[i]);
                break;
            }
            default: {
                float s = 0;
                for (int i = a; i < b; ++i) s += src[i];
                val = s / float(b - a);
                break;
            }
            }
            if (first)                    dst[c] = val;
            else if (p.agg == AggMode::Max) dst[c] = std::max(dst[c], val);
            else if (p.agg == AggMode::Min) dst[c] = std::min(dst[c], val);
            else                          dst[c] += val;        // Mean：先累加，最后除帧数
        }
        ++cnt[r];
    }

    if (p.agg == AggMode::Mean) {
        for (int r = 0; r < rows; ++r)
            if (cnt[r] > 1)
                for (int c = 0; c < cols; ++c) accp[r * cols + c] /= float(cnt[r]);
    }

    // ---- 空箱（数据中断）：沿用上一行；开头的空箱用第一个有数据的行回填 ----
    int firstValid = 0;
    while (firstValid < rows && cnt[firstValid] == 0) ++firstValid;
    if (firstValid == rows)
        return g;
    for (int r = 0; r < firstValid; ++r)
        std::memcpy(accp + r * cols, accp + firstValid * cols, sizeof(float) * cols);
    for (int r = firstValid + 1; r < rows; ++r)
        if (cnt[r] == 0)
            std::memcpy(accp + r * cols, accp + (r - 1) * cols, sizeof(float) * cols);

    // ---- 输出 ----
    g.rows = rows;
    g.cols = cols;
    g.depths = depths;
    g.times.resize(rows);
    for (int r = 0; r < rows; ++r)
        g.times[r] = tStart + qint64(r) * binMs + binMs / 2;
    g.tStart = tStart;
    g.tEnd   = tEndA;
    auto mm = std::minmax_element(acc.constBegin(), acc.constEnd());
    g.vmin = *mm.first;
    g.vmax = *mm.second;
    g.z = acc;
    return g;
}
