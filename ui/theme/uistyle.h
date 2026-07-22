#ifndef UISTYLE_H
#define UISTYLE_H

#include <QColor>

namespace Selt {

/// Shared spacing/size tokens and industrial vision palette (presentation only).
struct UiStyle
{
    static constexpr int toolIconSize = 18;
    static constexpr int dockIconSize = 20;
    static constexpr int toolboxIconSize = 32;
    static constexpr int leftDockDefaultWidth = 280;
    static constexpr int leftDockMinWidth = 180;
    static constexpr int rightDockDefaultWidth = 360;
    static constexpr int rightDockMinWidth = 240;
    static constexpr int inspectorDockDefaultWidth = 280;
    static constexpr int centralMinWidth = 320;
    static constexpr int bottomDockDefaultHeight = 260;
    static constexpr int flowBarHeight = 36;
    static constexpr int imageToolbarHeight = 30;
    static constexpr int splitterHandleWidth = 8;
    static constexpr int visionPreviewDefaultHeight = 280;
    static constexpr int visionResultDefaultHeight = 220;
    static constexpr int visionPreviewDefaultWidth = 320;
    static constexpr int visionResultDefaultWidth = 240;
    static constexpr int compactSpacing = 4;
    static constexpr int panelMargin = 6;
    /// v6: vision workbench orientation + splitter persistence + compact docks
    static constexpr int windowLayoutVersion = 6;

    // Industrial dark palette (Selt-owned, not a third-party brand clone).
    static inline QColor canvasBackground() { return QColor(32, 34, 38); }
    static inline QColor panelBackground() { return QColor(40, 43, 48); }
    static inline QColor panelAltBackground() { return QColor(48, 51, 58); }
    static inline QColor windowBackground() { return QColor(28, 30, 34); }
    static inline QColor border() { return QColor(62, 66, 74); }
    static inline QColor textPrimary() { return QColor(230, 232, 235); }
    static inline QColor textSecondary() { return QColor(170, 175, 182); }
    static inline QColor accentOrange() { return QColor(255, 140, 0); }
    static inline QColor accentOrangeBright() { return QColor(255, 170, 48); }
    static inline QColor accentOrangeDim() { return QColor(200, 110, 20); }
    static inline QColor successGreen() { return QColor(56, 180, 90); }
    static inline QColor failRed() { return QColor(220, 70, 60); }
    static inline QColor runningBlue() { return QColor(64, 150, 230); }
    static inline QColor connectionIdle() { return QColor(110, 115, 125); }
    static inline QColor connectionActive() { return QColor(255, 140, 0); }
    static inline QColor connectionHover() { return QColor(255, 170, 48); }
    static inline QColor nodeFill() { return QColor(45, 48, 54); }
    static inline QColor gridLine() { return QColor(55, 58, 64); }
    static inline QColor gridMajorLine() { return QColor(255, 140, 0, 55); }
    static inline QColor originMarker() { return QColor(255, 140, 0, 180); }
};

} // namespace Selt

#endif // UISTYLE_H
