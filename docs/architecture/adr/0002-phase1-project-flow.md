# ADR 0002: Phase 1 Project / Flow Domain Wiring

- Status: Accepted
- Date: 2026-07-21
- Phase: 1

## Context

Scaffolding already introduced `VisionProject` / `VisionFlow`, but the UI still owned a single `Document`. Canvas, Undo, and `.selt` IO were Document-centric. Rewriting `CanvasScene` immediately would be high risk.

## Decision

1. Introduce `Selt::ProjectService` as the application-layer coordinator (no global singleton).
2. Keep `Document` as a canvas adapter mirror of the **active** `VisionFlow`.
3. Bidirectional sync is serialized inside `ProjectService` with a sync-depth guard.
4. Each flow owns an independent `QUndoStack` under a `QUndoGroup`.
5. Legacy `.selt` v1–v3 remains the on-disk format; import wraps content as a single main flow named「主流程」; save exports the active flow only.
6. Flow management UI is a lightweight combo + buttons above the toolbox (not an XVision dock clone).

## Consequences

- MainWindow no longer owns flow lifecycle directly when OpenCV/vision runtime is enabled.
- Switching flows restores per-flow viewport state and clears run results/selection.
- Cross-flow Undo is intentionally unsupported.
- New project container format and typed parameter bindings remain later phases.

## Acceptance Checklist (Qt Creator)

1. Re-Configure CMake and build `Selt_Gui`, `Selt_Gui_domain_tests`, `Selt_Gui_project_service_tests`.
2. New project shows default「主流程」.
3. Create「流程 A」「流程 B」, add different nodes, switch without cross-leak.
4. Rename updates window title `项目 — 流程`.
5. Delete non-last flow falls back to adjacent flow.
6. Undo/Redo is independent per flow.
7. Open legacy `.selt` into「主流程」with visible migration notes / no silent failure.
8. Save/reopen keeps active-flow graph for this phase’s legacy export rule.
9. F5 runs only the active flow’s document graph.
10. Clean exit with no crash.
