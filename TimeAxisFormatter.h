#pragma once
#include <QtDataVisualization/QValue3DAxisFormatter>
#include <QString>

// 时间轴格式化器：轴上存的是“相对 baseMs 的秒数”（float 精度够用），
// 显示时换算回绝对时间 HH:mm:ss。
// 注意：Qt 会在渲染侧复制一份 formatter，所以成员必须在 populateCopy() 里拷贝。
class TimeAxisFormatter : public QtDataVisualization::QValue3DAxisFormatter
{
public:
    explicit TimeAxisFormatter(qint64 baseMs = 0,
                               const QString &fmt = QObject::tr("HH:mm:ss"));

    QString stringForValue(qreal value, const QString &format) const override;

protected:
    QtDataVisualization::QValue3DAxisFormatter *createNewInstance() const override;
    void populateCopy(QtDataVisualization::QValue3DAxisFormatter &copy) const override;

private:
    qint64  baseMs_;
    QString fmt_;
};
