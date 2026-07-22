# Selt Architecture Overview

## Goal

Migrate XVision-like industrial vision workflow capabilities into Selt_Gui using Qt 6 + OpenCV, with progressive refactoring inside the current repository.

## Layered Targets

| Target | Responsibility |
|--------|----------------|
| `SeltFoundation` | Version, IDs, errors |
| `SeltVisionData` | Typed vision values (Image, ROI, Measurement, Overlay) |
| `SeltFlowCore` | Project / Flow / Node / Port / Connection models |
| `SeltVisionRuntime` | DAG validation, scheduling, execution context |
| `SeltPluginSdk` | Stable plugin metadata + factory interfaces |
| `SeltOpenCvNodes` | Built-in OpenCV node executors |
| `SeltGraphEditor` | Canvas scene/view/items |
| `SeltVisionDisplay` | Image view, ROI editing, overlays |
| `SeltStudio` | Main window / docks / services |

## Dependency Rules

1. UI must not call OpenCV APIs directly.
2. Algorithm nodes must not depend on QWidget.
3. Runtime dispatches through registries/interfaces, not type switch statements (migration goal).
4. ROI/Overlay are domain data; Display only renders/edits them.
5. Model mutations go through Command/Service layers.
6. Execution results must not mutate undoable project models.

## Migration Strategy

- Keep existing `Document` as an adapter while `VisionProject` / `VisionFlow` mature.
- Keep `.selt` format versioned; add migration chain for newer formats and
  `.seltproject` directory containers.
- Built-in nodes register statically; optional external nodes load via Qt plugins.
- Runtime executes against immutable snapshots; external files/plugins are validated
  before entering OpenCV or plugin execution paths.

## Related Documents

- [ADR template](adr/0000-template.md)
- [ADR 0002 Phase 1 Project/Flow](adr/0002-phase1-project-flow.md)
- [ADR 0003 UI Simplification](adr/0003-ui-simplification.md)
- [ADR 0004 Typed Bindings](adr/0004-typed-bindings.md)
- [ADR 0005 Runtime Production](adr/0005-runtime-production.md)
- [ADR 0006 Plugin / Input / Runtime Observability](adr/0006-plugin-input-runtime.md)
- [ADR 0007 Node Visual / ROI / Template Teach](adr/0007-node-visual-template-teach.md)
- [ADR 0009 Runtime Guards / Plugin Deployment](adr/0009-runtime-guards-and-plugin-deployment.md)
- [ADR 0010 Industrial Measurement / Calibration / Devices](adr/0010-industrial-measurement-calibration-devices.md)
- [ADR 0011 Operator Platform / OpenCV Extension](adr/0011-operator-platform-opencv-extension.md)
- [UI guidelines](ui-guidelines.md)
- [VisionMaster-style UI checklist](ui-visionmaster-checklist.md)
- [Communication operators](communication-operators.md)
- [Communication production baseline](communication-production-baseline.md)
- [Communication production checklist](communication-production-checklist.md)
- [Production hardening checklist](production-hardening-checklist.md)
- [Deployment README](../deployment/README.md)
- [Exe missing troubleshooting](../troubleshooting/selt-gui-exe-missing.md)
- [XVision feature map](xvision-feature-map.md)
- [OpenCV substitution matrix](opencv-substitution-matrix.md)
- [Algorithm factor checklist](algorithm-factor-checklist.md)
- [Migration log](migration-log.md)
- [Dependency rules](dependency-rules.md)
- [Phase completion notes](phase-completion-0-12.md)
