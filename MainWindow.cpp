#include "MainWindow.h"
#include "SurfaceView.h"
#include "DataSimulator.h"
#include <QtConcurrent>
#include <QDateTime>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QSpinBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), buffer_(depth_)
{
    setWindowTitle(QObject::tr("注气井温度剖面"));

    // ---- 界面 ----
    auto *central = new QWidget(this);
    auto *vbox = new QVBoxLayout(central);

    auto *title = new QLabel(cfg_.title, central);
    title->setAlignment(Qt::AlignCenter);
    QFont tf = title->font();
    tf.setPointSize(16);
    tf.setBold(true);
    title->setFont(tf);
    vbox->addWidget(title);

    view_ = new SurfaceView(central);
    vbox->addWidget(view_, 1);

    auto *bar = new QHBoxLayout;
    bar->addWidget(new QLabel(QObject::tr("时间窗口(分钟):"), central));
    windowSpin_ = new QSpinBox(central);
    windowSpin_->setRange(1, 720);
    windowSpin_->setValue(cfg_.windowSec / 60);
    bar->addWidget(windowSpin_);
    pickLabel_ = new QLabel(QObject::tr("点击曲面上的点查看数值"), central);
    bar->addSpacing(20);
    bar->addWidget(pickLabel_, 1);
    vbox->addLayout(bar);

    statusLabel_ = new QLabel(central);
    vbox->addWidget(statusLabel_);

    setCentralWidget(central);
    resize(1100, 800);

    // ---- 时间轴基准：取当前整点，保证轴上的数值很小，float 精度够用 ----
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    baseMs_ = now - now % 3600000;
    view_->setBaseTimeMs(baseMs_);
    view_->applyConfig(cfg_);

    // ---- 数据：研发阶段用模拟器（真实数据接入时，改为调用 buffer_.append()） ----
    updateRetention();
    sim_ = new DataSimulator(&buffer_, this);
    sim_->prefill(cfg_.windowSec);
    sim_->start(2000, 2000);        // 调试加速：sim_->start(200, 2000) 即 10 倍速

    // ---- 刷新：独立于数据周期；重采样在工作线程，完成后回到界面线程更新 ----
    connect(&refreshTimer_, &QTimer::timeout, this, &MainWindow::onRefresh);
    connect(&watcher_, &QFutureWatcher<Grid>::finished, this, &MainWindow::onResampleFinished);
    connect(windowSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onWindowMinutesChanged);
    connect(view_, &SurfaceView::pointPicked, this, &MainWindow::onPointPicked);
    connect(view_, &SurfaceView::pointCleared, this, [this] {
        pickLabel_->setText(QObject::tr("点击曲面上的点查看数值"));
    });

    refreshTimer_.start(cfg_.refreshMs);
    QTimer::singleShot(0, this, &MainWindow::onRefresh);
}

void MainWindow::updateRetention()
{
    // 保留窗口长度的 1.25 倍 + 10 秒余量（时间箱对齐会让窗口起点略早于 tEnd - window）
    buffer_.setRetainMs(qint64(cfg_.windowSec) * 1250 + 10000);
}

void MainWindow::onRefresh()
{
    if (watcher_.isRunning())
        return;                         // 上一次还没算完，跳过这次，避免积压
    qint64 tLatest = 0;
    if (!buffer_.latestTime(&tLatest))
        return;

    ResampleParams p;
    p.tEnd     = tLatest;
    p.windowMs = qint64(cfg_.windowSec) * 1000;
    p.rows     = cfg_.maxRows;
    p.cols     = cfg_.maxCols;
    p.depthMin = cfg_.depthMin;
    p.depthMax = cfg_.depthMax;
    p.agg      = cfg_.agg;

    jobTimer_.start();
    watcher_.setFuture(QtConcurrent::run(&Resampler::run, &buffer_, p));
}

void MainWindow::onResampleFinished()
{
    const Grid g = watcher_.result();
    const qint64 ms = jobTimer_.elapsed();
    if (g.isEmpty()) {
        statusLabel_->setText(QObject::tr("等待数据…"));
        return;
    }
    view_->setGrid(g);
    statusLabel_->setText(QObject::tr("显示网格 %1 × %2    重采样耗时 %3 ms    缓冲帧数 %4")
                          .arg(g.rows).arg(g.cols).arg(ms).arg(buffer_.size()));
}

void MainWindow::onWindowMinutesChanged(int minutes)
{
    cfg_.windowSec = minutes * 60;
    updateRetention();
    onRefresh();
}

void MainWindow::onPointPicked(double depth, qint64 timeMs, double temp)
{
    pickLabel_->setText(QObject::tr("井深 %1 m    时间 %2    温度 %3 ℃")
        .arg(depth, 0, 'f', 1)
        .arg(QDateTime::fromMSecsSinceEpoch(timeMs).toString(cfg_.timeFormat))
        .arg(temp, 0, 'f', 2));
}
