# Selt UI Guidelines

## Principles

1. Canvas is the visual center; docks are secondary.
2. Menus keep full text commands and shortcuts; toolbars stay icon-only with tooltips.
3. Prefer one `QAction` per command. Never duplicate business slots in menus and toolbars.
4. Load all icons and stylesheets from `resources/selt.qrc` (`:/selt/...`). Never hardcode absolute disk paths.
5. Use `Selt::IconProvider` / `Selt::ThemeController` / `Selt::UiStyle` instead of ad-hoc colors and icon files in widgets.

## Layout

| Region | Content | Default |
|--------|---------|---------|
| Top toolbar | New/Open/Save, Undo/Redo, Fit/Grid, Run/Stop | Always visible, icon-only |
| Flow bar | Flow tabs (combo fallback) + add/rename/delete | Above canvas |
| Left dock | Operator toolbox (icon grid + list mode) | ~300px |
| Right vision dock | Image preview + results/monitor/help | ~360px, visible; splitter 可上下/左右切换 |
| Right inspector | Properties / input / help | Hidden until opened |
| Status bar | Run status, cursor, zoom | Permanent |

## Theme

- Modes: light, dark, system (`视图 → 主题`)
- **Default mode: dark** (industrial workspace)
- Dark industrial palette: charcoal panels + orange accent (`#ff8c00`)
- Shared tokens: `Selt::UiStyle` (`accentOrange`, `connectionIdle/Active/Hover`, `gridMajorLine`)
- Stylesheets: `resources/theme/theme_light.qss`, `theme_dark.qss`
- Layout persistence keys: `ui/geometry`, `ui/windowState`, `ui/themeMode`, `ui/layoutVersion` (current=6), `ui/visionSplitterOrientation`, `ui/visionSplitterState`
- Vision workbench: `视图 → 图像/结果左右布局`、`恢复图像与结果布局`
- Port exposure ADR: `docs/architecture/adr/0012-port-exposure-canvas-contract.md`
- Roadmap baseline: `docs/architecture/roadmap-baseline.md`
- Manual roadmap checklist: `docs/architecture/editor-roadmap-checklist.md`
- VisionMaster checklist: `docs/architecture/ui-visionmaster-checklist.md`

## Adding an icon

### Toolbar / chrome (`IconId`)

1. Add `resources/icons/<name>.svg` (24×24, stroke `#2c3e50` unless status/color critical).
2. Register it in `resources/selt.qrc`.
3. Add `IconId` + path mapping in `ui/theme/iconprovider.*`.
4. Bind the icon through `IconProvider::icon(...)` on a `QAction` or tool button.

### Vision operators (`iconKey`)

1. Prefer regenerating the full set: `python scripts/generate_vision_icons.py`
   then `python scripts/update_selt_qrc_vision_icons.py`.
2. Or add `resources/icons/vision/<stem>.svg` manually and list it in `selt.qrc`
   (`stem` = `typeId` with `.` → `_`, e.g. `vision_grayscale`).
3. Set `ModuleDescriptor.iconKey` to the `typeId` (registry `makeBase` does this).
4. Categories / plugins may use Chinese keys (`输入`, `插件`, …); `IconProvider::visionIcon`
   maps them to `cat_*.svg`.
5. Licensing notes live in `resources/icons/NOTICE.md`.

## Manual check (Qt Creator)

1. Re-Configure and build `Selt_Gui`, `Selt_Gui_ui_tests`.
2. Confirm toolbar icons and tooltips.
3. Switch light/dark theme; restart and verify persistence.
4. Search nodes, drag to canvas, switch flows.
5. Run F5; results dock appears; hide via View menu.
6. Use “恢复默认布局”.
7. Resize to ~1024×640 and confirm canvas remains usable.
