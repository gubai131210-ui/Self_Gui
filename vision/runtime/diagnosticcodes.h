#ifndef DIAGNOSTICCODES_H
#define DIAGNOSTICCODES_H

#include <QString>

namespace Selt {
namespace DiagnosticCodes {

inline QString imageEmpty() { return QStringLiteral("image_empty"); }
inline QString invalidRoi() { return QStringLiteral("invalid_roi"); }
inline QString imageTypeMismatch() { return QStringLiteral("image_type_mismatch"); }
inline QString invalidParameter() { return QStringLiteral("invalid_parameter"); }
inline QString opencvException() { return QStringLiteral("opencv_exception"); }
inline QString measureFailed() { return QStringLiteral("measure_failed"); }
inline QString matchFailed() { return QStringLiteral("match_failed"); }
inline QString templateMissing() { return QStringLiteral("template_missing"); }
inline QString noTarget() { return QStringLiteral("no_target"); }
inline QString regionEmpty() { return QStringLiteral("region_empty"); }
/// Degenerate geometry (collinear points, zero-length line, etc.).
inline QString degenerateGeometry() { return QStringLiteral("degenerate_geometry"); }
/// Feature is wired but intentionally limited / passthrough.
inline QString capabilityLimited() { return QStringLiteral("capability_limited"); }
/// Required decoder backend missing at compile/runtime.
inline QString backendMissing() { return QStringLiteral("backend_missing"); }
/// Input image invalid for recognition (empty / unsupported format).
inline QString imageInvalid() { return QStringLiteral("image_invalid"); }
/// Detector found no candidate regions.
inline QString noCandidate() { return QStringLiteral("no_candidate"); }
/// Candidate region found but decode failed.
inline QString decodeFailed() { return QStringLiteral("decode_failed"); }
/// Decoded but quality/confidence below threshold.
inline QString qualityLow() { return QStringLiteral("quality_low"); }
/// Active calibration missing; length results remain in pixels.
inline QString calibrationMissing() { return QStringLiteral("calibration_missing"); }
/// Operator contract is registered, but no decoder backend is bundled yet.
inline QString operatorNotImplemented() { return QStringLiteral("operator_not_implemented"); }
/// Result exists but confidence is below configured threshold.
inline QString lowConfidence() { return QStringLiteral("low_confidence"); }
/// OCR language pack (*.traineddata) missing.
inline QString languagePackMissing() { return QStringLiteral("language_pack_missing"); }
/// Operator exceeded Safety time budget.
inline QString timeout() { return QStringLiteral("timeout"); }
/// Candidate rejected by quality gates (residual/coverage/stability).
inline QString rejectedByQuality() { return QStringLiteral("rejected_by_quality"); }
/// Geometry teach missing or invalid for spatial operators.
inline QString invalidGeometry() { return QStringLiteral("invalid_geometry"); }
/// Live frame / camera grab failed or input source closed.
inline QString deviceGrabFailed() { return QStringLiteral("device_grab_failed"); }
/// Required graph input missing at publish or run preflight.
inline QString missingInput() { return QStringLiteral("missing_input"); }
inline QString commInvalidParameter() { return QStringLiteral("comm_invalid_parameter"); }
inline QString commNotAvailable() { return QStringLiteral("comm_not_available"); }
inline QString commTimeout() { return QStringLiteral("comm_timeout"); }
inline QString commWriteProtected() { return QStringLiteral("comm_write_protected"); }
inline QString commProtocolError() { return QStringLiteral("comm_protocol_error"); }
inline QString commDeviceException() { return QStringLiteral("comm_device_exception"); }

} // namespace DiagnosticCodes
} // namespace Selt

#endif // DIAGNOSTICCODES_H
