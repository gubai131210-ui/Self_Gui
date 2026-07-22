# Phase Completion Notes (0-12 baseline)

This migration pass established the architectural spine inside the existing repository.

## Delivered

- Modular CMake targets: `SeltFoundation`, `SeltFlowCore`, `SeltVisionData`, `SeltVisionRuntime`
- Architecture docs under `docs/architecture/`
- `VisionProject` / `VisionFlow` domain with ordered flows, graph validation, and legacy Document import/export
- **Phase 1 (detailed):** `ProjectService`, FlowBar multi-flow UI, Document↔Flow sync, per-flow `QUndoStack`, migration report on legacy `.selt`
- **UI polish:** SVG icon provider, light/dark theme, icon-only toolbar, Model/View node catalog, inspector dock, deferred results dock, layout persistence
- **Phase 2:** Typed `DataValue`/`ParameterBinding`, port types in registry, `GraphValidator`, `PortValueStore`/`BindingResolver`, FlowRuntime binding resolution, BindingEditor, project variables persistence, diagnostics UI
- **Phase 3:** Interactive ROI move/resize/clamp, overlay filter, `RuntimeFailureKind` diagnostics, non-reentrant loop runs, Blur/Canny/Morphology/Resize nodes, `InputSourceService`, plugin conflict policy, scheduler/subflow/container interfaces
- **Phase 4:** Runtime progress/busy UI wiring, scheduler injection, plugin descriptor→catalog merge + example invert plugin, input-source dock + live-frame injection, Template Match node, expanded CTest coverage
- **Phase 5 (visual + teach):** Category/accent colors + catalog icons, node fold/run summary, multi-shape ROI tools, `ProjectResourceStore` + `TemplateTeachService`, `vision.templateSource`, Template Match resource priority
- Typed `DataValue` / bindings / extended ROI models
- `INodeExecutor` registry + deterministic DAG `FlowRuntime`
- Hybrid plugin host (`plugins/nodes`) + built-in OpenCV executors
- RunController once/loop/stop + runtime monitor dock
- Camera abstractions (`OpenCvVideoCaptureDevice`, `DirectoryReplayDevice`) + `probeOpenCvCameras`
- Extra algorithms: blur / canny / morphology / template match (**node registered**)
- Project storage migration helper for legacy `.selt`
- **Phase 6 foundation:** directory `.seltproject` container, manifest, resource copying,
  atomic replacement, `ExecutionScope`, immutable execution snapshots, and partial DAG runs
- **Phase 6 debug:** canvas actions for running a node or its upstream closure, with scope and
  snapshot metadata shown in the runtime monitor
- Logging + app settings stubs for productization

## Still progressive / next stage plans

- Rich industrial metrology nodes and vendor camera plugins
- Crash dump packaging and formal i18n `.ts` catalogs
- Full subflow UI / parallel scheduling / compressed project packaging
- Theme-specific vision SVG packs and node-specific content cards

Keep using the stage template in the master plan before expanding each remaining UX surface.

## Phase 1 manual acceptance

See [adr/0002-phase1-project-flow.md](adr/0002-phase1-project-flow.md).

## Phase 2 manual acceptance

See [adr/0004-typed-bindings.md](adr/0004-typed-bindings.md). After Qt Creator Configure/rebuild:

1. Create image→gray→threshold nodes and connect; incompatible Real→Image links are rejected.
2. In inspector, switch a numeric parameter to Upstream/ProjectVariable and Undo/Redo.
3. Run pipeline; binding/parameter/execution failures show distinct labels and jump to failed node.
4. Open an old `.selt`; constants still run; migration notes mention binding/variable counts.

## Phase 3 manual acceptance

See [adr/0005-runtime-production.md](adr/0005-runtime-production.md). After Qt Creator Configure/rebuild:

1. Draw ROI with Ctrl+drag; drag interior to move; drag corners to resize; locked ROI cannot edit.
2. Overlay filter combo limits visible overlay types; zoom keeps ROI aligned.
3. Start continuous run then Stop; cannot start another run while busy; monitor shows node status/failure kind.
4. Drag Blur/Canny/Morphology/Resize from catalog, connect image ports, run successfully.
5. Plugin directory missing or bad plugin does not prevent opening old projects.

## Phase 4 manual acceptance

See [adr/0006-plugin-input-runtime.md](adr/0006-plugin-input-runtime.md). After Qt Creator Configure/rebuild:

1. F5 run: canvas nodes highlight Running→Success/Failed in order; run/loop disabled while busy; F8 stops loop.
2. Build `selt_invert_node` (or copy example DLL into `bin/plugins/nodes`); restart; catalog shows「反色(示例插件)」 and it executes.
3. Inspector → 输入源: open directory replay with several images; continuous run advances frames without editing loader path.
4. Drag「模板匹配」, set template path / connect image, run; overlays and score appear.
5. Missing plugin dir / bad plugin still soft-fails; diagnostics visible in runtime monitor.

## Phase 5 manual acceptance (visual + template teach)

See [adr/0007-node-visual-template-teach.md](adr/0007-node-visual-template-teach.md). After Qt Creator Configure/rebuild:

1. Catalog and canvas nodes show category icons/colors; hover shows help text; fold chevron collapses body without moving ports.
2. Result preview ROI tool: rectangle / rotated rect / circle / ellipse / polygon; lock and clear still work; zoom keeps ROI aligned.
3. Draw ROI → 教示模板 → resource under `assets/templates/`; selected Template Match / Template Source gets `templateResourceId`.
4. Wire 图片输入 → 模板匹配(in) and 模板源 → 模板匹配(template); run once; score/overlays appear.
5. Save/reopen project; missing template file shows diagnostics; old `.selt` without resources still opens.
