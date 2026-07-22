# ADR 0007: Node Visual Metadata, Multi-shape ROI and Template Teach

## Status

Accepted — 2026-07-21

## Context

Phase 4 delivered runtime observability, plugins and template match via file path. Nodes still looked uniform; ROI editing was rectangle-only; templates could not be taught from the preview or stored as project-relative assets.

## Decision

1. **Visual metadata** lives on `ModuleDescriptor` (`iconKey`, `accentColor`, `helpText`) and is rendered by `NodeGraphicsItem` / `NodeCatalogModel` via `IconProvider::visionIcon` and `modulevisualstyle.h`. Color is assistive only.
2. **Fold / run summary** are UI concerns: `NodeModel.collapsed` persists; summaries are transient on the graphics item and do not alter the DAG.
3. **Multi-shape ROI** uses `ExtendedRoi` as domain data; `ImageCanvasWidget` owns interaction tools (rectangle, rotated rect, circle, ellipse, polygon). Algorithms continue to consume axis-aligned crops via bounding boxes where needed.
4. **Template teach** is a service (`TemplateTeachService`) that crops from ROI and registers files through `ProjectResourceStore` under `assets/templates/` with project-relative paths.
5. **Template Match input priority**: upstream `template` Image port → `templateResourceId` project resource → `templatePath` file. `vision.templateSource` exposes resources/files as typed Image for wiring.

## Consequences

- Legacy `.selt` gains optional `document.projectResources` without bumping file version.
- Missing template files produce diagnostics instead of silent failure.
- Display remains free of OpenCV; executors remain free of QWidget.
- Future phases can add theme-specific SVG packs and interactive template editors without changing this ownership split.
