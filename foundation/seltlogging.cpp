#include "foundation/seltlogging.h"

#include <QMutex>
#include <QMutexLocker>

namespace Selt {
namespace {
QStringList g_lines;
QMutex g_mutex;
}

void AppLog::append(LogLevel level, const QString &message)
{
    QString prefix = QStringLiteral("INFO");
    switch (level) {
    case LogLevel::Trace: prefix = QStringLiteral("TRACE"); break;
    case LogLevel::Debug: prefix = QStringLiteral("DEBUG"); break;
    case LogLevel::Warn: prefix = QStringLiteral("WARN"); break;
    case LogLevel::Error: prefix = QStringLiteral("ERROR"); break;
    case LogLevel::Info:
    default: break;
    }
    QMutexLocker lock(&g_mutex);
    g_lines.append(QStringLiteral("[%1] %2").arg(prefix, message));
    if (g_lines.size() > 500)
        g_lines.removeFirst();
}

QStringList AppLog::lines()
{
    QMutexLocker lock(&g_mutex);
    return g_lines;
}

void AppLog::clear()
{
    QMutexLocker lock(&g_mutex);
    g_lines.clear();
}

} // namespace Selt
