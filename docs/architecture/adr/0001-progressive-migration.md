# ADR-0001: Progressive In-Repo Migration with Hybrid Plugins

- Status: Accepted
- Date: 2026-07-21
- Phase: 0

## Context

We need VisionMaster-like workflow capabilities without rewriting from scratch or adopting Halcon.

## Decision

1. Progressive refactor inside `Selt_Gui`.
2. Hybrid extension: built-in static registration + external Qt DLL plugins.
3. OpenCV only for vision algorithms.
4. Keep UI visually distinct from XVision.

## Consequences

- Existing app remains runnable during migration.
- Temporary adapters (`Document`, old pipeline switch) exist until later phases remove them.
- Plugin ABI must be versioned carefully from Phase 4 onward.
