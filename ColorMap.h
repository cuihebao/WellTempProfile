#pragma once
#include <QColor>
#include <QGradient>
#include <cmath>

// jet 色阶（蓝→青→黄→红）。曲面和色带共用这一份，保证颜色一致。
inline QGradientStops jetStops()
{
    QGradientStops s;
    s << QGradientStop(0.000, QColor(0, 0, 128))
      << QGradientStop(0.125, QColor(0, 0, 255))
      << QGradientStop(0.375, QColor(0, 255, 255))
      << QGradientStop(0.625, QColor(255, 255, 0))
      << QGradientStop(0.875, QColor(255, 0, 0))
      << QGradientStop(1.000, QColor(128, 0, 0));
    return s;
}

// 取 1/2/5×10^n 的“好看”刻度间隔
inline double niceStep(double raw)
{
    const double p = std::pow(10.0, std::floor(std::log10(raw)));
    const double f = raw / p;
    const double n = f < 1.5 ? 1.0 : f < 3.0 ? 2.0 : f < 7.0 ? 5.0 : 10.0;
    return n * p;
}
