#include "foundation/selterror.h"

namespace Selt {

Error Error::success()
{
    return {};
}

Error Error::fail(ErrorCode code, const QString &message, const QString &contextId)
{
    Error e;
    e.code = code;
    e.message = message;
    e.contextId = contextId;
    return e;
}

} // namespace Selt
