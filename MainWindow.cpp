#include "MainWindow.h"
#include "DataSimulator.h"
#include "DataIngest.h"
#include "SurfaceView.h"
#include "HeatmapView.h"
#include "SettingsDialog.h"
#include "ConfigStore.h"
#include "DataExporter.h"
#include <QtConcurrent>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>

namespace {
const QString kTimeFull = QStringLiteral("yyyy-MM-dd HH:mm:ss");
QString stamp() { return QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")); }
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), buffer_(depth_)
{
    cfg_ = ConfigStore::load(cfg_);
    setWindowTitle(QObject::tr("注气井温度剖面"));

    // ================= 界面 =================
    auto *central = new QWidget(this);
    auto *vbox = new QVBoxLayout(central);

    titleLabel_ = new QLabel(cfg_.title, central);
    titleLabel_->setAlignment(Qt::AlignCenter);
    QFont tf = titleLabel_->font();
    tf.setPointSize(16);
    tf.setBold(true);
    titleLabel_->setFont(tf);
    vbox->addWidget(titleLabel_);

    // 3D 曲面和 2D 热力图叠放，下拉框切换；只有当前可见的那个会收到新数据
    stack_  = new QStackedWidget(central);
    view3d_ = new SurfaceView(stack_);
    view2d_ = new HeatmapView(stack_);
    stack_->addWidget(view3d_);
    stack_->addWidget(view2d_);
    view_ = view3d_;
    vbox->addWidget(stack_, 1);

    // ---- 控制行 ----
    auto *bar = new QHBoxLayout;
    bar->addWidget(new QLabel(QObject::tr("视图:"), central));
    modeCombo_ = new QComboBox(central);
    modeCombo_->addItem(QObject::tr("3D 曲面"));
    modeCombo_->addItem(QObject::tr("2D 热力图"));
    bar->addWidget(modeCombo_);
    bar->addSpacing(16);
    bar->addWidget(new QLabel(QObject::tr("时间窗口(秒):"), central));
    windowSpin_ = new QSpinBox(central);
    windowSpin_->setRange(20, int(cfg_.maxRetainSec * 0.8));     // 保证 1.25×窗口 ≤ 保留上限
    windowSpin_->setSingleStep(60);
    windowSpin_->setValue(cfg_.windowSec);
    bar->addWidget(windowSpin_);

    followBtn_ = new QPushButton(QObject::tr("跟随最新"), central);
    followBtn_->setCheckable(true);
    followBtn_->setChecked(true);
    followBtn_->setToolTip(QObject::tr("选中：时间轴右端跟随最新数据；取消：冻结当前时间范围"));
    bar->addWidget(followBtn_);

    bar->addSpacing(16);
    bar->addWidget(new QLabel(QObject::tr("井深:"), central));
    depthMinSpin_ = new QDoubleSpinBox(central);
    depthMaxSpin_ = new QDoubleSpinBox(central);
    for (QDoubleSpinBox *sp : {depthMinSpin_, depthMaxSpin_}) {
        sp->setRange(depth_.start, fullDepthMax());
        sp->setDecimals(1);
        sp->setSingleStep(10.0);
        sp->setSuffix(QObject::tr(" m"));
        sp->setKeyboardTracking(false);                          // 回车/失焦才触发，避免输入过程中反复重算
    }
    depthMinSpin_->setValue(cfg_.depthMin);
    depthMaxSpin_->setValue(cfg_.depthMax);
    bar->addWidget(depthMinSpin_);
    bar->addWidget(new QLabel(QObject::tr("~"), central));
    bar->addWidget(depthMaxSpin_);
    auto *resetDepthBtn = new QPushButton(QObject::tr("重置深度"), central);
    bar->addWidget(resetDepthBtn);

    bar->addSpacing(16);
    colorBarChk_ = new QCheckBox(QObject::tr("显示色带"), central);
    colorBarChk_->setChecked(cfg_.showColorBar);
    bar->addWidget(colorBarChk_);
    auto *settingsBtn = new QPushButton(QObject::tr("设置…"), central);
    bar->addWidget(settingsBtn);

    bar->addStretch(1);
    auto *imgBtn = new QPushButton(QObject::tr("导出图片"), central);
    bar->addWidget(imgBtn);
    auto *dataBtn = new QToolButton(central);
    dataBtn->setText(QObject::tr("导出数据"));
    dataBtn->setPopupMode(QToolButton::InstantPopup);
    auto *menu = new QMenu(dataBtn);
    menu->addAction(QObject::tr("当前显示网格 (CSV)"), this, &MainWindow::exportGridCsv);
    menu->addAction(QObject::tr("当前范围原始数据 (CSV)"), this, &MainWindow::exportRawCsv);
    dataBtn->setMenu(menu);
    bar->addWidget(dataBtn);
    vbox->addLayout(bar);

    pickLabel_ = new QLabel(QObject::tr("点击图上的点查看数值"), central);
    vbox->addWidget(pickLabel_);

    auto *hint = new QLabel(QObject::tr(
        "左键拖动：旋转　滚轮：缩放视角　Shift+滚轮：缩放井深范围　Ctrl+滚轮：缩放时间范围　"
        "范围缩放以最近点选的点为中心；要移动范围请用上方井深输入框，或取消“跟随最新”后缩放时间　2D 热力图：悬停读数，左键点选，右键取消，缩放以鼠标位置为中心"), central);
    hint->setStyleSheet(QStringLiteral("color: gray;"));
    hint->setWordWrap(true);
    vbox->addWidget(hint);

    statusLabel_ = new QLabel(central);
    vbox->addWidget(statusLabel_);

    setCentralWidget(central);
    resize(1200, 860);

    // ================= 数据 =================
    // 时间轴基准：取当前整点，保证轴上的数值很小，float 精度够用
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    baseMs_ = now - now % 3600000;
    for (ProfileView *v : {static_cast<ProfileView *>(view3d_), static_cast<ProfileView *>(view2d_)}) {
        v->setBaseTimeMs(baseMs_);
        v->applyConfig(cfg_);
    }

    updateRetention();

    // 真实数据入口。第一帧到达时会按数据自带的点数/点间隔重置缓冲，并通过 depthInfoChanged 通知界面
    qRegisterMetaType<DepthInfo>("DepthInfo");
    ingest_ = new DataIngest(&buffer_, cfg_.depthStart, this);
    connect(ingest_, &DataIngest::depthInfoChanged, this, &MainWindow::onDepthInfoChanged);

#ifdef USE_SIMULATOR
    // 研发阶段用模拟器（接入真实数据时在 .pro 里注释掉 DEFINES += USE_SIMULATOR）
    sim_ = new DataSimulator(&buffer_, this);
    sim_->prefill(cfg_.windowSec);
    sim_->start(2000, 2000);        // 调试加速：sim_->start(200, 2000) 即 10 倍速
#endif

    // ================= 信号 =================
    refreshTimer_.setInterval(cfg_.refreshMs);
    connect(&refreshTimer_, &QTimer::timeout, this, &MainWindow::onPeriodicRefresh);
    debounce_.setSingleShot(true);
    debounce_.setInterval(150);
    connect(&debounce_, &QTimer::timeout, this, &MainWindow::requestResample);

    connect(&watcher_, &QFutureWatcher<Grid>::finished, this, &MainWindow::onResampleFinished);
    connect(&exportWatcher_, &QFutureWatcher<QString>::finished, this, &MainWindow::onExportFinished);

    connect(windowSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onWindowSecChanged);
    connect(depthMinSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::onDepthSpinChanged);
    connect(depthMaxSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::onDepthSpinChanged);
    connect(resetDepthBtn, &QPushButton::clicked, this, &MainWindow::onResetDepth);
    connect(followBtn_, &QPushButton::toggled, this, &MainWindow::onFollowToggled);
    connect(imgBtn, &QPushButton::clicked, this, &MainWindow::exportImage);
    connect(colorBarChk_, &QCheckBox::toggled, this, [this](bool on) {
        cfg_.showColorBar = on;
        view3d_->setColorBarVisible(on);
        view2d_->setColorBarVisible(on);
        ConfigStore::save(cfg_);
    });
    connect(settingsBtn, &QPushButton::clicked, this, &MainWindow::openSettings);
    connect(modeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onViewModeChanged);

    for (ProfileView *v : {static_cast<ProfileView *>(view3d_), static_cast<ProfileView *>(view2d_)}) {
        connect(v, &ProfileView::navRequested, this, &MainWindow::onNavRequested);
        connect(v, &ProfileView::pointPicked, this, &MainWindow::onPointPicked);
        connect(v, &ProfileView::pointCleared, this, [this] {
            pickLabel_->setText(QObject::tr("点击图上的点查看数值"));
        });
    }

    refreshTimer_.start();
    QTimer::singleShot(0, this, &MainWindow::requestResample);

//    for(int i = 0;i< 100;i++)
//    {
//        QTimer::singleShot(i*1000, [this]()
//        {
//            QVector<float>temp;
//            for(int i = 0;i< 5000;i++)
//            {
//                temp.append(i);
//            }
//            qint64 tsMs = QDateTime::currentMSecsSinceEpoch();
//            ingest()->pushFrameMs(temp,tsMs,0.2);
//        });
//    }
}

// ====================== 重采样调度 ======================

void MainWindow::onPeriodicRefresh()
{
    updateRetention();
    if (follow_)                // 冻结时数据范围不变，不需要定时重算
        requestResample();
}

void MainWindow::scheduleResample()
{
    debounce_.start();          // 连续滚轮/连续输入时合并成一次
}

void MainWindow::requestResample()
{
    if (watcher_.isRunning()) { // 上一次还没算完：记一笔，算完后用最新的范围再算一次
        pending_ = true;
        return;
    }
    startResample();
}

void MainWindow::startResample()
{
    ResampleParams p;
    p.rows     = cfg_.maxRows;
    p.cols     = cfg_.maxCols;
    p.depthMin = cfg_.depthMin;
    p.depthMax = cfg_.depthMax;
    p.agg      = cfg_.agg;

    if (follow_) {
        qint64 latest = 0;
        if (!buffer_.latestTime(&latest))
            return;
        p.tEnd     = latest;
        p.windowMs = qint64(cfg_.windowSec) * 1000;
    } else {
        p.tEnd     = frozenT1_ - 1;
        p.windowMs = frozenT1_ - frozenT0_;
    }

    jobTimer_.start();
    watcher_.setFuture(QtConcurrent::run(&Resampler::run, &buffer_, p));
}

void MainWindow::onResampleFinished()
{
    const Grid g = watcher_.result();
    const qint64 ms = jobTimer_.elapsed();

    if (pending_) {             // 范围又变了，这次结果作废，直接按新范围重算，避免画面闪烁
        pending_ = false;
        startResample();
        return;
    }
    if (g.isEmpty()) {
        statusLabel_->setText(QObject::tr("当前范围内没有数据，或数据还不够（等待中…）"));
        return;
    }
    lastGrid_ = g;
    view_->setGrid(g);
    updateStatus(ms);
}

void MainWindow::updateStatus(qint64 resampleMs)
{
    statusLabel_->setText(
        QObject::tr("%1　时间 %2 ~ %3　井深 %4 ~ %5 m　网格 %6×%7　重采样 %8 ms　缓冲帧数 %9")
            .arg(follow_ ? QObject::tr("[跟随最新]") : QObject::tr("[已冻结]"))
            .arg(QDateTime::fromMSecsSinceEpoch(lastGrid_.tStart).toString(cfg_.timeFormat))
            .arg(QDateTime::fromMSecsSinceEpoch(lastGrid_.tEnd).toString(cfg_.timeFormat))
            .arg(cfg_.depthMin, 0, 'f', 1)
            .arg(cfg_.depthMax, 0, 'f', 1)
            .arg(lastGrid_.rows).arg(lastGrid_.cols)
            .arg(resampleMs).arg(buffer_.size()));
}

// ====================== 保留时间 ======================

void MainWindow::updateRetention()
{
    // 至少保留 retainSec，且不少于窗口的 1.25 倍 + 10 秒（时间箱对齐会让窗口起点略早于 tEnd - window）；
    // 冻结时还要覆盖冻结范围的起点；总量受 maxRetainSec 限制。
    qint64 ret = std::max<qint64>(qint64(cfg_.retainSec) * 1000, qint64(cfg_.windowSec) * 1250 + 10000);
    qint64 latest = 0;
    if (!follow_ && buffer_.latestTime(&latest))
        ret = std::max<qint64>(ret, latest - frozenT0_ + 10000);
    ret = std::min<qint64>(ret, qint64(cfg_.maxRetainSec) * 1000);
    buffer_.setRetainMs(ret);
}

// ====================== 深度范围 ======================

void MainWindow::applyDepthRange(double lo, double hi)
{
    const double full0 = depth_.start, full1 = fullDepthMax();
    const double minSpan = depth_.step * 50;                     // 至少 50 个点（20 m）
    double span = std::min(std::max(hi - lo, minSpan), full1 - full0);
    const double c = 0.5 * (lo + hi);
    lo = c - span / 2;
    hi = c + span / 2;
    if (lo < full0) { hi += full0 - lo; lo = full0; }            // 贴边时整体平移，保持跨度
    if (hi > full1) { lo -= hi - full1; hi = full1; }
    lo = std::max(lo, full0);
    hi = std::min(hi, full1);

    cfg_.depthMin = lo;
    cfg_.depthMax = hi;
    {
        QSignalBlocker b1(depthMinSpin_), b2(depthMaxSpin_);
        depthMinSpin_->setValue(lo);
        depthMaxSpin_->setValue(hi);
    }
    scheduleResample();
}

void MainWindow::onDepthSpinChanged()
{
    applyDepthRange(depthMinSpin_->value(), depthMaxSpin_->value());
}

void MainWindow::onResetDepth()
{
    applyDepthRange(depth_.start, fullDepthMax());
}

void MainWindow::zoomDepth(double f)
{
    const double lo = cfg_.depthMin, hi = cfg_.depthMax;
    double c = view_->hasPick() ? view_->pickDepth() : 0.5 * (lo + hi);
    if (c < lo || c > hi)
        c = 0.5 * (lo + hi);
    applyDepthRange(c - (c - lo) * f, c + (hi - c) * f);
}

// ====================== 时间范围 ======================

bool MainWindow::currentTimeRange(qint64 *t0, qint64 *t1) const
{
    if (!follow_) {
        *t0 = frozenT0_;
        *t1 = frozenT1_;
        return true;
    }
    qint64 latest = 0;
    if (!buffer_.latestTime(&latest))
        return false;
    *t1 = latest;
    *t0 = latest - qint64(cfg_.windowSec) * 1000;
    return true;
}

void MainWindow::applyTimeRange(qint64 t0, qint64 t1)
{
    qint64 latest = 0;
    if (!buffer_.latestTime(&latest))
        return;
    const qint64 dt      = std::max<qint64>(buffer_.frameIntervalMs(), 1000);
    const qint64 minSpan = std::max<qint64>(20000, 10 * dt);     // 至少 20 秒且不少于 10 帧
    const qint64 maxSpan = qint64(cfg_.maxRetainSec) * 800;      // 与窗口输入框上限一致
    const qint64 span    = std::min(std::max(t1 - t0, minSpan), maxSpan);
    const qint64 c       = t0 + (t1 - t0) / 2;
    t0 = c - span / 2;
    t1 = t0 + span;

    if (t1 >= latest - dt) {                 // 右端到达最新数据：回到“跟随最新”，窗口长度取当前跨度
        follow_ = true;
        cfg_.windowSec = int(span / 1000);
        QSignalBlocker b(windowSpin_);
        windowSpin_->setValue(cfg_.windowSec);
    } else {                                 // 否则冻结在这个时间范围
        follow_ = false;
        frozenT0_ = t0;
        frozenT1_ = t1;
    }
    syncFollowButton();
    updateRetention();
    scheduleResample();
}

void MainWindow::zoomTime(double f)
{
    qint64 t0, t1;
    if (!currentTimeRange(&t0, &t1))
        return;
    qint64 c = t0 + (t1 - t0) / 2;
    if (view_->hasPick() && view_->pickTimeMs() >= t0 && view_->pickTimeMs() <= t1)
        c = view_->pickTimeMs();
    applyTimeRange(c - qint64((c - t0) * f), c + qint64((t1 - c) * f));
}

void MainWindow::onWindowSecChanged(int sec)
{
    // 手动输入窗口长度 = 回到“最近 N 秒”的滚动模式
    cfg_.windowSec = sec;
    follow_ = true;
    syncFollowButton();
    updateRetention();
    scheduleResample();
}

void MainWindow::onFollowToggled(bool on)
{
    if (on) {
        follow_ = true;
    } else {
        qint64 t0, t1;
        if (!currentTimeRange(&t0, &t1)) {   // 还没有数据，没法冻结
            syncFollowButton();
            return;
        }
        follow_ = false;
        frozenT0_ = t0;
        frozenT1_ = t1;
    }
    updateRetention();
    scheduleResample();
}

void MainWindow::syncFollowButton()
{
    QSignalBlocker b(followBtn_);
    followBtn_->setChecked(follow_);
}

void MainWindow::onNavRequested(NavAction a, double amount)
{
    switch (a) {
    case NavAction::ZoomDepth: zoomDepth(amount); break;
    case NavAction::ZoomTime:  zoomTime(amount);  break;
    }
}

void MainWindow::onPointPicked(double depth, qint64 timeMs, double temp)
{
    pickLabel_->setText(QObject::tr("井深 %1 m　时间 %2　温度 %3 ℃")
        .arg(depth, 0, 'f', 1)
        .arg(QDateTime::fromMSecsSinceEpoch(timeMs).toString(cfg_.timeFormat))
        .arg(temp, 0, 'f', 2));
}

// ====================== 导出 ======================

void MainWindow::exportImage()
{
    const QString path = QFileDialog::getSaveFileName(
        this, QObject::tr("导出图片"), QStringLiteral("temp_profile_%1.png").arg(stamp()),
        QObject::tr("PNG 图片 (*.png);;JPEG 图片 (*.jpg)"));
    if (path.isEmpty())
        return;

    // 以 1.5 倍窗口尺寸渲染；无独显/软件渲染时若失败或很慢，可把倍数和 MSAA 调低
    const qreal scale = 1.5;
    const QSize sz(int(view_->width() * scale), int(view_->height() * scale));
    const QImage graph = view_->renderImage(sz, 4);
    if (graph.isNull()) {
        QMessageBox::warning(this, QObject::tr("导出图片"), QObject::tr("渲染失败（当前 OpenGL 环境可能不支持离屏渲染）"));
        return;
    }

    // 色带（显示时）拼在曲面右侧，高度对齐
    QImage cbImg;
    const QPixmap cbPix = view_->grabColorBar();
    if (!cbPix.isNull())
        cbImg = cbPix.scaled(int(cbPix.width() * scale), graph.height(),
                             Qt::IgnoreAspectRatio, Qt::SmoothTransformation).toImage();

    // 上方加标题和时间/深度范围说明
    const int hdr = int(72 * scale);
    QImage out(graph.width() + cbImg.width(), graph.height() + hdr, QImage::Format_RGB32);
    out.fill(Qt::white);
    QPainter p(&out);
    QFont f = font();
    f.setBold(true);
    f.setPixelSize(int(24 * scale));
    p.setFont(f);
    p.drawText(QRect(0, 0, out.width(), int(hdr * 0.6)), Qt::AlignCenter, cfg_.title);
    f.setBold(false);
    f.setPixelSize(int(14 * scale));
    p.setFont(f);
    p.setPen(Qt::darkGray);
    QString sub = QObject::tr("井深 %1 ~ %2 m").arg(cfg_.depthMin, 0, 'f', 1).arg(cfg_.depthMax, 0, 'f', 1);
    if (!lastGrid_.isEmpty())
        sub = QObject::tr("时间 %1 ~ %2　").arg(QDateTime::fromMSecsSinceEpoch(lastGrid_.tStart).toString(kTimeFull),
                                              QDateTime::fromMSecsSinceEpoch(lastGrid_.tEnd).toString(kTimeFull)) + sub;
    p.drawText(QRect(0, int(hdr * 0.6), out.width(), int(hdr * 0.4)), Qt::AlignCenter, sub);
    p.drawImage(0, hdr, graph);
    if (!cbImg.isNull())
        p.drawImage(graph.width(), hdr, cbImg);
    p.end();

    if (out.save(path))
        statusLabel_->setText(QObject::tr("图片已保存：%1").arg(path));
    else
        QMessageBox::warning(this, QObject::tr("导出图片"), QObject::tr("保存失败：%1").arg(path));
}

void MainWindow::exportGridCsv()
{
    if (lastGrid_.isEmpty()) {
        QMessageBox::information(this, QObject::tr("导出数据"), QObject::tr("当前没有可导出的数据"));
        return;
    }
    if (exportWatcher_.isRunning()) {
        QMessageBox::information(this, QObject::tr("导出数据"), QObject::tr("上一次导出还在进行中"));
        return;
    }
    const QString path = QFileDialog::getSaveFileName(
        this, QObject::tr("导出显示网格"), QStringLiteral("grid_%1.csv").arg(stamp()), QObject::tr("CSV 文件 (*.csv)"));
    if (path.isEmpty())
        return;
    lastExportPath_ = path;
    statusLabel_->setText(QObject::tr("正在导出…"));
    exportWatcher_.setFuture(QtConcurrent::run(&DataExporter::writeGridCsv, lastGrid_, path));
}

void MainWindow::exportRawCsv()
{
    if (exportWatcher_.isRunning()) {
        QMessageBox::information(this, QObject::tr("导出数据"), QObject::tr("上一次导出还在进行中"));
        return;
    }
    RawExportJob job;
    if (!currentTimeRange(&job.t0, &job.t1)) {
        QMessageBox::information(this, QObject::tr("导出数据"), QObject::tr("当前没有可导出的数据"));
        return;
    }
    const QString path = QFileDialog::getSaveFileName(
        this, QObject::tr("导出原始数据"), QStringLiteral("raw_%1.csv").arg(stamp()), QObject::tr("CSV 文件 (*.csv)"));
    if (path.isEmpty())
        return;
    job.buf      = &buffer_;
    job.depthMin = cfg_.depthMin;
    job.depthMax = cfg_.depthMax;
    job.path     = path;
    lastExportPath_ = path;
    statusLabel_->setText(QObject::tr("正在导出…"));
    exportWatcher_.setFuture(QtConcurrent::run(&DataExporter::writeRawCsv, job));
}

void MainWindow::onExportFinished()
{
    const QString err = exportWatcher_.result();
    if (err.isEmpty())
        statusLabel_->setText(QObject::tr("数据已导出：%1").arg(lastExportPath_));
    else
        QMessageBox::warning(this, QObject::tr("导出数据"), err);
}

// ====================== 视图切换与设置 ======================

void MainWindow::onViewModeChanged(int index)
{
    view_ = (index == 0) ? static_cast<ProfileView *>(view3d_) : static_cast<ProfileView *>(view2d_);
    stack_->setCurrentIndex(index);
    if (!lastGrid_.isEmpty())
        view_->setGrid(lastGrid_);          // 隐藏期间没有收到新数据，切回来时补上最新的一份
}

void MainWindow::openSettings()
{
    SettingsDialog dlg(cfg_, this);
    connect(&dlg, &SettingsDialog::applied, this, [this](const ViewConfig &c) { applyConfig(c); });
    if (dlg.exec() == QDialog::Accepted)
        applyConfig(dlg.config());
}

void MainWindow::applyConfig(const ViewConfig &c)
{
    const bool windowChanged = (c.windowSec != cfg_.windowSec);

    ViewConfig n = c;
    n.depthMin = cfg_.depthMin;             // 井深查看范围属于视图状态，不由设置面板修改
    n.depthMax = cfg_.depthMax;
    cfg_ = n;

    titleLabel_->setText(cfg_.title);
    view3d_->applyConfig(cfg_);
    view2d_->applyConfig(cfg_);
    ingest_->setStartDepth(cfg_.depthStart);
    refreshTimer_.setInterval(cfg_.refreshMs);
    {
        QSignalBlocker b(windowSpin_);
        windowSpin_->setValue(cfg_.windowSec);
    }
    {
        QSignalBlocker b(colorBarChk_);
        colorBarChk_->setChecked(cfg_.showColorBar);
    }
    if (windowChanged) {                    // 改了窗口长度 = 回到“最近 N 秒”的滚动模式
        follow_ = true;
        syncFollowButton();
    }
    updateRetention();
    scheduleResample();
    ConfigStore::save(cfg_);
}

// 真实数据的深度结构（起始井深/点间隔/点数）确定或变化：更新井深范围，旧数据已被缓冲清空
void MainWindow::onDepthInfoChanged(DepthInfo info)
{
    depth_ = info;
    for (QDoubleSpinBox *sp : {depthMinSpin_, depthMaxSpin_}) {
        QSignalBlocker b(sp);
        sp->setRange(depth_.start, fullDepthMax());
    }
    lastGrid_ = Grid();
    applyDepthRange(depth_.start, fullDepthMax());      // 查看范围重置为整段井深，并同步输入框、触发重采样
}
