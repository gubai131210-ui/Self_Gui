#ifndef ICONPROVIDER_H
#define ICONPROVIDER_H

#include <QHash>
#include <QIcon>
#include <QString>

namespace Selt {

enum class IconId {
    New,
    Open,
    Save,
    SaveAs,
    Undo,
    Redo,
    Copy,
    Paste,
    Delete,
    ZoomIn,
    ZoomOut,
    Fit,
    Grid,
    Snap,
    Run,
    Loop,
    Stop,
    ClearResult,
    AddFlow,
    Rename,
    RemoveFlow,
    Nodes,
    Properties,
    Results,
    Settings,
    Theme,
    LayoutReset,
    StatusIdle,
    StatusRun,
    StatusOk,
    StatusFail,
    Node
};

class IconProvider
{
public:
    static QIcon icon(IconId id, int size = 20);
    static QIcon visionIcon(const QString &iconKey, int size = 20);
    static QString resourcePath(IconId id);
    static void clearCache();

private:
    static QHash<int, QIcon> &cache();
};

} // namespace Selt

#endif // ICONPROVIDER_H
