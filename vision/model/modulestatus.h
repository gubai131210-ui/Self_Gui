#ifndef MODULESTATUS_H
#define MODULESTATUS_H

#include <QString>

enum class ModuleStatus {
    Idle,
    Running,
    Success,
    Failed,
    Disabled
};

inline QString moduleStatusToString(ModuleStatus status)
{
    switch (status) {
    case ModuleStatus::Running:
        return QStringLiteral("执行中");
    case ModuleStatus::Success:
        return QStringLiteral("成功");
    case ModuleStatus::Failed:
        return QStringLiteral("失败");
    case ModuleStatus::Disabled:
        return QStringLiteral("禁用");
    case ModuleStatus::Idle:
    default:
        return QStringLiteral("未执行");
    }
}

#endif // MODULESTATUS_H
