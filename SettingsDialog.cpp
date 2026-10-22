#include "SettingsDialog.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(const ViewConfig &cfg, QWidget *parent)
    : QDialog(parent), base_(cfg)
{
    setWindowTitle(QObject::tr("显示设置"));

    // ---- 文字与格式 ----
    auto *gText = new QGroupBox(QObject::tr("标题与坐标轴"), this);
    auto *fText = new QFormLayout(gText);
    title_      = new QLineEdit(gText);
    depthTitle_ = new QLineEdit(gText);
    tempTitle_  = new QLineEdit(gText);
    timeTitle_  = new QLineEdit(gText);
    timeFormat_ = new QLineEdit(gText);
    timeFormat_->setToolTip(QObject::tr("Qt 时间格式，例如 HH:mm:ss 或 MM-dd HH:mm"));
    fText->addRow(QObject::tr("图标题"), title_);
    fText->addRow(QObject::tr("井深轴名称"), depthTitle_);
    fText->addRow(QObject::tr("温度轴/色带名称"), tempTitle_);
    fText->addRow(QObject::tr("时间轴名称"), timeTitle_);
    fText->addRow(QObject::tr("时间显示格式"), timeFormat_);

    // ---- 数据与刷新 ----
    auto *gData = new QGroupBox(QObject::tr("数据与刷新"), this);
    auto *fData = new QFormLayout(gData);
    depthStart_ = new QDoubleSpinBox(gData);
    depthStart_->setRange(-10000.0, 100000.0);
    depthStart_->setDecimals(2);
    depthStart_->setSuffix(QObject::tr(" m"));
    depthStart_->setToolTip(QObject::tr("真实数据每帧第 0 个点对应的井深。修改后下一帧到达时会清空已缓存的数据。"));
    windowSec_ = new QSpinBox(gData);
    windowSec_->setRange(20, int(base_.maxRetainSec * 0.8));
    windowSec_->setSuffix(QObject::tr(" 秒"));
    refreshMs_ = new QSpinBox(gData);
    refreshMs_->setRange(500, 60000);
    refreshMs_->setSingleStep(500);
    refreshMs_->setSuffix(QObject::tr(" 毫秒"));
    maxRows_ = new QSpinBox(gData);
    maxRows_->setRange(20, 1000);
    maxCols_ = new QSpinBox(gData);
    maxCols_->setRange(50, 2000);
    agg_ = new QComboBox(gData);
    agg_->addItem(QObject::tr("平均值"));
    agg_->addItem(QObject::tr("最大值（保留高温尖峰）"));
    agg_->addItem(QObject::tr("最小值（保留低温尖峰）"));
    info_ = new QLabel(gData);
    info_->setWordWrap(true);
    fData->addRow(QObject::tr("起始井深（第 0 个点）"), depthStart_);
    fData->addRow(QObject::tr("时间窗口"), windowSec_);
    fData->addRow(QObject::tr("刷新周期"), refreshMs_);
    fData->addRow(QObject::tr("网格最大行数（时间方向）"), maxRows_);
    fData->addRow(QObject::tr("网格最大列数（井深方向）"), maxCols_);
    fData->addRow(QObject::tr("降采样聚合方式"), agg_);
    fData->addRow(info_);

    // ---- 温度范围与色带 ----
    auto *gColor = new QGroupBox(QObject::tr("温度范围与色带"), this);
    auto *fColor = new QFormLayout(gColor);
    autoTemp_ = new QCheckBox(QObject::tr("根据数据自动确定范围"), gColor);
    tempMin_ = new QDoubleSpinBox(gColor);
    tempMax_ = new QDoubleSpinBox(gColor);
    for (QDoubleSpinBox *sp : {tempMin_, tempMax_}) {
        sp->setRange(-200.0, 500.0);
        sp->setDecimals(1);
    }
    showColorBar_ = new QCheckBox(QObject::tr("显示色带"), gColor);
    fColor->addRow(autoTemp_);
    fColor->addRow(QObject::tr("温度下限"), tempMin_);
    fColor->addRow(QObject::tr("温度上限"), tempMax_);
    fColor->addRow(showColorBar_);

    buttons_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel |
                                    QDialogButtonBox::Apply | QDialogButtonBox::RestoreDefaults, this);
    buttons_->button(QDialogButtonBox::Ok)->setText(QObject::tr("确定"));
    buttons_->button(QDialogButtonBox::Cancel)->setText(QObject::tr("取消"));
    buttons_->button(QDialogButtonBox::Apply)->setText(QObject::tr("应用"));
    buttons_->button(QDialogButtonBox::RestoreDefaults)->setText(QObject::tr("恢复默认"));

    auto *lay = new QVBoxLayout(this);
    lay->addWidget(gText);
    lay->addWidget(gData);
    lay->addWidget(gColor);
    lay->addWidget(buttons_);

    connect(buttons_, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(buttons_, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);
    connect(buttons_->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, &SettingsDialog::onApply);
    connect(buttons_->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked,
            this, &SettingsDialog::restoreDefaults);

    connect(maxRows_, QOverload<int>::of(&QSpinBox::valueChanged), this, &SettingsDialog::updateInfo);
    connect(maxCols_, QOverload<int>::of(&QSpinBox::valueChanged), this, &SettingsDialog::updateInfo);
    connect(autoTemp_, &QCheckBox::toggled, this, [this](bool on) {
        tempMin_->setEnabled(!on);
        tempMax_->setEnabled(!on);
    });

    load(base_);
}

void SettingsDialog::load(const ViewConfig &c)
{
    title_->setText(c.title);
    depthTitle_->setText(c.depthTitle);
    tempTitle_->setText(c.tempTitle);
    timeTitle_->setText(c.timeTitle);
    timeFormat_->setText(c.timeFormat);
    depthStart_->setValue(c.depthStart);
    windowSec_->setValue(c.windowSec);
    refreshMs_->setValue(c.refreshMs);
    maxRows_->setValue(c.maxRows);
    maxCols_->setValue(c.maxCols);
    agg_->setCurrentIndex(int(c.agg));
    autoTemp_->setChecked(c.autoTemp);
    tempMin_->setValue(c.tempMin);
    tempMax_->setValue(c.tempMax);
    tempMin_->setEnabled(!c.autoTemp);
    tempMax_->setEnabled(!c.autoTemp);
    showColorBar_->setChecked(c.showColorBar);
    updateInfo();
}

ViewConfig SettingsDialog::config() const
{
    ViewConfig c = base_;
    c.title      = title_->text();
    c.depthTitle = depthTitle_->text();
    c.tempTitle  = tempTitle_->text();
    c.timeTitle  = timeTitle_->text();
    c.timeFormat = timeFormat_->text().trimmed().isEmpty() ? ViewConfig().timeFormat : timeFormat_->text();
    c.depthStart = depthStart_->value();
    c.windowSec  = windowSec_->value();
    c.refreshMs  = refreshMs_->value();
    c.maxRows    = maxRows_->value();
    c.maxCols    = maxCols_->value();
    c.agg        = AggMode(agg_->currentIndex());
    c.autoTemp   = autoTemp_->isChecked();
    c.tempMin    = tempMin_->value();
    c.tempMax    = tempMax_->value();
    c.showColorBar = showColorBar_->isChecked();
    return c;
}

void SettingsDialog::updateInfo()
{
    const int verts = maxRows_->value() * maxCols_->value();
    QString t = QObject::tr("显示网格最多 %1 个顶点。").arg(verts);
    if (verts > 300000)
        t += QObject::tr("<span style='color:#c00'> 超过 30 万，3D 曲面在无独显的机器上旋转可能明显卡顿（2D 热力图不受影响）。</span>");
    else
        t += QObject::tr(" 3D 曲面在无独显的机器上建议不超过 30 万。");
    info_->setText(t);
}

bool SettingsDialog::validate()
{
    if (!autoTemp_->isChecked() && tempMax_->value() <= tempMin_->value()) {
        QMessageBox::warning(this, windowTitle(), QObject::tr("温度上限必须大于下限"));
        return false;
    }
    return true;
}

void SettingsDialog::onApply()
{
    if (validate())
        emit applied(config());
}

void SettingsDialog::accept()
{
    if (validate())
        QDialog::accept();
}

void SettingsDialog::restoreDefaults()
{
    ViewConfig d;
    d.depthMin = base_.depthMin;      // 当前查看范围不属于“设置”，保持不变
    d.depthMax = base_.depthMax;
    load(d);
}
