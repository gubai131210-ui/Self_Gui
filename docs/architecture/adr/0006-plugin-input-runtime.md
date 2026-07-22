# ADR 0006: Plugin Descriptor Registration, Input Source Injection, Runtime Observability

- Status: Accepted
- Date: 2026-07-21

## Context

Phase 3 delivered plugin loading, `InputSourceService`, and `IFlowScheduler`, but several capabilities were not wired into the product surface:

- plugins registered executors without merging `ModuleDescriptor` into the node catalog;
- `InputSourceService` existed only in tests;
- run progress did not update canvas nodes until the whole DAG finished;
- `RunController` called `FlowRuntime` directly instead of the scheduler interface.

## Decision

1. `VisionNodeRegistry` owns a mutable descriptor cache and accepts `registerExternalDescriptor(s)` for plugins. Built-in IDs still win on conflict.
2. `PluginHost` registers descriptors and executors together, records `pluginId` per type, and remains fail-soft.
3. `MainWindow` reloads `NodePalette` after plugin load and surfaces plugin diagnostics in the runtime monitor.
4. `RunController` depends on `IFlowScheduler` (default `SequentialScheduler`), emits per-node `runProgress`, and exposes `busyChanged` for UI enablement.
5. Optional live/replay frames are injected through `ExecutionContext::setLiveFrame` / `RuntimeExecuteOptions::liveFrame`; `imageLoader` prefers the live frame over file path without mutating node parameters.
6. Template Match is a first-class built-in node (`vision.templateMatch`) with descriptor + executor.
7. Old `.selt` v1–v3 remain readable; Document adapter and `VisionPipeline` legacy entry are unchanged.

## Consequences

- Example plugin `plugins/examples/invert_node` builds as `selt_invert_node` into `bin/plugins/nodes`.
- Camera probing is best-effort and requires OpenCV videoio; absence is diagnostic, not a link failure.
- True single-node subgraph execution, multi-shape ROI, subflows, and parallel scheduling remain deferred.
