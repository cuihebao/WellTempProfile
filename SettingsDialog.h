#pragma once
#include "Config.h"
#include <QDialog>

class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QSpinBox;

// 显示配置面板：读写 ViewConfig。
// “应用”立即生效（发出 applied），“确定”应用并关闭，“恢复默认”只重置界面上的值。
class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(const ViewConfig &cfg, QWidget *parent = nullptr);
    ViewConfig config() const;               // 把界面上的值写回一份 ViewConfig 返回

signals:
    void applied(const ViewConfig &cfg);

public slots:
    void accept() override;

private slots:
    void onApply();
    void restoreDefaults();
    void updateInfo();

private:
    void load(const ViewConfig &cfg);
    bool validate();

    ViewConfig base_;                        // 面板不编辑的字段（如当前井深范围）原样保留

    QLineEdit      *title_, *depthTitle_, *tempTitle_, *timeTitle_, *timeFormat_;
    QSpinBox       *windowSec_, *refreshMs_, *maxRows_, *maxCols_;
    QComboBox      *agg_;
    QLabel         *info_;
    QCheckBox      *autoTemp_, *showColorBar_;
    QDoubleSpinBox *tempMin_, *tempMax_, *depthStart_;
    QDialogButtonBox *buttons_;
};
