#ifndef SELTERROR_H
#define SELTERROR_H

#include <QString>

namespace Selt {

enum class ErrorCode {
    Ok = 0,
    InvalidArgument,
    NotFound,
    ValidationFailed,
    CycleDetected,
    TypeMismatch,
    PluginLoadFailed,
    ExecutionFailed,
    Cancelled,
    NotImplemented
};

struct Error
{
    ErrorCode code{ErrorCode::Ok};
    QString message;
    QString contextId;

    bool ok() const { return code == ErrorCode::Ok; }
    static Error success();
    static Error fail(ErrorCode code, const QString &message, const QString &contextId = QString());
};

} // namespace Selt

#endif // SELTERROR_H
