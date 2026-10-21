#pragma once
#include "Config.h"
#include <QVector>
#include <QMutex>
#include <deque>

// 一帧数据：一个时刻、整条井筒所有深度点的温度
struct Frame {
    qint64         tMs = 0;   // 时间戳（毫秒，自 epoch）
    QVector<float> v;         // size == DepthInfo::count
};

// 线程安全的环形缓冲：只保留最近 retainMs 毫秒的原始帧，超出的丢弃，内存占用恒定。
// 以后要接历史回放/落盘，只需要在 append() 处加钩子，或另写一个同接口的数据源。
class FrameBuffer
{
public:
    explicit FrameBuffer(const DepthInfo &depth, qint64 retainMs = 15 * 60 * 1000);

    void append(const Frame &f);
    void setRetainMs(qint64 ms);

    QVector<Frame> range(qint64 t0, qint64 t1) const;   // 取 [t0, t1) 内的帧
    bool  latestTime(qint64 *t) const;
    qint64 frameIntervalMs() const;                      // 按最近若干帧估计帧间隔，不足两帧返回 0
    int   size() const;
    DepthInfo depth() const { return depth_; }

private:
    void trimLocked();

    DepthInfo           depth_;
    qint64              retainMs_;
    mutable QMutex      mutex_;
    std::deque<Frame>   frames_;
};
