# OpenCV Substitution Matrix

| XVision / Halcon capability | OpenCV substitute in Selt | Phase | Status |
|-----------------------------|---------------------------|-------|--------|
| Image load (file/dir) | `cv::imread`, directory sequencer | 9/10 | Implemented |
| Threshold / morph / edge | `imgproc` + Manual/Otsu/Adaptive/Sobel/Gaussian/Median/Bilateral | O1 + factor | Implemented + tests |
| Shape/template match | `matchTemplate` multi-peak NMS + IoU, ORB+Lowe+RANSAC homography, Hough params exposed | O3 + factor | Implemented; real subpixel via `cornerSubPix` / peak fit |
| Metrology line/circle | algebraic / RANSAC fit, caliper interpolated gradient + dual-edge scoring | O4 + factor | Implemented + calibration + factor DAG tests |
| Region paint / mask | Contour/Region/Table ports + connectedComponents/Blob filters (aspect/extent/solidity) | O2 + factor | Implemented |
| Logic / batch / sink | bool/compare/counter/aggregate + ResultSink writer | O5 | Implemented |
| Camera grab | `VideoCapture` + vendor plugins later | 10 | Stub / deferred |
| Calibration | `CalibrationModel` / store + runtime snapshot | ADR-0010 | Implemented; missing cal keeps `px` |

## Algorithm factor notes

| Node | Modes / params | Precision boundary | Failure diagnostics |
|------|----------------|--------------------|---------------------|
| `circleMeasure` / `rectangleMeasure` | `thresholdStrategy` = manual/otsu/adaptive | Clean synthetic diameter/width ≤ 1 px | `image_empty` / `invalid_roi` / `measure_failed` (+ strategy in message) |
| `caliperMeasure` | sampleStep, smoothSigma, polarity, gradientThreshold, minEdgeGap, **secondPeakRatio** | Clean 70 px bar ≤ 1.5 px; confidence on dual edges | `measure_failed` / `invalid_parameter` |
| `fitCircle` / `fitLine` | `fitMode` algebraic\|ransac; contourSelect; **minInliers** | RANSAC recovers with 10–20% outliers; collinear rejected | `degenerate_geometry` / `measure_failed` |
| `houghLines` / `houghCircles` | accumulator / length / gap / dp / minDist / maxCount | First output remains primary; `candidateCount`/`confidence` added | `no_target` |
| `blobAnalyze` / `blobLocate` | aspect/extent/solidity + sortBy | Sorted candidates; first selected | `no_target` / `match_failed` |
| `featureMatch` | Lowe ratio + RANSAC homography | Score blends distance + inlier ratio | `match_failed` / `template_missing` |
| `templateMatchMulti` | NMS IoU / center distance | Multi peaks with explicit count | `match_failed` |
| `subpixelRefine` | corner / peak; terminateEps/maxIterations; optional image; `passthrough` compat | With image: local refine; without image: passthrough + `capability_limited` | `measure_failed` / `capability_limited` |
| preprocess morph/canny | iterations / apertureSize / roiApplyMode(mask\|crop) | Defaults match legacy behavior | `image_empty` / `invalid_roi` |
| `distanceMeasure` / `parallelDistance` / angle | Geometry | Length calibrated when snapshot valid; angle stays `deg` | `degenerate_geometry` |

## VisionMaster UX notes

- Parameter schema remains registry-driven (`VisionNodeRegistry`); new keys are additive with safe defaults for old `.selt`.
- Workflow templates: `measure_basic` / `locate_measure` / `caliper_decision` / `full_chain` via `Selt::WorkflowTemplates`.
- Auto layout uses DAG levels left-to-right; selection highlights related connections in orange.
- Color tokens: `Selt::UiStyle` + `theme_dark.qss`; dark mode is the default industrial workspace.

Rule: no Halcon types may cross SDK/runtime boundaries.
No silent physical units without a valid calibration snapshot.
File benchmark numbers are recorded by `Selt_Gui_algorithm_benchmark_tests` and are **not** Release production SLAs.
