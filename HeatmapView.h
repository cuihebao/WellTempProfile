#pragma once
#include "ProfileView.h"
#include <QPoint>
#include <QPointF>

class ColorBar;

// 2D 热力图（waterfall）：横轴=井深，纵轴=时间（上旧下新），颜色=温度。
// 纯 CPU 绘制，不依赖 OpenGL，没有独显时也很流畅，适合长时间范围浏览。
// 交互：悬停显示读数；左键点选（作为缩放中心并在图上标记），右键取消；
//       Shift/Ctrl+滚轮缩放井深/时间范围，以鼠标位置为中心。
class HeatmapView : public ProfileView
{
    Q_OBJECT
public:
    explicit HeatmapView(QWidget *parent = nullptr);

    void applyConfig(const ViewConfig &cfg) override;
    void setBaseTimeMs(qint64) override {}
    void setGrid(const Grid &g) override;
    void setColorBarVisible(bool visible) override;
    QPixmap grabColorBar() const override { return QPixmap(); }   // 色带已包含在 renderImage 里
    QImage renderImage(const QSize &size = QSize(), int msaaSamples = 4) override;

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void leaveEvent(QEvent *) override;
    void wheelEvent(QWheelEvent *e) override;

private:
    QRect plotRect() const;
    bool  cellAt(const QPoint &pos, int *row, int *col) const;
    bool  pickAt(const QPoint &pos);              // 成功则更新点选并发出 pointPicked
    void  updateRange();                          // 计算颜色范围并同步色带
    void  rebuildImage();
    QPointF toPixel(double depth, qint64 timeMs) const;

    ViewConfig  cfg_;
    Grid        grid_;
    QImage      img_;
    ColorBar   *colorBar_;
    double      lo_ = -20, hi_ = 60;              // 当前颜色映射范围
    double      tMin_ = 0, tMax_ = 1;             // 纵轴时间范围（毫秒）：首/末行各向外延半个行距
    QPoint      hover_;
    bool        hoverValid_ = false;
    bool        rendering_  = false;              // 导出渲染时不画悬停框
};
