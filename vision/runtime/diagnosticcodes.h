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
/// Operator contract is registered, but no decoder backend is bundled yet.
inline QString operatorNotImplemented() { return QStringLiteral("operator_not_implemented"); }
/// Result exists but confidence is below configured threshold.
inline QString lowConfidence() { return QStringLiteral("low_confidence"); }
/// OCR language pack (*.traineddata) missing.
inline QString languagePackMissing() { return QStringLiteral("language_pack_missing"); }
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
