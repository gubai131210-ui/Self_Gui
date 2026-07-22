# Editor Roadmap Acceptance (Qt Creator)

Use after Configure + Build `Selt_Gui`. Prefer **Build** over **Rebuild** while the app is closed.

## Baseline
- [ ] `python scripts/verify_vision_icons.py` exits 0
- [ ] Toolbox shows per-operator icons
- [ ] Layout restore / theme switch works

## Phase 1 — Canvas / toolbox
- [ ] Edit → Align / distribute / auto-layout (Ctrl+Shift+L)
- [ ] Drag connection: compatible ports highlight green; occupied input can be replaced
- [ ] Toolbox: favorites (right-click) + recent section update after place
- [ ] View → navigational minimap toggles
- [ ] F9 / context menu toggles breakpoint (red dot)

## Phase 2 — Config
- [ ] Property panel Color params open color picker
- [ ] Binding state line shows Constant / Upstream / ProjectVariable
- [ ] ROI teach + image preview still work

## Phase 3 — Debug
- [ ] Run hits breakpoint and stops with diagnostic
- [ ] Measurement table filter hides non-matching rows
- [ ] Runtime monitor receives publish diagnostics

## Phase 4 — Reuse
- [ ] View → 项目资源 dock lists templates/resources
- [ ] Execute → 发布前校验 reports disabled/breakpoint/missing resources

## Phase 5 — Platform
- [ ] Inspector 说明 tab shows plugin host summary
- [ ] ModuleDescriptor.uiSchema tags appear for measure/locate operators

## Production hardening
- [ ] See `docs/architecture/production-hardening-checklist.md`
- [ ] Deploy layout: `docs/deployment/README.md`
- [ ] Exe missing recovery: `docs/troubleshooting/selt-gui-exe-missing.md`
