#pragma once
#include "FrameBuffer.h"
#include <QObject>
#include <QTimer>
#include <random>

// 研发阶段用的数据模拟器：每个 tick 往 FrameBuffer 里塞一帧。
// 接入真实数据后，把真实数据到达的回调里调用 FrameBuffer::append() 即可，本类可整体删除。
class DataSimulator : public QObject
{
    Q_OBJECT
public:
    DataSimulator(FrameBuffer *buf, QObject *parent = nullptr);

    // 先生成过去 seconds 秒的数据，让界面一启动就有图
    void prefill(int seconds, int stepMs = 2000);
    // intervalMs: 真实定时器周期；stepMs: 每帧代表的“模拟时间”。
    // 调试时可 start(200, 2000) 把时间加速 10 倍。
    void start(int intervalMs = 2000, int stepMs = 2000);

private slots:
    void tick();

private:
    Frame makeFrame(qint64 tMs);

    FrameBuffer  *buf_;
    QTimer        timer_;
    qint64        simNowMs_ = 0;
    qint64        t0Ms_     = 0;
    int           stepMs_   = 2000;
    std::mt19937  rng_;
};
