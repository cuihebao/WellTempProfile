#pragma once
#include "ProfileView.h"
#include <QtDataVisualization/Q3DSurface>
#include <QtDataVisualization/QSurface3DSeries>
#include <QtDataVisualization/QSurfaceDataProxy>
#include <QtDataVisualization/QValue3DAxis>

class TimeAxisFormatter;
class ColorBar;

// 3D 温度曲面：X=井深(m)  Y=温度(℃，竖直方向)  Z=时间
// 内置交互（Qt 提供）：左键拖动旋转、滚轮缩放视角、点击选点。
// 本类额外提供：Shift/Ctrl+滚轮缩放井深/时间范围（见 navRequested），以及右侧的色带。
class SurfaceView : public ProfileView
{
    Q_OBJECT
public:
    explicit SurfaceView(QWidget *parent = nullptr);

    void applyConfig(const ViewConfig &cfg) override;
    void setBaseTimeMs(qint64 baseMs) override;     // 时间轴数值 = (时间 - baseMs) 秒
    void setGrid(const Grid &g) override;
    void setColorBarVisible(bool visible) override;
    QPixmap grabColorBar() const override;  // 色带当前的截图（隐藏时返回空）

    // 把当前 3D 画面渲染成图片（size 为空则用窗口大小）
    QImage renderImage(const QSize &size = QSize(), int msaaSamples = 4) override;

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override;

private slots:
    void onSelectedPointChanged(const QPoint &p);

private:
    QtDataVisualization::Q3DSurface        *graph_;
    QtDataVisualization::QSurface3DSeries  *series_;
    QtDataVisualization::QSurfaceDataProxy *proxy_;
    QtDataVisualization::QValue3DAxis      *axisDepth_;
    QtDataVisualization::QValue3DAxis      *axisTemp_;
    QtDataVisualization::QValue3DAxis      *axisTime_;
    ColorBar                               *colorBar_;
    ViewConfig cfg_;
    qint64     baseMs_ = 0;

    bool   ignoreSel_  = false;     // 程序自己清除选中时，不当作用户取消选择
};
