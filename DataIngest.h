#pragma once
#include "Config.h"
#include "FrameBuffer.h"
#include <QAtomicInt>
#include <QDateTime>
#include <QMutex>
#include <QObject>
#include <QVector>

// 真实数据接入口：把“一帧温度数组 + 时间戳 + 点间隔”校验、清洗后写进 FrameBuffer。
// 线程安全：pushFrame*() 可以直接在设备/网络线程里调用，不需要切到界面线程。
//
// 第一帧到达（或点数/点间隔/起始井深发生变化）时，会按数据自带的结构重置缓冲，
// 并发出 depthInfoChanged，界面据此更新井深范围。
class DataIngest : public QObject
{
    Q_OBJECT
public:
    explicit DataIngest(FrameBuffer *buf, double startDepth = 0.0, QObject *parent = nullptr);

    void setStartDepth(double depthM);                 // 第 0 个点对应的井深（m）
    void setValidRange(double lo, double hi);          // 有效温度范围（℃）；范围外/非数值的点视为无效，默认 -100 ~ 300

    int acceptedCount() const { return accepted_.loadAcquire(); }
    int rejectedCount() const { return rejected_.loadAcquire(); }

public slots:
    // temp: 整条井筒的温度数组；tsMs: 毫秒时间戳（自 1970 epoch）；
    // spatialResolution: 相邻两点的间隔（m）。返回是否被接收。
    bool pushFrameMs(const QVector<float> &temp, qint64 tsMs, double spatialResolution);

public:
    bool pushFrame(const QVector<float> &temp, const QDateTime &ts, double spatialResolution)
    {
        return pushFrameMs(temp, ts.toMSecsSinceEpoch(), spatialResolution);
    }

signals:
    void depthInfoChanged(DepthInfo info);             // 跨线程时自动走队列连接

private:
    bool reject(const char *why);

    FrameBuffer *buf_;
    QMutex       mutex_;
    double       startDepth_;
    double       validLo_ = -100.0, validHi_ = 300.0;
    qint64       lastTs_  = 0;
    QAtomicInt   accepted_, rejected_;
};
