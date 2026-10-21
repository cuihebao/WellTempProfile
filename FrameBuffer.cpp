#include "FrameBuffer.h"
#include <QMutexLocker>
#include <algorithm>

FrameBuffer::FrameBuffer(const DepthInfo &depth, qint64 retainMs)
    : depth_(depth), retainMs_(retainMs)
{
}

void FrameBuffer::append(const Frame &f)
{
    QMutexLocker lock(&mutex_);
    frames_.push_back(f);
    trimLocked();
}

void FrameBuffer::setRetainMs(qint64 ms)
{
    QMutexLocker lock(&mutex_);
    retainMs_ = ms;
    trimLocked();
}

void FrameBuffer::trimLocked()
{
    while (frames_.size() > 1 &&
           frames_.back().tMs - frames_.front().tMs > retainMs_) {
        frames_.pop_front();
    }
}

QVector<Frame> FrameBuffer::range(qint64 t0, qint64 t1) const
{
    QVector<Frame> out;
    QMutexLocker lock(&mutex_);
    // 帧按时间递增追加，二分定位起点；QVector<float> 隐式共享，拷贝很便宜
    auto it = std::lower_bound(frames_.begin(), frames_.end(), t0,
                               [](const Frame &f, qint64 t) { return f.tMs < t; });
    for (; it != frames_.end() && it->tMs < t1; ++it)
        out.append(*it);
    return out;
}

bool FrameBuffer::latestTime(qint64 *t) const
{
    QMutexLocker lock(&mutex_);
    if (frames_.empty())
        return false;
    *t = frames_.back().tMs;
    return true;
}

int FrameBuffer::size() const
{
    QMutexLocker lock(&mutex_);
    return int(frames_.size());
}

qint64 FrameBuffer::frameIntervalMs() const
{
    QMutexLocker lock(&mutex_);
    const size_t n = std::min<size_t>(frames_.size(), 16);
    if (n < 2)
        return 0;
    return (frames_.back().tMs - frames_[frames_.size() - n].tMs) / qint64(n - 1);
}
