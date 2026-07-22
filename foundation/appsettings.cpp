#include "foundation/appsettings.h"

namespace Selt {

QSettings AppSettings::settings()
{
    return QSettings(QStringLiteral("Selt"), QStringLiteral("SeltVisionStudio"));
}

QString AppSettings::language()
{
    return settings().value(QStringLiteral("ui/language"), QStringLiteral("zh_CN")).toString();
}

void AppSettings::setLanguage(const QString &lang)
{
    settings().setValue(QStringLiteral("ui/language"), lang);
}

QString AppSettings::theme()
{
    return settings().value(QStringLiteral("ui/theme"), QStringLiteral("selt-neutral")).toString();
}

void AppSettings::setTheme(const QString &theme)
{
    settings().setValue(QStringLiteral("ui/theme"), theme);
}

bool AppSettings::restoreLayout()
{
    return settings().value(QStringLiteral("ui/restoreLayout"), true).toBool();
}

void AppSettings::setRestoreLayout(bool on)
{
    settings().setValue(QStringLiteral("ui/restoreLayout"), on);
}

} // namespace Selt
