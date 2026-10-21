#include "TimeAxisFormatter.h"
#include <QDateTime>

using namespace QtDataVisualization;

TimeAxisFormatter::TimeAxisFormatter(qint64 baseMs, const QString &fmt)
    : QValue3DAxisFormatter(), baseMs_(baseMs), fmt_(fmt)
{
}

QString TimeAxisFormatter::stringForValue(qreal value, const QString &) const
{
    return QDateTime::fromMSecsSinceEpoch(baseMs_ + qint64(value * 1000.0)).toString(fmt_);
}

QValue3DAxisFormatter *TimeAxisFormatter::createNewInstance() const
{
    return new TimeAxisFormatter(baseMs_, fmt_);
}

void TimeAxisFormatter::populateCopy(QValue3DAxisFormatter &copy) const
{
    QValue3DAxisFormatter::populateCopy(copy);
    TimeAxisFormatter *c = static_cast<TimeAxisFormatter *>(&copy);
    c->baseMs_ = baseMs_;
    c->fmt_    = fmt_;
}
