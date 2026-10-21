#pragma once
#include "Config.h"
#include "Resampler.h"
#include <QImage>
#include <QPixmap>
#include <QWidget>

// 3D 曲面（SurfaceView）和 2D 热力图（HeatmapView）的公共接口，MainWindow 只面向它编程。
class ProfileView : public QWidget
{
    Q_OBJECT
public:
    explicit ProfileView(QWidget *parent = nullptr) : QWidget(parent) {}

    virtual void applyConfig(const ViewConfig &cfg) = 0;
    virtual void setBaseTimeMs(qint64 baseMs) = 0;
    virtual void setGrid(const Grid &g) = 0;
    virtual void setColorBarVisible(bool visible) = 0;
    // 导出图片用：3D 视图的色带需要单独截图再由调用方拼到右侧；
    // 2D 热力图的色带已经包含在 renderImage() 里，这里返回空。
    virtual QPixmap grabColorBar() const = 0;
    virtual QImage renderImage(const QSize &size = QSize(), int msaaSamples = 4) = 0;

    // 最近一次点选的位置，用作范围缩放的中心
    bool   hasPick() const    { return pickValid_; }
    double pickDepth() const  { return pickDepth_; }
    qint64 pickTimeMs() const { return pickTimeMs_; }

signals:
    void pointPicked(double depth, qint64 timeMs, double temp);
    void pointCleared();
    // 缩放：amount 为范围缩放系数（<1 放大，>1 缩小）
    void navRequested(NavAction action, double amount);

protected:
    bool   pickValid_  = false;
    double pickDepth_  = 0;
    qint64 pickTimeMs_ = 0;
};
