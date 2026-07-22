# ADR 0005: Runtime Reliability, Industrial Nodes, and Future Scheduling Boundaries

- Status: Accepted
- Date: 2026-07-21

## Context

Phase 2 delivered typed ports and parameter bindings. Production use still needed:

- reliable ROI/overlay editing;
- clearer runtime failure taxonomy and non-reentrant loop execution;
- industrial preprocessing nodes with typed ports;
- stable camera/replay/file input boundaries;
- reserved interfaces for subflows, schedulers, and project containers.

## Decision

1. Display edits ROI through coordinate-clamped interaction and Undo commands; `ExtendedRoi` remains the richer model with legacy `RoiRect` adapters.
2. Runtime failures use `RuntimeFailureKind` (`validation/binding/parameter/device/execution/cancelled/disabled`) and feed both diagnostics and the monitor task table.
3. `RunController` rejects re-entrant starts; loop ticks skip while a previous tick is still running.
4. Blur/Canny/Morphology/Resize register as first-class vision nodes with descriptors and executors.
5. `InputSourceService` owns camera/replay/file device lifecycle; UI only observes status and diagnostics.
6. Plugin load failures never block host startup; built-in type IDs win on conflict.
7. `IFlowScheduler` / `SequentialScheduler`, expanded `SubflowInterface`, and `ProjectContainerManifest` define next-stage boundaries without changing `.selt` version.

## Consequences

- Existing `.selt` files remain readable; container format is reserved.
- Subflow UI and parallel scheduling are intentionally deferred.
- New nodes appear in the catalog immediately after Configure/rebuild.
