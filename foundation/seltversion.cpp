#include "foundation/seltversion.h"

namespace Selt {

QString VersionInfo::productName()
{
    return QStringLiteral("Selt Vision Studio");
}

QString VersionInfo::productVersionString()
{
    return QStringLiteral("%1.%2.%3").arg(major).arg(minor).arg(patch);
}

QString VersionInfo::architectureCodename()
{
    return QStringLiteral("xvision-migration");
}

} // namespace Selt
