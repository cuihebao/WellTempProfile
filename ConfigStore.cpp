#include "ConfigStore.h"
#include <QSettings>
#include <QtGlobal>

namespace {
void open(QSettings &s) { s.setIniCodec("UTF-8"); }
}

ViewConfig ConfigStore::load(const ViewConfig &d)
{
    QSettings s(QSettings::IniFormat, QSettings::UserScope,
                QStringLiteral("WellTempProfile"), QStringLiteral("settings"));
    open(s);

    ViewConfig c = d;
    c.title      = s.value(QStringLiteral("title"),      d.title).toString();
    c.depthTitle = s.value(QStringLiteral("depthTitle"), d.depthTitle).toString();
    c.tempTitle  = s.value(QStringLiteral("tempTitle"),  d.tempTitle).toString();
    c.timeTitle  = s.value(QStringLiteral("timeTitle"),  d.timeTitle).toString();
    c.timeFormat = s.value(QStringLiteral("timeFormat"), d.timeFormat).toString();
    if (c.timeFormat.isEmpty())
        c.timeFormat = d.timeFormat;

    c.depthStart   = s.value(QStringLiteral("depthStart"),   d.depthStart).toDouble();
    c.windowSec    = qBound(20,   s.value(QStringLiteral("windowSec"),    d.windowSec).toInt(),    int(d.maxRetainSec * 0.8));
    c.refreshMs    = qBound(500,  s.value(QStringLiteral("refreshMs"),    d.refreshMs).toInt(),    60000);
    c.maxRows      = qBound(20,   s.value(QStringLiteral("maxRows"),      d.maxRows).toInt(),      1000);
    c.maxCols      = qBound(50,   s.value(QStringLiteral("maxCols"),      d.maxCols).toInt(),      2000);
    c.agg          = AggMode(qBound(0, s.value(QStringLiteral("agg"), int(d.agg)).toInt(), 2));
    c.autoTemp     = s.value(QStringLiteral("autoTemp"),     d.autoTemp).toBool();
    c.tempMin      = s.value(QStringLiteral("tempMin"),      d.tempMin).toDouble();
    c.tempMax      = s.value(QStringLiteral("tempMax"),      d.tempMax).toDouble();
    if (c.tempMax <= c.tempMin) { c.tempMin = d.tempMin; c.tempMax = d.tempMax; }
    c.showColorBar = s.value(QStringLiteral("showColorBar"), d.showColorBar).toBool();
    return c;
}

void ConfigStore::save(const ViewConfig &c)
{
    QSettings s(QSettings::IniFormat, QSettings::UserScope,
                QStringLiteral("WellTempProfile"), QStringLiteral("settings"));
    open(s);

    s.setValue(QStringLiteral("title"),      c.title);
    s.setValue(QStringLiteral("depthTitle"), c.depthTitle);
    s.setValue(QStringLiteral("tempTitle"),  c.tempTitle);
    s.setValue(QStringLiteral("timeTitle"),  c.timeTitle);
    s.setValue(QStringLiteral("timeFormat"), c.timeFormat);
    s.setValue(QStringLiteral("depthStart"), c.depthStart);
    s.setValue(QStringLiteral("windowSec"),  c.windowSec);
    s.setValue(QStringLiteral("refreshMs"),  c.refreshMs);
    s.setValue(QStringLiteral("maxRows"),    c.maxRows);
    s.setValue(QStringLiteral("maxCols"),    c.maxCols);
    s.setValue(QStringLiteral("agg"),        int(c.agg));
    s.setValue(QStringLiteral("autoTemp"),   c.autoTemp);
    s.setValue(QStringLiteral("tempMin"),    c.tempMin);
    s.setValue(QStringLiteral("tempMax"),    c.tempMax);
    s.setValue(QStringLiteral("showColorBar"), c.showColorBar);
    s.sync();
}
