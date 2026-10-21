#include "ColorBar.h"
#include "ColorMap.h"
#include <QLinearGradient>
#include <QPainter>
#include <QSizePolicy>
#include <cmath>

ColorBar::ColorBar(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    setFixedWidth(84);
}

void ColorBar::setRange(double lo, double hi)
{
    if (lo == lo_ && hi == hi_)
        return;
    lo_ = lo;
    hi_ = hi;
    update();
}

void ColorBar::setTitle(const QString &title)
{
    title_ = title;
    update();
}

void ColorBar::paintEvent(QPaintEvent *)
{
    if (hi_ <= lo_)
        return;

    const int top = 44, bottom = 56, left = 8, barW = 18;
    const QRect bar(left, top, barW, height() - top - bottom);
    if (bar.height() < 20)
        return;

    QPainter p(this);
    QLinearGradient g(bar.left(), bar.bottom(), bar.left(), bar.top());   // 下→上：min→max
    g.setStops(jetStops());
    p.fillRect(bar, g);
    p.setPen(palette().color(QPalette::WindowText));
    p.drawRect(bar);

    const double step = niceStep((hi_ - lo_) / 6.0);
    const int decimals = step < 1.0 ? 1 : 0;
    const double first = std::ceil(lo_ / step - 1e-9) * step;
    const QFontMetrics fm(font());
    for (int k = 0;; ++k) {
        double v = first + k * step;
        if (v > hi_ + 1e-9)
            break;
        if (std::fabs(v) < 1e-9)
            v = 0.0;                                   // 避免显示 "-0"
        const double t = (v - lo_) / (hi_ - lo_);
        const int y = bar.bottom() - int(std::lround(t * bar.height()));
        p.drawLine(bar.right(), y, bar.right() + 4, y);
        p.drawText(bar.right() + 7, y + fm.ascent() / 2 - 1, QString::number(v, 'f', decimals));
    }

    if (!title_.isEmpty())
        p.drawText(QRect(0, 8, width(), top - 14), Qt::AlignHCenter | Qt::AlignBottom, title_);
}
