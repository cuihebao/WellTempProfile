#pragma once
#include "Config.h"

// 把 ViewConfig 保存到用户配置文件（INI，UTF-8），下次启动自动恢复。
// 井深/时间的当前查看范围属于“视图状态”，不保存。
class ConfigStore
{
public:
    static ViewConfig load(const ViewConfig &defaults);
    static void save(const ViewConfig &cfg);
};
