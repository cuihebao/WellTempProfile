#include "DataIngest.h"
#include <QDebug>
#include <QMutexLocker>
#include <cmath>

DataIngest::DataIngest(FrameBuffer *buf, double startDepth, QObject *parent)
    : QObject(parent), buf_(buf), startDepth_(startDepth)
{
}

void DataIngest::setStartDepth(double depthM)
{
    QMutexLocker lock(&mutex_);
    startDepth_ = depthM;       // 下一帧到达时发现结构变化，会自动重置缓冲
}

void DataIngest::setValidRange(double lo, double hi)
{
    QMutexLocker lock(&mutex_);
    validLo_ = lo;
    validHi_ = hi;
}

bool DataIngest::reject(const char *why)
{
    const int n = rejected_.fetchAndAddRelaxed(1) + 1;
    if (n <= 5 || n % 100 == 0)                       // 避免设备持续出错时刷屏
        qWarning("DataIngest: 丢弃数据帧（累计 %d 帧）：%s", n, why);
    return false;
}

bool DataIngest::pushFrameMs(const QVector<float> &temp, qint64 tsMs, double res)
{
    if (temp.size() < 2)
        return reject("温度数组为空或点数不足");
    if (!(res > 0.0) || !std::isfinite(res))
        return reject("点间隔 spatialResolution 无效");
    if (tsMs <= 0)
        return reject("时间戳无效");

    DepthInfo want;
    double lo, hi;
    {
        QMutexLocker lock(&mutex_);
        if (tsMs <= lastTs_)
            return reject("时间戳没有递增（重复或乱序）");
        lastTs_    = tsMs;
        want.start = startDepth_;
        lo = validLo_;
        hi = validHi_;
    }
    want.step  = res;
    want.count = temp.size();

    // ---- 清洗无效点：非数值/超出有效范围（如 -999）的点，用同一帧里前一个有效值填充 ----
    QVector<float> v = temp;                          // 隐式共享，没有无效点时不会真正拷贝
    const float *src = v.constData();
    auto ok = [&](float x) { return std::isfinite(x) && x >= lo && x <= hi; };
    int firstValid = -1;
    bool dirty = false;
    for (int i = 0; i < v.size(); ++i) {
        if (ok(src[i])) {
            if (firstValid < 0) firstValid = i;
        } else {
            dirty = true;
        }
    }
    if (firstValid < 0)
        return reject("整帧都是无效值");
    if (dirty) {
        float *d = v.data();                          // 这里才会发生拷贝
        float last = d[firstValid];
        for (int i = 0; i < v.size(); ++i) {
            if (ok(d[i])) last = d[i];
            else          d[i] = last;
        }
    }

    // ---- 结构（起始井深/点间隔/点数）变化：重置缓冲并通知界面 ----
    const DepthInfo cur = buf_->depth();
    if (cur.count != want.count || std::fabs(cur.step - want.step) > 1e-9
            || std::fabs(cur.start - want.start) > 1e-9) {
        buf_->reset(want);
        emit depthInfoChanged(want);
    }

    Frame f;
    f.tMs = tsMs;
    f.v   = v;
    if (!buf_->append(f))
        return reject("时间戳不比缓冲中最新一帧大（是否还有别的数据源在写入？例如模拟器没关）");
    accepted_.fetchAndAddRelaxed(1);
    return true;
}
