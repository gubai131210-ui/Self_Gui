# Roadmap Baseline Freeze

Date: 2026-07-21

## Purpose

Freeze the VisionMaster-style editor + operator-icon baseline before implementing
the multi-phase editor roadmap (canvas interaction → operator config → debug →
reuse → platform).

## Included in product baseline

- Qt 6 node canvas, flow bar, toolbox, property panel, image preview, results/monitor
- ~50 built-in vision operators with typed ports and executors
- Per-operator SVG icons under `resources/icons/vision/` (`iconKey` = `typeId`)
- Theme / IconProvider / `resources/selt.qrc`
- Industrial measurement + calibration + partial run + project resources

## Not product source (ignore / do not commit)

| Path / pattern | Reason |
|----------------|--------|
| `build/` | CMake / Qt Creator build tree |
| `*.exe`, `*.obj`, `moc_*`, `qrc_*`, `ui_*.h` | Generated artifacts |
| `.qtcreator/`, `*.user` | Local IDE settings |
| `autosave.selt.tmp` | Runtime autosave |
| `XVision-develop/` | External reference only |

## Integrity checks

```text
python scripts/verify_vision_icons.py
python scripts/generate_vision_icons.py   # only when regenerating icons
python scripts/update_selt_qrc_vision_icons.py
```

## Acceptance

1. Qt Creator Configure + Build `Selt_Gui` succeeds.
2. `bin/Selt_Gui.exe` exists after build (avoid Rebuild while the app is running).
3. Toolbox shows distinct icons per operator; categories use `cat_*.svg`.
4. Manual smoke: open project, search node, drag, connect, F5 run, restore layout.
