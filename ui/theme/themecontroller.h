#ifndef THEMECONTROLLER_H
#define THEMECONTROLLER_H

#include <QObject>
#include <QString>

class QApplication;

namespace Selt {

enum class ThemeMode {
    Light,
    Dark,
    System
};

class ThemeController : public QObject
{
    Q_OBJECT
public:
    explicit ThemeController(QObject *parent = nullptr);

    ThemeMode mode() const { return m_mode; }
    void setMode(ThemeMode mode);
    void apply(QApplication *app);
    void loadFromSettings();
    void saveToSettings() const;

    static QString modeToString(ThemeMode mode);
    static ThemeMode modeFromString(const QString &text);

signals:
    void themeChanged(ThemeMode mode);

private:
    bool preferDarkSystem() const;
    QString styleSheetFor(ThemeMode effective) const;

    ThemeMode m_mode{ThemeMode::Light};
};

} // namespace Selt

#endif // THEMECONTROLLER_H
