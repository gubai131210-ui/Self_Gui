#ifndef APPSETTINGS_H
#define APPSETTINGS_H

#include <QSettings>
#include <QString>

namespace Selt {

class AppSettings
{
public:
    static QString language();
    static void setLanguage(const QString &lang);
    static QString theme();
    static void setTheme(const QString &theme);
    static bool restoreLayout();
    static void setRestoreLayout(bool on);

private:
    static QSettings settings();
};

} // namespace Selt

#endif
