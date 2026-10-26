#include "Resampler.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace {

// 深度方向的分列方案：把 [iMin, iMax] 均分成 cols 段，每段聚合成一个值
struct Cols {
    bool ok = false;
    int  iMin = 0, iMax = 0, n = 0, cols = 0;
    QVector<int>    i0, i1;
    QVector<double> depths;
};

Cols makeCols(const DepthInfo &di, const ResampleParams &p)
{
    Cols cs;
    auto toIdx = [&](double d) { return int(std::lround((d - di.start) / di.step)); };
    cs.iMin = std::max(0, toIdx(p.depthMin));
    cs.iMax = std::min(di.count - 1, toIdx(p.depthMax));
    cs.n    = cs.iMax - cs.iMin + 1;
    if (cs.n < 2)
        return cs;
    cs.cols = std::min(p.cols, cs.n);
    cs.i0.resize(cs.cols);
    cs.i1.resize(cs.cols);
    cs.depths.resize(cs.cols);
    for (int c = 0; c < cs.cols; ++c) {
        cs.i0[c] = cs.iMin + int(qint64(c) * cs.n / cs.cols);
        cs.i1[c] = cs.iMin + int(qint64(c + 1) * cs.n / cs.cols);      // 开区间，至少含 1 个点
        cs.depths[c] = di.start + di.step * 0.5 * (cs.i0[c] + cs.i1[c] - 1);
    }
    cs.ok = true;
    return cs;
}

// 一帧在深度方向聚合成 cols 个值
void aggFrame(const float *src, const Cols &cs, AggMode agg, float *dst)
{
    for (int c = 0; c < cs.cols; ++c) {
        const int a = cs.i0[c], b = cs.i1[c];
        float val;
        switch (agg) {
        case AggMode::Max:
            val = src[a];
            for (int i = a + 1; i < b; ++i) val = std::max(val, src[i]);
            break;
        case AggMode::Min:
            val = src[a];
            for (int i = a + 1; i < b; ++i) val = std::min(val, src[i]);
            break;
        default: {
            float s = 0;
            for (int i = a; i < b; ++i) s += src[i];
            val = s / float(b - a);
            break;
        }
        }
        dst[c] = val;
    }
}

} // namespace

Grid Resampler::run(const FrameBuffer *buf, ResampleParams p)
{
    Grid g;
    const DepthInfo di = buf->depth();
    if (p.rows < 2 || p.cols < 2 || p.windowMs <= 0 || di.count < 2)
        return g;
    const Cols cs = makeCols(di, p);
    if (!cs.ok)
        return g;
    const int cols = cs.cols;

    // =====================================================================
    // 稀疏数据（窗口内帧数不超过行数上限）：一帧一行，用帧自己的时间戳。
    // 帧间隔不均匀（如 7~9 秒一帧）时，不做时间分箱，避免出现空箱/重复行。
    // =====================================================================
    {
        const QVector<Frame> fr = buf->range(p.tEnd + 1 - p.windowMs, p.tEnd + 1);
        QVector<const Frame *> use;
        use.reserve(fr.size());
        for (const Frame &f : fr)
            if (f.v.size() > cs.iMax)                         // 帧长度不对的丢弃
                use.append(&f);
        if (use.size() <= p.rows) {
            if (use.size() < 2)
                return g;
            g.rows = use.size();
            g.cols = cols;
            g.depths = cs.depths;
            g.z.resize(g.rows * cols);
            g.times.resize(g.rows);
            float *z = g.z.data();
            for (int r = 0; r < g.rows; ++r) {
                aggFrame(use[r]->v.constData(), cs, p.agg, z + r * cols);
                g.times[r] = use[r]->tMs;
            }
            g.tStart = g.times.first();
            g.tEnd   = g.times.last();
            auto mm = std::minmax_element(g.z.constBegin(), g.z.constEnd());
            g.vmin = *mm.first;
            g.vmax = *mm.second;
            return g;
        }
    }

    // =====================================================================
    // 密集数据（帧数多于行数上限）：按时间分箱聚合。
    // 箱宽取帧间隔的整数倍，边界对齐绝对时间网格，窗口滚动时画面不抖。
    // =====================================================================
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

    QVector<float> acc(rows * cols);
    QVector<int>   cnt(rows, 0);
    QVector<float> tmp(cols);
    float *accp = acc.data();

    for (const Frame &f : frames) {
        if (f.v.size() <= cs.iMax)
            continue;
        int r = int((f.tMs - tStart) / binMs);
        r = std::min(std::max(r, 0), rows - 1);
        aggFrame(f.v.constData(), cs, p.agg, tmp.data());
        float *dst = accp + r * cols;
        const float *v = tmp.constData();
        if (cnt[r] == 0) {
            std::memcpy(dst, v, sizeof(float) * cols);
        } else {
            for (int c = 0; c < cols; ++c) {
                if (p.agg == AggMode::Max)      dst[c] = std::max(dst[c], v[c]);
                else if (p.agg == AggMode::Min) dst[c] = std::min(dst[c], v[c]);
                else                            dst[c] += v[c];      // Mean：先累加，最后除帧数
            }
        }
        ++cnt[r];
    }

    if (p.agg == AggMode::Mean) {
        for (int r = 0; r < rows; ++r)
            if (cnt[r] > 1)
                for (int c = 0; c < cols; ++c) accp[r * cols + c] /= float(cnt[r]);
    }

    // 空箱（数据中断）：沿用上一行；开头的空箱用第一个有数据的行回填
    int firstValid = 0;
    while (firstValid < rows && cnt[firstValid] == 0) ++firstValid;
    if (firstValid == rows)
        return g;
    for (int r = 0; r < firstValid; ++r)
        std::memcpy(accp + r * cols, accp + firstValid * cols, sizeof(float) * cols);
    for (int r = firstValid + 1; r < rows; ++r)
        if (cnt[r] == 0)
            std::memcpy(accp + r * cols, accp + (r - 1) * cols, sizeof(float) * cols);

    g.rows = rows;
    g.cols = cols;
    g.depths = cs.depths;
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
