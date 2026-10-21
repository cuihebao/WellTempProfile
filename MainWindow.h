#pragma once
#include "Config.h"
#include "FrameBuffer.h"
#include "Resampler.h"
#include "ProfileView.h"
#include <QMainWindow>
#include <QTimer>
#include <QElapsedTimer>
#include <QFutureWatcher>

class DataSimulator;
class SurfaceView;
class HeatmapView;
class QStackedWidget;
class QComboBox;
class QCheckBox;
class QLabel;
class QSpinBox;
class QDoubleSpinBox;
class QPushButton;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void onPeriodicRefresh();
    void onResampleFinished();
    void onWindowSecChanged(int sec);
    void onDepthSpinChanged();
    void onResetDepth();
    void onFollowToggled(bool on);
    void onNavRequested(NavAction action, double amount);
    void onPointPicked(double depth, qint64 timeMs, double temp);
    void onViewModeChanged(int index);
    void openSettings();

    void exportImage();
    void exportGridCsv();
    void exportRawCsv();
    void onExportFinished();

private:
    // 重采样调度：requestResample 立即（若上一次未完成则排队一次），scheduleResample 带 150ms 防抖
    void requestResample();
    void startResample();
    void scheduleResample();

    // 范围控制
    double fullDepthMax() const { return depth_.start + depth_.step * (depth_.count - 1); }
    void applyDepthRange(double lo, double hi);
    void zoomDepth(double factor);
    bool currentTimeRange(qint64 *t0, qint64 *t1) const;
    void applyTimeRange(qint64 t0, qint64 t1);
    void zoomTime(double factor);

    void applyConfig(const ViewConfig &c);     // 应用并保存配置（设置面板调用）
    void updateRetention();
    void syncFollowButton();
    void updateStatus(qint64 resampleMs);

    ViewConfig  cfg_;
    DepthInfo   depth_;          // 注意：必须先于 buffer_ 声明
    FrameBuffer buffer_;
    qint64      baseMs_ = 0;

    // 时间视图：follow_=true 时右端跟随最新数据；否则固定在 [frozenT0_, frozenT1_]
    bool        follow_   = true;
    qint64      frozenT0_ = 0, frozenT1_ = 0;
    Grid        lastGrid_;       // 最近一次显示的网格（导出用）
    bool        pending_  = false;

    ProfileView    *view_;          // 当前显示的视图（view3d_ 或 view2d_）
    SurfaceView    *view3d_;
    HeatmapView    *view2d_;
    QStackedWidget *stack_;
    QComboBox      *modeCombo_;
    QLabel         *titleLabel_;
    QCheckBox      *colorBarChk_;
    DataSimulator  *sim_;
    QLabel         *pickLabel_;
    QLabel         *statusLabel_;
    QSpinBox       *windowSpin_;
    QDoubleSpinBox *depthMinSpin_;
    QDoubleSpinBox *depthMaxSpin_;
    QPushButton    *followBtn_;

    QTimer                 refreshTimer_;
    QTimer                 debounce_;
    QElapsedTimer          jobTimer_;
    QFutureWatcher<Grid>   watcher_;
    QFutureWatcher<QString> exportWatcher_;
    QString                lastExportPath_;
};
