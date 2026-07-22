#include "ui/theme/themecontroller.h"

#include <QApplication>
#include <QColor>
#include <QCoreApplication>
#include <QFile>
#include <QPalette>
#include <QSettings>
#include <QStyle>
#include <QStyleFactory>

namespace Selt {

ThemeController::ThemeController(QObject *parent)
    : QObject(parent)
{
}

void ThemeController::setMode(ThemeMode mode)
{
    if (m_mode == mode)
        return;
    m_mode = mode;
    saveToSettings();
    if (auto *app = qobject_cast<QApplication *>(QCoreApplication::instance()))
        apply(app);
    emit themeChanged(m_mode);
}

void ThemeController::apply(QApplication *app)
{
    if (!app)
        return;

    ThemeMode effective = m_mode;
    if (effective == ThemeMode::System)
        effective = preferDarkSystem() ? ThemeMode::Dark : ThemeMode::Light;

    app->setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QPalette palette = app->style()->standardPalette();
    if (effective == ThemeMode::Dark) {
        palette.setColor(QPalette::Window, QColor(28, 30, 34));
        palette.setColor(QPalette::WindowText, QColor(230, 232, 235));
        palette.setColor(QPalette::Base, QColor(34, 37, 43));
        palette.setColor(QPalette::AlternateBase, QColor(42, 45, 52));
        palette.setColor(QPalette::Text, QColor(230, 232, 235));
        palette.setColor(QPalette::Button, QColor(53, 57, 64));
        palette.setColor(QPalette::ButtonText, QColor(230, 232, 235));
        palette.setColor(QPalette::Highlight, QColor(255, 140, 0));
        palette.setColor(QPalette::HighlightedText, QColor(28, 30, 34));
        palette.setColor(QPalette::ToolTipBase, QColor(48, 51, 58));
        palette.setColor(QPalette::ToolTipText, QColor(230, 232, 235));
        palette.setColor(QPalette::PlaceholderText, QColor(150, 155, 162));
        palette.setColor(QPalette::Link, QColor(255, 160, 40));
    } else {
        // Explicit light text colors so QComboBox never inherits white-on-white.
        palette.setColor(QPalette::Window, QColor(244, 246, 248));
        palette.setColor(QPalette::WindowText, QColor(31, 41, 51));
        palette.setColor(QPalette::Base, QColor(255, 255, 255));
        palette.setColor(QPalette::AlternateBase, QColor(247, 249, 251));
        palette.setColor(QPalette::Text, QColor(31, 41, 51));
        palette.setColor(QPalette::Button, QColor(255, 255, 255));
        palette.setColor(QPalette::ButtonText, QColor(31, 41, 51));
        palette.setColor(QPalette::Highlight, QColor(74, 144, 217));
        palette.setColor(QPalette::HighlightedText, Qt::white);
        palette.setColor(QPalette::PlaceholderText, QColor(154, 165, 177));
    }
    app->setPalette(palette);
    app->setStyleSheet(styleSheetFor(effective));
}

void ThemeController::loadFromSettings()
{
    QSettings settings;
    m_mode = modeFromString(settings.value(QStringLiteral("ui/themeMode"),
                                           modeToString(ThemeMode::Dark)).toString());
}

void ThemeController::saveToSettings() const
{
    QSettings settings;
    settings.setValue(QStringLiteral("ui/themeMode"), modeToString(m_mode));
}

QString ThemeController::modeToString(ThemeMode mode)
{
    switch (mode) {
    case ThemeMode::Light: return QStringLiteral("light");
    case ThemeMode::Dark: return QStringLiteral("dark");
    case ThemeMode::System: return QStringLiteral("system");
    }
    return QStringLiteral("system");
}

ThemeMode ThemeController::modeFromString(const QString &text)
{
    if (text == QLatin1String("light"))
        return ThemeMode::Light;
    if (text == QLatin1String("dark"))
        return ThemeMode::Dark;
    return ThemeMode::System;
}

bool ThemeController::preferDarkSystem() const
{
    const QPalette systemPalette = QApplication::palette();
    return systemPalette.color(QPalette::Window).lightness() < 128;
}

QString ThemeController::styleSheetFor(ThemeMode effective) const
{
    const QString path = (effective == ThemeMode::Dark)
        ? QStringLiteral(":/selt/theme/theme_dark.qss")
        : QStringLiteral(":/selt/theme/theme_light.qss");
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readAll());
}

} // namespace Selt
