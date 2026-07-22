#ifndef UISTYLE_H
#define UISTYLE_H

#include <QColor>

namespace Selt {

/// Shared spacing/size tokens and industrial vision palette (presentation only).
/// Colors follow the active palette set by ThemeController; algorithms must not depend on this.
struct UiStyle
{
    enum class Palette {
        Dark,
        Light
    };

    static constexpr int toolIconSize = 18;
    static constexpr int dockIconSize = 20;
    static constexpr int toolboxIconSize = 32;
    static constexpr int leftDockDefaultWidth = 280;
    static constexpr int leftDockMinWidth = 180;
    static constexpr int rightDockDefaultWidth = 360;
    static constexpr int rightDockMinWidth = 240;
    static constexpr int inspectorDockDefaultWidth = 320;
    static constexpr int inspectorDockMinWidth = 220;
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
    /// v8: light industrial palette + sky-blue collapsible sections
    static constexpr int windowLayoutVersion = 8;

    static void setActivePalette(Palette palette);
    static Palette activePalette();
    static bool isLight() { return activePalette() == Palette::Light; }

    // --- semantic tokens (palette-aware) ---
    static inline QColor canvasBackground()
    {
        return isLight() ? QColor(236, 241, 247) : QColor(32, 34, 38);
    }
    static inline QColor panelBackground()
    {
        return isLight() ? QColor(255, 255, 255) : QColor(40, 43, 48);
    }
    static inline QColor panelAltBackground()
    {
        return isLight() ? QColor(247, 250, 253) : QColor(48, 51, 58);
    }
    static inline QColor windowBackground()
    {
        return isLight() ? QColor(244, 246, 248) : QColor(28, 30, 34);
    }
    static inline QColor border()
    {
        return isLight() ? QColor(201, 210, 220) : QColor(62, 66, 74);
    }
    static inline QColor textPrimary()
    {
        return isLight() ? QColor(31, 41, 51) : QColor(230, 232, 235);
    }
    static inline QColor textSecondary()
    {
        return isLight() ? QColor(107, 122, 138) : QColor(170, 175, 182);
    }

    /// Interaction accent: sky blue in light theme; keep orange aliases for dark compatibility.
    static inline QColor accent()
    {
        return isLight() ? QColor(74, 144, 217) : QColor(255, 140, 0);
    }
    static inline QColor accentBright()
    {
        return isLight() ? QColor(110, 170, 230) : QColor(255, 170, 48);
    }
    static inline QColor accentDim()
    {
        return isLight() ? QColor(56, 118, 186) : QColor(200, 110, 20);
    }

    /// Legacy names — map to active accent so existing call sites keep compiling.
    static inline QColor accentOrange() { return accent(); }
    static inline QColor accentOrangeBright() { return accentBright(); }
    static inline QColor accentOrangeDim() { return accentDim(); }

    static inline QColor successGreen() { return QColor(56, 180, 90); }
    static inline QColor failRed() { return QColor(220, 70, 60); }
    static inline QColor runningBlue() { return QColor(64, 150, 230); }

    static inline QColor connectionIdle()
    {
        return isLight() ? QColor(154, 165, 177) : QColor(110, 115, 125);
    }
    static inline QColor connectionActive() { return accent(); }
    static inline QColor connectionHover() { return accentBright(); }

    static inline QColor nodeFill()
    {
        return isLight() ? QColor(255, 255, 255) : QColor(45, 48, 54);
    }
    static inline QColor gridLine()
    {
        return isLight() ? QColor(220, 228, 236) : QColor(55, 58, 64);
    }
    static inline QColor gridMajorLine()
    {
        return isLight() ? QColor(74, 144, 217, 50) : QColor(255, 140, 0, 55);
    }
    static inline QColor originMarker()
    {
        return isLight() ? QColor(74, 144, 217, 180) : QColor(255, 140, 0, 180);
    }

    /// CollapsibleSection header — soft sky blue (light) / muted slate (dark).
    static inline QColor sectionHeaderBackground()
    {
        return isLight() ? QColor(220, 236, 250) : QColor(48, 51, 58);
    }
    static inline QColor sectionHeaderHover()
    {
        return isLight() ? QColor(198, 224, 245) : QColor(62, 66, 74);
    }
    static inline QColor sectionHeaderBorder()
    {
        return isLight() ? QColor(163, 201, 232) : QColor(62, 66, 74);
    }
    static inline QColor sectionHeaderText()
    {
        return isLight() ? QColor(36, 80, 130) : QColor(230, 232, 235);
    }

private:
    static Palette &paletteRef();
};

} // namespace Selt

#endif // UISTYLE_H
