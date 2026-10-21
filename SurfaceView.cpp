#include "SurfaceView.h"
#include "TimeAxisFormatter.h"
#include <QVBoxLayout>
#include <QLinearGradient>
#include <QWheelEvent>
#include <cmath>

using namespace QtDataVisualization;

SurfaceView::SurfaceView(QWidget *parent)
    : QWidget(parent)
{
    graph_ = new Q3DSurface();
    QWidget *container = QWidget::createWindowContainer(graph_, this);
    container->setMinimumSize(640, 480);
    container->setFocusPolicy(Qt::StrongFocus);

    auto *lay = new QVBoxLayout(this);
    lay->setMargin(0);
    lay->addWidget(container);

    // 坐标轴（所有权交给 graph）
    axisDepth_ = new QValue3DAxis;
    axisTemp_  = new QValue3DAxis;
    axisTime_  = new QValue3DAxis;
    graph_->setAxisX(axisDepth_);
    graph_->setAxisY(axisTemp_);
    graph_->setAxisZ(axisTime_);
    axisDepth_->setLabelFormat(QObject::tr("%.0f"));
    axisTemp_->setLabelFormat(QObject::tr("%.0f"));
    axisDepth_->setSegmentCount(8);
    axisTemp_->setSegmentCount(8);
    axisTime_->setSegmentCount(5);
    axisDepth_->setTitleVisible(true);
    axisTemp_->setTitleVisible(true);
    axisTime_->setTitleVisible(true);

    // 曲面系列 + jet 色带（色带映射到 Y 轴范围，固定温度范围时颜色含义稳定）
    proxy_  = new QSurfaceDataProxy();
    series_ = new QSurface3DSeries(proxy_);
    series_->setDrawMode(QSurface3DSeries::DrawSurface);
    series_->setFlatShadingEnabled(true);          // 平面着色计算量小，无独显时更流畅

    QLinearGradient jet;
    jet.setColorAt(0.000, QColor(0, 0, 128));
    jet.setColorAt(0.125, QColor(0, 0, 255));
    jet.setColorAt(0.375, QColor(0, 255, 255));
    jet.setColorAt(0.625, QColor(255, 255, 0));
    jet.setColorAt(0.875, QColor(255, 0, 0));
    jet.setColorAt(1.000, QColor(128, 0, 0));
    series_->setBaseGradient(jet);
    series_->setColorStyle(Q3DTheme::ColorStyleRangeGradient);
    graph_->addSeries(series_);

    // 渲染选项：无独显，关阴影；允许点选
    graph_->setShadowQuality(QAbstract3DGraph::ShadowQualityNone);
    graph_->setSelectionMode(QAbstract3DGraph::SelectionItem);
    graph_->activeTheme()->setFont(QFont(QObject::tr("Microsoft YaHei"), 10));
    graph_->scene()->activeCamera()->setCameraPreset(Q3DCamera::CameraPresetIsometricLeftHigh);

    connect(series_, &QSurface3DSeries::selectedPointChanged,
            this, &SurfaceView::onSelectedPointChanged);

    // 拦截滚轮：带 Shift/Ctrl 的滚轮用于缩放/平移数据范围，其余交给 Qt 默认的视角缩放
    graph_->installEventFilter(this);

    applyConfig(cfg_);
}

void SurfaceView::applyConfig(const ViewConfig &cfg)
{
    cfg_ = cfg;
    axisDepth_->setTitle(cfg_.depthTitle);
    axisTemp_->setTitle(cfg_.tempTitle);
    axisTime_->setTitle(cfg_.timeTitle);
    if (!cfg_.autoTemp)
        axisTemp_->setRange(float(cfg_.tempMin), float(cfg_.tempMax));
    // 时间格式变化时重建 formatter
    setBaseTimeMs(baseMs_);
}

void SurfaceView::setBaseTimeMs(qint64 baseMs)
{
    baseMs_ = baseMs;
    // setFormatter 会接管新对象并释放旧的
    axisTime_->setFormatter(new TimeAxisFormatter(baseMs_, cfg_.timeFormat));
}

void SurfaceView::setGrid(const Grid &g)
{
    if (g.isEmpty())
        return;

    auto *arr = new QSurfaceDataArray;
    arr->reserve(g.rows);
    for (int r = 0; r < g.rows; ++r) {
        auto *row = new QSurfaceDataRow(g.cols);
        const float zv = float((g.times[r] - baseMs_) / 1000.0);
        const float *src = g.z.constData() + r * g.cols;
        for (int c = 0; c < g.cols; ++c)
            (*row)[c].setPosition(QVector3D(float(g.depths[c]), src[c], zv));
        *arr << row;
    }

    // 轴范围先于数据设置
    axisDepth_->setRange(float(g.depths.first()), float(g.depths.last()));
    axisTime_->setRange(float((g.times.first() - baseMs_) / 1000.0),
                        float((g.times.last()  - baseMs_) / 1000.0));
    if (cfg_.autoTemp) {
        const float lo = std::floor((g.vmin - 2.0f) / 5.0f) * 5.0f;
        const float hi = std::ceil ((g.vmax + 2.0f) / 5.0f) * 5.0f;
        axisTemp_->setRange(lo, hi);
    }

    proxy_->resetArray(arr);    // proxy 接管 arr

    // 数据换了，原来的“第几行第几列”高亮已经对不上了：清掉高亮，
    // 但保留 pickDepth_/pickTimeMs_ 作为缩放中心（界面上的读数文字也仍然正确）
    ignoreSel_ = true;
    series_->setSelectedPoint(QSurface3DSeries::invalidSelectionPosition());
    ignoreSel_ = false;
}

QImage SurfaceView::renderImage(const QSize &size, int msaaSamples)
{
    return graph_->renderToImage(msaaSamples, size);
}

void SurfaceView::onSelectedPointChanged(const QPoint &p)
{
    if (p == QSurface3DSeries::invalidSelectionPosition()) {
        if (!ignoreSel_) {
            pickValid_ = false;
            emit pointCleared();
        }
        return;
    }
    const QSurfaceDataItem *it = proxy_->itemAt(p);
    if (!it)
        return;
    pickValid_  = true;
    pickDepth_  = it->x();
    pickTimeMs_ = baseMs_ + qint64(double(it->z()) * 1000.0);
    emit pointPicked(pickDepth_, pickTimeMs_, it->y());
}

bool SurfaceView::eventFilter(QObject *obj, QEvent *ev)
{
    if (obj == graph_ && ev->type() == QEvent::Wheel) {
        auto *we = static_cast<QWheelEvent *>(ev);
        const Qt::KeyboardModifiers m = we->modifiers();
        const bool shift = m & Qt::ShiftModifier;
        const bool ctrl  = m & Qt::ControlModifier;
        const bool alt   = m & Qt::AltModifier;
        if (shift || ctrl) {
            // 个别平台按住 Shift 滚轮会变成横向滚动，这里两个方向都取
            int dy = we->angleDelta().y();
            if (dy == 0)
                dy = we->angleDelta().x();
            if (dy != 0) {
                const bool up = dy > 0;
                if (alt)
                    emit navRequested(ctrl ? NavAction::PanTime : NavAction::PanDepth,
                                      up ? -0.2 : 0.2);          // 向上滚：向浅处/较早时间
                else
                    emit navRequested(ctrl ? NavAction::ZoomTime : NavAction::ZoomDepth,
                                      up ? 0.8 : 1.25);          // 向上滚：放大（范围变小）
            }
            return true;                                         // 吃掉，不再触发视角缩放
        }
    }
    return QWidget::eventFilter(obj, ev);
}
