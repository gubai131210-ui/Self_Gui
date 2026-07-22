#ifndef SELTID_H
#define SELTID_H

#include <QString>

namespace Selt {

class IdGenerator
{
public:
    static QString create(const QString &prefix);
};

} // namespace Selt

#endif // SELTID_H
