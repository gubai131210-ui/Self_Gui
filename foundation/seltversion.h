#ifndef SELTVERSION_H
#define SELTVERSION_H

#include <QString>

namespace Selt {

struct VersionInfo
{
    static constexpr int major = 0;
    static constexpr int minor = 2;
    static constexpr int patch = 0;
    static constexpr int projectFormatVersion = 3;
    static constexpr int pluginSdkVersion = 1;

    static QString productName();
    static QString productVersionString();
    static QString architectureCodename();
};

} // namespace Selt

#endif // SELTVERSION_H
