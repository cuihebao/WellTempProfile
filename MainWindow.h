#pragma once
#include "Config.h"
#include "FrameBuffer.h"
#include "Resampler.h"
#include <QMainWindow>
#include <QTimer>
#include <QElapsedTimer>
#include <QFutureWatcher>

class SurfaceView;
class DataSimulator;
class QLabel;
class QSpinBox;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void onRefresh();
    void onResampleFinished();
    void onWindowMinutesChanged(int minutes);
    void onPointPicked(double depth, qint64 timeMs, double temp);

private:
    void updateRetention();

    ViewConfig  cfg_;
    DepthInfo   depth_;          // 注意：必须先于 buffer_ 声明
    FrameBuffer buffer_;
    qint64      baseMs_ = 0;

    SurfaceView   *view_;
    DataSimulator *sim_;
    QLabel        *pickLabel_;
    QLabel        *statusLabel_;
    QSpinBox      *windowSpin_;

    QTimer                 refreshTimer_;
    QElapsedTimer          jobTimer_;
    QFutureWatcher<Grid>   watcher_;
};
