#pragma once
#include "Config.h"
#include "Resampler.h"
#include <QWidget>
#include <QtDataVisualization/Q3DSurface>
#include <QtDataVisualization/QSurface3DSeries>
#include <QtDataVisualization/QSurfaceDataProxy>
#include <QtDataVisualization/QValue3DAxis>

class TimeAxisFormatter;

// 3D 温度曲面：X=井深(m)  Y=温度(℃，竖直方向)  Z=时间
class SurfaceView : public QWidget
{
    Q_OBJECT
public:
    explicit SurfaceView(QWidget *parent = nullptr);

    void applyConfig(const ViewConfig &cfg);
    void setBaseTimeMs(qint64 baseMs);     // 时间轴数值 = (时间 - baseMs) 秒
    void setGrid(const Grid &g);

signals:
    void pointPicked(double depth, qint64 timeMs, double temp);
    void pointCleared();

private slots:
    void onSelectedPointChanged(const QPoint &p);

private:
    QtDataVisualization::Q3DSurface        *graph_;
    QtDataVisualization::QSurface3DSeries  *series_;
    QtDataVisualization::QSurfaceDataProxy *proxy_;
    QtDataVisualization::QValue3DAxis      *axisDepth_;
    QtDataVisualization::QValue3DAxis      *axisTemp_;
    QtDataVisualization::QValue3DAxis      *axisTime_;
    ViewConfig cfg_;
    qint64     baseMs_ = 0;
};
