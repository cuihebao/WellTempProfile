#pragma once
#include <QObject>
#include <QString>
#include <QtGlobal>

// 视图范围导航动作（3D 曲面和 2D 热力图共用）
enum class NavAction { ZoomDepth, ZoomTime };

// 降采样聚合方式
enum class AggMode { Mean, Max, Min };

// 原始数据的深度坐标描述：每帧 count 个点，从 start 开始，间隔 step（单位 m）
struct DepthInfo {
    double start = 0.0;
    double step  = 0.4;
    int    count = 5000;
};

// 显示相关的所有可配置项（后续配置面板直接读写这个结构）
struct ViewConfig {
    QString title      = QObject::tr("二氧化碳注气井温度剖面图");
    QString depthTitle = QObject::tr("井深/m");
    QString tempTitle  = QObject::tr("温度/℃");
    QString timeTitle  = QObject::tr("时间");
    QString timeFormat = QObject::tr("HH:mm:ss");

    int    windowSec = 600;     // 时间窗口（秒）
    int    refreshMs = 5000;    // 界面刷新周期（毫秒），独立于 2 秒的数据周期
    int    maxRows   = 300;     // 显示网格：时间方向最大行数
    int    maxCols   = 700;     // 显示网格：深度方向最大列数
    AggMode agg      = AggMode::Mean;

    double depthMin  = 1500.0;  // 显示的深度范围（m）
    double depthMax  = 3500.0;

    int    retainSec    = 1800; // 内存中至少保留多少秒的原始帧（用于缩小窗口后再放大）
    int    maxRetainSec = 7200; // 保留时间上限（冻结视图/放大时间范围时受此限制），按 5 km 约 50 KB/帧估算

    bool   showColorBar = true; // 是否显示右侧色带

    bool   autoTemp  = false;   // 温度轴/色带范围：自动 or 固定
    double tempMin   = -20.0;
    double tempMax   = 60.0;
};
