#include "foundation/seltid.h"

#include <QUuid>

namespace Selt {

QString IdGenerator::create(const QString &prefix)
{
    const QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (prefix.isEmpty())
        return uuid;
    return prefix + QLatin1Char('_') + uuid;
}

} // namespace Selt
