#pragma once
#include <QWidget>
#include <QString>

// 竖直色带（色度条）：下端=最小值（蓝），上端=最大值（红），右侧带刻度数值。
class ColorBar : public QWidget
{
    Q_OBJECT
public:
    explicit ColorBar(QWidget *parent = nullptr);

    void setRange(double lo, double hi);
    void setTitle(const QString &title);
    QSize sizeHint() const override { return QSize(84, 200); }
    QSize minimumSizeHint() const override { return QSize(84, 120); }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    double  lo_ = -20.0, hi_ = 60.0;
    QString title_;
};
