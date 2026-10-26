#include "HeatmapView.h"
#include "ColorBar.h"
#include "ColorMap.h"
#include <QDateTime>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QStringList>
#include <QVector>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

namespace {
// 与 ColorBar 的上下留白保持一致，这样色带和绘图区上下对齐
const int kTop = 44, kBottom = 56, kLeft = 96, kRightNoBar = 16, kBarW = 84, kGap = 8;

// 256 级 jet 查找表
const QVector<QRgb> &jetLut()
{
    static QVector<QRgb> lut;
    if (lut.isEmpty()) {
        const QGradientStops st = jetStops();
        lut.resize(256);
        for (int i = 0; i < 256; ++i) {
            const double t = i / 255.0;
            int k = 1;
            while (k < st.size() - 1 && st[k].first < t) ++k;
            const double t0 = st[k - 1].first, t1 = st[k].first;
            const double f = t1 > t0 ? (t - t0) / (t1 - t0) : 0.0;
            const QColor a = st[k - 1].second, b = st[k].second;
            lut[i] = qRgb(int(a.red()   + f * (b.red()   - a.red())   + 0.5),
                          int(a.green() + f * (b.green() - a.green()) + 0.5),
                          int(a.blue()  + f * (b.blue()  - a.blue())  + 0.5));
        }
    }
    return lut;
}
}

HeatmapView::HeatmapView(QWidget *parent)
    : ProfileView(parent)
{
    setMouseTracking(true);
    setMinimumSize(640, 480);
    colorBar_ = new ColorBar(this);
    applyConfig(cfg_);
}

void HeatmapView::applyConfig(const ViewConfig &cfg)
{
    cfg_ = cfg;
    colorBar_->setTitle(cfg_.tempTitle);
    colorBar_->setVisible(cfg_.showColorBar);
    updateRange();
    rebuildImage();
    resizeEvent(nullptr);
    update();
}

void HeatmapView::setColorBarVisible(bool visible)
{
    cfg_.showColorBar = visible;
    colorBar_->setVisible(visible);
    update();
}

void HeatmapView::setGrid(const Grid &g)
{
    grid_ = g;
    if (!grid_.isEmpty()) {
        const int n = grid_.rows;
        const double firstHalf = 0.5 * double(grid_.times[1] - grid_.times[0]);
        const double lastHalf  = 0.5 * double(grid_.times[n - 1] - grid_.times[n - 2]);
        tMin_ = double(grid_.times.first()) - firstHalf;
        tMax_ = double(grid_.times.last())  + lastHalf;
    }
    updateRange();
    rebuildImage();
    update();
}

void HeatmapView::updateRange()
{
    if (cfg_.autoTemp && !grid_.isEmpty()) {
        lo_ = std::floor((grid_.vmin - 2.0f) / 5.0f) * 5.0;
        hi_ = std::ceil ((grid_.vmax + 2.0f) / 5.0f) * 5.0;
    } else {
        lo_ = cfg_.tempMin;
        hi_ = cfg_.tempMax;
    }
    colorBar_->setRange(lo_, hi_);
}

void HeatmapView::rebuildImage()
{
    if (grid_.isEmpty() || hi_ <= lo_) {
        img_ = QImage();
        return;
    }
    const QVector<QRgb> &lut = jetLut();
    QImage img(grid_.cols, grid_.rows, QImage::Format_RGB32);
    const double scale = 255.0 / (hi_ - lo_);
    for (int r = 0; r < grid_.rows; ++r) {
        QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(r));   // 第 0 行=最旧，画在最上面
        const float *src = grid_.z.constData() + r * grid_.cols;
        for (int c = 0; c < grid_.cols; ++c) {
            const int idx = int((src[c] - lo_) * scale);
            line[c] = lut[std::min(255, std::max(0, idx))];
        }
    }
    img_ = img;
}

QRect HeatmapView::plotRect() const
{
    const int right = width() - (cfg_.showColorBar ? (kBarW + kGap + 4) : kRightNoBar);
    return QRect(kLeft, kTop, std::max(10, right - kLeft), std::max(10, height() - kTop - kBottom));
}

void HeatmapView::resizeEvent(QResizeEvent *)
{
    colorBar_->setGeometry(width() - kBarW - 4, 0, kBarW, height());
}

// ------------------------------------------------------------------ 坐标换算

bool HeatmapView::cellAt(const QPoint &pos, int *row, int *col) const
{
    const QRect pr = plotRect();
    if (grid_.isEmpty() || !pr.contains(pos))
        return false;
    const double fx = double(pos.x() - pr.left()) / pr.width();
    const double fy = double(pos.y() - pr.top())  / pr.height();
    *col = std::min(grid_.cols - 1, std::max(0, int(std::lround(fx * (grid_.cols - 1)))));

    // 行按时间定位：每行占据以自己时间戳为中心的一段（帧间隔不均匀时行高不同）
    const qint64 t = qint64(tMin_ + fy * (tMax_ - tMin_));
    auto it = std::lower_bound(grid_.times.constBegin(), grid_.times.constEnd(), t);
    int r = int(it - grid_.times.constBegin());
    if (r >= grid_.rows) r = grid_.rows - 1;
    else if (r > 0 && (t - grid_.times[r - 1]) < (grid_.times[r] - t)) --r;
    *row = r;
    return true;
}

QPointF HeatmapView::toPixel(double depth, qint64 timeMs) const
{
    const QRect pr = plotRect();
    const double d0 = grid_.depths.first(), d1 = grid_.depths.last();
    return QPointF(pr.left() + (depth - d0) / (d1 - d0) * pr.width(),
                   pr.top()  + (double(timeMs) - tMin_) / (tMax_ - tMin_) * pr.height());
}

bool HeatmapView::pickAt(const QPoint &pos)
{
    int r, c;
    if (!cellAt(pos, &r, &c))
        return false;
    pickValid_  = true;
    pickDepth_  = grid_.depths[c];
    pickTimeMs_ = grid_.times[r];
    emit pointPicked(pickDepth_, pickTimeMs_, grid_.z[r * grid_.cols + c]);
    update();
    return true;
}

// ------------------------------------------------------------------ 交互

void HeatmapView::mouseMoveEvent(QMouseEvent *e)
{
    hover_ = e->pos();
    hoverValid_ = plotRect().contains(hover_) && !grid_.isEmpty();
    update();
}

void HeatmapView::leaveEvent(QEvent *)
{
    hoverValid_ = false;
    update();
}

void HeatmapView::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        pickAt(e->pos());
    } else if (e->button() == Qt::RightButton && pickValid_) {
        pickValid_ = false;
        emit pointCleared();
        update();
    }
}

void HeatmapView::wheelEvent(QWheelEvent *e)
{
    const Qt::KeyboardModifiers m = e->modifiers();
    const bool shift = m & Qt::ShiftModifier;
    const bool ctrl  = m & Qt::ControlModifier;
    if (!shift && !ctrl) {
        e->ignore();
        return;
    }
    int dy = e->angleDelta().y();
    if (dy == 0)
        dy = e->angleDelta().x();
    if (dy != 0) {
        pickAt(e->pos());              // 以鼠标位置为缩放中心（不在图内则沿用上次点选）
        emit navRequested(ctrl ? NavAction::ZoomTime : NavAction::ZoomDepth, dy > 0 ? 0.8 : 1.25);
    }
    e->accept();
}

// ------------------------------------------------------------------ 绘制

void HeatmapView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), palette().color(QPalette::Base));
    p.setPen(palette().color(QPalette::WindowText));
    const QRect pr = plotRect();
    const QFontMetrics fm(font());

    if (grid_.isEmpty() || img_.isNull()) {
        p.drawRect(pr);
        p.drawText(pr, Qt::AlignCenter, QObject::tr("等待数据…"));
        return;
    }

    // 每一行按自己的时间区间画成一条横带：行与行之间的分界取相邻两帧的中点，
    // 所以帧间隔不均匀（如 7~9 秒）或有数据中断时，图上的时间位置依然是准确的
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    for (int r = 0; r < grid_.rows; ++r) {
        const double tLo = (r == 0) ? tMin_ : 0.5 * double(grid_.times[r - 1] + grid_.times[r]);
        const double tHi = (r == grid_.rows - 1) ? tMax_ : 0.5 * double(grid_.times[r] + grid_.times[r + 1]);
        const int y0 = int(std::lround(pr.top() + (tLo - tMin_) / (tMax_ - tMin_) * pr.height()));
        const int y1 = int(std::lround(pr.top() + (tHi - tMin_) / (tMax_ - tMin_) * pr.height()));
        if (y1 <= y0)
            continue;
        p.drawImage(QRect(pr.left(), y0, pr.width(), y1 - y0), img_, QRect(0, r, grid_.cols, 1));
    }
    p.drawRect(pr);

    const double d0 = grid_.depths.first(), d1 = grid_.depths.last();
    const qint64 t0 = qint64(tMin_), t1 = qint64(tMax_);

    // ---- 井深刻度（下方）----
    const double dStep = niceStep((d1 - d0) / 8.0);
    const int dDec = dStep < 1.0 ? 1 : 0;
    for (int k = 0;; ++k) {
        const double d = std::ceil(d0 / dStep - 1e-9) * dStep + k * dStep;
        if (d > d1 + 1e-9)
            break;
        const int x = int(std::lround(pr.left() + (d - d0) / (d1 - d0) * pr.width()));
        p.drawLine(x, pr.bottom(), x, pr.bottom() + 4);
        const QString txt = QString::number(d, 'f', dDec);
        p.drawText(x - fm.horizontalAdvance(txt) / 2, pr.bottom() + 6 + fm.ascent(), txt);
    }
    p.drawText(QRect(pr.left(), height() - 22, pr.width(), 20), Qt::AlignCenter, cfg_.depthTitle);

    // ---- 时间刻度（左侧，上旧下新）----
    static const qint64 kSteps[] = {1, 2, 5, 10, 15, 30, 60, 120, 300, 600, 900, 1800,
                                    3600, 7200, 10800, 21600, 43200, 86400};
    const double spanSec = double(t1 - t0) / 1000.0;
    const int maxTicks = std::max(2, pr.height() / 36);
    qint64 stepSec = kSteps[sizeof(kSteps) / sizeof(kSteps[0]) - 1];
    for (qint64 s : kSteps)
        if (spanSec / double(s) <= maxTicks) { stepSec = s; break; }
    const qint64 stepMs = stepSec * 1000;
    const qint64 off = qint64(QDateTime::fromMSecsSinceEpoch(t0).offsetFromUtc()) * 1000;   // 刻度对齐到本地整点
    for (qint64 tk = ((t0 + off + stepMs - 1) / stepMs) * stepMs - off; tk <= t1; tk += stepMs) {
        const int y = int(std::lround(pr.top() + double(tk - t0) / double(t1 - t0) * pr.height()));
        p.drawLine(pr.left() - 4, y, pr.left(), y);
        const QString txt = QDateTime::fromMSecsSinceEpoch(tk).toString(cfg_.timeFormat);
        p.drawText(pr.left() - 7 - fm.horizontalAdvance(txt), y + fm.ascent() / 2 - 1, txt);
    }
    p.save();
    p.translate(16, pr.center().y());
    p.rotate(-90);
    p.drawText(QRect(-pr.height() / 2, -10, pr.height(), 20), Qt::AlignCenter, cfg_.timeTitle);
    p.restore();

    // ---- 点选标记（按深度/时间换算位置，数据滚动时会跟着移动）----
    if (pickValid_ && pickDepth_ >= d0 && pickDepth_ <= d1 && pickTimeMs_ >= t0 && pickTimeMs_ <= t1) {
        const QPointF pt = toPixel(pickDepth_, pickTimeMs_);
        p.setClipRect(pr);
        p.setPen(QPen(Qt::white, 3));
        p.drawEllipse(pt, 6, 6);
        p.setPen(QPen(Qt::black, 1.5));
        p.drawEllipse(pt, 6, 6);
        p.drawLine(QPointF(pt.x() - 12, pt.y()), QPointF(pt.x() + 12, pt.y()));
        p.drawLine(QPointF(pt.x(), pt.y() - 12), QPointF(pt.x(), pt.y() + 12));
        p.setClipping(false);
    }

    // ---- 悬停读数框 ----
    int r, c;
    if (!rendering_ && hoverValid_ && cellAt(hover_, &r, &c)) {
        const QStringList lines = {
            QObject::tr("井深 %1 m").arg(grid_.depths[c], 0, 'f', 1),
            QObject::tr("时间 %1").arg(QDateTime::fromMSecsSinceEpoch(grid_.times[r]).toString(cfg_.timeFormat)),
            QObject::tr("温度 %1 ℃").arg(double(grid_.z[r * grid_.cols + c]), 0, 'f', 2)};
        int w = 0;
        for (const QString &l : lines)
            w = std::max(w, fm.horizontalAdvance(l));
        const int h = fm.height() * lines.size();
        QRect box(hover_.x() + 14, hover_.y() + 14, w + 14, h + 10);
        if (box.right() > pr.right())  box.moveRight(hover_.x() - 14);
        if (box.bottom() > pr.bottom()) box.moveBottom(hover_.y() - 14);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 225));
        p.drawRect(box);
        p.setPen(Qt::black);
        for (int i = 0; i < lines.size(); ++i)
            p.drawText(box.left() + 7, box.top() + 5 + fm.ascent() + i * fm.height(), lines[i]);
    }
}

QImage HeatmapView::renderImage(const QSize &size, int)
{
    const QSize sz = size.isEmpty() ? this->size() : size;
    QImage img(sz, QImage::Format_RGB32);
    img.fill(palette().color(QPalette::Base));
    QPainter p(&img);
    p.scale(sz.width() / double(width()), sz.height() / double(height()));
    rendering_ = true;
    render(&p);
    rendering_ = false;
    p.end();
    return img;
}
