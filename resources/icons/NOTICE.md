# Icon Assets NOTICE

Icons under `resources/icons/` are original SVG assets created for the Selt_Gui project.

- License: MIT (same as project contribution intent for UI assets)
- Style: 24×24, stroke `#2c3e50`, stroke-width ~1.8, round caps/joins
- Style reference research included public MIT icon sets such as
  [oclero/qlementine-icons](https://github.com/oclero/qlementine-icons) and Lucide,
  but **no third-party binary/SVG files were vendored** into this tree

## Layout

| Path | Purpose |
|------|---------|
| `resources/icons/*.svg` | Toolbar / chrome icons (`IconId`) |
| `resources/icons/vision/*.svg` | Per-operator + category glyphs for the node toolbox |

Vision operator files are named from `ModuleDescriptor.typeId` with `.` → `_`
(e.g. `vision.imageLoader` → `vision_imageLoader.svg`). Category fallbacks use
`cat_<name>.svg` (输入→`cat_input`, 预处理→`cat_preprocess`, …).

Regenerate operator SVGs with:

```text
python scripts/generate_vision_icons.py
python scripts/update_selt_qrc_vision_icons.py
```

When replacing icons with third-party packs, keep the upstream LICENSE next to this NOTICE
and record the source/version in `docs/architecture/ui-guidelines.md`.
