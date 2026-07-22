# ADR 0003: UI Simplification and Icon System

- Status: Accepted
- Date: 2026-07-21
- Phase: UI refactor (post Phase 1)

## Context

After wiring Project/Flow, the main window still looked crowded: text-only toolbar buttons, flow controls buried in the left dock, and the results dock always occupying vertical space. Users asked for icon buttons and a cleaner IDE-like layout without replacing the Qt Widgets stack.

## Decision

1. Keep Qt Widgets (`QMainWindow`, `QDockWidget`, `QToolBar`, `QTabWidget`).
2. Introduce a small theme layer:
   - `IconProvider` for SVG icons packed in `resources/selt.qrc`
   - `ThemeController` for light/dark/system stylesheets
   - `UiStyle` for spacing and default dock sizes
3. Use unified `QAction` objects for menus and toolbar; toolbar shows icon-only.
4. Move flow management to a compact top bar above the canvas.
5. Convert the node palette to Model/View with search and categories.
6. Rename the right dock to “检查器”; hide the results dock until a run starts.
7. Persist window geometry/state and provide “恢复默认布局”.
8. Do **not** vendor QFluentKit / Advanced Docking System in this phase (license/complexity). Optional Qlementine style remains a future CMake flag, not enabled by default.

## Icon source

Custom MIT-compatible stroke SVG icons authored for Selt under `resources/icons/`. Design language is inspired by common desktop icon sets (including research of [qlementine-icons](https://github.com/oclero/qlementine-icons)), but assets are project-owned to avoid FetchContent/offline issues on Chinese Windows paths.

## Consequences

- UI code owns presentation; ProjectService remains the domain coordinator.
- Theme changes do not rebuild VisionProject/Document.
- Users must re-Configure CMake because `Qt6::Svg` is now required.
