# ADR 0004: Typed Ports, Parameter Bindings, and Runtime Resolution

- Status: Accepted
- Date: 2026-07-21

## Context

Phase 1 established VisionProject/Flow and Document adapters. Node parameters were still flat JSON constants, and FlowRuntime only forwarded the last image. That blocked typed measurements, project variables, and safe wiring.

## Decision

1. Ports and values use `DataTypeId` / `DataValue` with explicit compatibility (`Int → Real` only).
2. Parameters store optional `parameterBindings`; missing bindings remain Constant via `NodeModel.parameters`.
3. `GraphValidator` owns static structure/type/binding diagnostics; canvas rejects invalid links before Undo.
4. `PortValueStore` + `BindingResolver` resolve constants, upstream outputs, and project variables at runtime.
5. Built-in executors keep reading legacy `ExecutionRequest.parameters` JSON; runtime builds that view after binding resolution.
6. Project variables persist as optional `document.projectVariables` without bumping `.selt` format version.

## Consequences

- Old `.selt` files open unchanged; constants are inferred.
- UI BindingEditor can switch Constant / Upstream / ProjectVariable with Undo.
- Runtime diagnostics distinguish parameter, binding, and execution failures.
