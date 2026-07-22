# XVision Feature Map

| XVision area | Status in XVision | Selt current | Target phase |
|--------------|-------------------|--------------|--------------|
| Project/Flow/Func | Core available, save incomplete | VisionProject + multi-flow UI via ProjectService; Document is active-flow adapter | 1 (done) |
| Typed data XObject | Partial | `DataValue` / typed ports / bindings | 2 (done) |
| Param subscribe | Implemented | BindingEditor + project variables | 2 (done) |
| Run once | Implemented | `RunController` → `IFlowScheduler` → `FlowRuntime` | 3–4 (done) |
| Run loop / stop | Implemented | Loop + stop + busy guard + monitor | 4 (done) |
| Plugin factory | Implemented | `PluginHost` + dynamic descriptor merge + example invert plugin | 4 (done) |
| Flow graphics | XFlowGraphics | CanvasScene + live run status | 4 (done) |
| Display + ROI | Partial (UI ROI unbound) | Multi-shape ROI tools + overlay filter | 5 (done for edit basics) |
| Project serialize | Incomplete | `.seltproject` directory container + manifest/resource migration; legacy `.selt` still readable | 6 (done) / reliability |
| Thread monitor | Implemented | RuntimeMonitorWidget task table + diagnostics + scope/snapshot metadata + filter/copy | 4 / reliability |
| Camera / IO modules | Planned | InputSource dock + frameId/softTrigger + StubCamera; vendor SDK via plugin | 4 / industrial |
| Global vars / subflow | Planned | Project variables done; subflow UI reserved | 2 / 11 |
| Template teach | Planned | TemplateTeachService + templateSource node + resource priority | 5 (done) |
| Measurement / calibration | Partial | Circle/line/distance/decision + CalibrationStore UI + physical units | industrial |
| Result export | Planned | CSV/JSON Lines `IResultSink` + failure isolation | industrial |
| Node visuals | Basic | Category color/icon/fold/summary | 5 (done) |
| Runtime guard / deploy | Partial | Unicode image load + template type normalization + plugin deployment diagnostics | reliability |
| Crash dump / i18n | Partial | Missing / Chinese UI hardcode | 12 |

## Explicitly Not Migrating

- Halcon dependency and HObject pipeline
- Global singleton managers everywhere
- Manual Debug/Release library path switching
- TokenMsg string bus as primary IPC
- Per-operator dedicated QWidget by default
