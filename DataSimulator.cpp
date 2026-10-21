#include "DataSimulator.h"
#include <QDateTime>
#include <cmath>

DataSimulator::DataSimulator(FrameBuffer *buf, QObject *parent)
    : QObject(parent), buf_(buf), rng_(12345)
{
    connect(&timer_, &QTimer::timeout, this, &DataSimulator::tick);
}

void DataSimulator::prefill(int seconds, int stepMs)
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    stepMs_   = stepMs;
    t0Ms_     = now - qint64(seconds) * 1000;
    simNowMs_ = t0Ms_;
    while (simNowMs_ < now) {
        buf_->append(makeFrame(simNowMs_));
        simNowMs_ += stepMs_;
    }
}

void DataSimulator::start(int intervalMs, int stepMs)
{
    stepMs_ = stepMs;
    if (simNowMs_ == 0) {
        t0Ms_ = simNowMs_ = QDateTime::currentMSecsSinceEpoch();
    }
    timer_.start(intervalMs);
}

void DataSimulator::tick()
{
    buf_->append(makeFrame(simNowMs_));
    simNowMs_ += stepMs_;
}

// 造一个大致像参考图的温度场：地温梯度 + 自井口向下推进的注气冷却 + 一处局部扰动 + 噪声
Frame DataSimulator::makeFrame(qint64 tMs)
{
    const DepthInfo di = buf_->depth();
    Frame f;
    f.tMs = tMs;
    f.v.resize(di.count);

    std::normal_distribution<float> noise(0.0f, 0.15f);
    const double ts   = (tMs - t0Ms_) / 1000.0;                 // 模拟开始后的秒数
    const double warm = 1.0 - std::exp(-ts / 1200.0);           // 冷却逐渐加深

    float *dst = f.v.data();
    for (int i = 0; i < di.count; ++i) {
        const double d    = di.start + di.step * i;
        const double geo  = 5.0 + (d - 1500.0) * 0.022;                         // 5 ~ 49 ℃
        const double cool = -28.0 * std::exp(-(d - 1500.0) / 700.0) * warm;     // 浅部被注入流体冷却
        const double x    = (d - 1650.0) / 15.0;
        const double bump = -6.0 * std::exp(-x * x) * warm;                     // 局部异常
        dst[i] = float(geo + cool + bump) + noise(rng_);
    }
    return f;
}
