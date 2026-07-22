#ifndef OPERATORPREFERENCES_H
#define OPERATORPREFERENCES_H

#include <QSettings>
#include <QString>
#include <QStringList>

namespace Selt {

/// Persists toolbox favorites and recently used operator typeIds.
class OperatorPreferences
{
public:
    static constexpr int kMaxRecent = 12;

    static QStringList favorites()
    {
        QSettings settings;
        return settings.value(QStringLiteral("toolbox/favorites")).toStringList();
    }

    static void setFavorites(const QStringList &typeIds)
    {
        QSettings settings;
        settings.setValue(QStringLiteral("toolbox/favorites"), typeIds);
    }

    static bool isFavorite(const QString &typeId)
    {
        return favorites().contains(typeId);
    }

    static void toggleFavorite(const QString &typeId)
    {
        QStringList list = favorites();
        if (list.contains(typeId))
            list.removeAll(typeId);
        else
            list.prepend(typeId);
        setFavorites(list);
    }

    static QStringList recent()
    {
        QSettings settings;
        return settings.value(QStringLiteral("toolbox/recent")).toStringList();
    }

    static void pushRecent(const QString &typeId)
    {
        if (typeId.isEmpty())
            return;
        QStringList list = recent();
        list.removeAll(typeId);
        list.prepend(typeId);
        while (list.size() > kMaxRecent)
            list.removeLast();
        QSettings settings;
        settings.setValue(QStringLiteral("toolbox/recent"), list);
    }
};

} // namespace Selt

#endif // OPERATORPREFERENCES_H
