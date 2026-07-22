#ifndef SELTLOGGING_H
#define SELTLOGGING_H

#include <QString>
#include <QStringList>

namespace Selt {

enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warn,
    Error
};

class AppLog
{
public:
    static void append(LogLevel level, const QString &message);
    static QStringList lines();
    static void clear();
};

} // namespace Selt

#endif
