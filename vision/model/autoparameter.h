#ifndef AUTOPARAMETER_H
#define AUTOPARAMETER_H

#include <QJsonObject>
#include <QJsonValue>
#include <QSize>
#include <QString>
#include <QtGlobal>
#include <cmath>

namespace Selt {

/// Parameter binding state for Auto / Unset / User / Strict.
enum class ParamBindingMode {
    Unset,   ///< Key absent or sentinel; resolve to Auto / unlimited.
    Auto,    ///< Explicitly auto.
    User,    ///< User-provided value (soft constraint).
    Strict   ///< User-provided hard constraint.
};

struct AutoSafetyLimits
{
    int maxCandidates{500};
    int maxCalipers{64};
    qint64 maxTimeMs{2000};
    int minLinePoints{2};
    int minCirclePoints{3};
    double maxImageSide{8192.0};
};

inline bool isAutoSentinelDouble(double value)
{
    return !std::isfinite(value) || value == 0.0;
}

inline bool isAutoSentinelInt(int value)
{
    // 0 and negative are Auto/Unlimited sentinels for counts and thresholds.
    // Do not use resolveAutoInt for zero-based indices (use resolveIntParam).
    return value <= 0;
}

inline ParamBindingMode paramBindingMode(const QJsonObject &params, const QString &key)
{
    if (!params.contains(key))
        return ParamBindingMode::Unset;
    const QJsonValue mode = params.value(QStringLiteral("_mode_") + key);
    if (mode.isString()) {
        const QString text = mode.toString().toLower();
        if (text == QLatin1String("auto") || text == QLatin1String("unset"))
            return ParamBindingMode::Auto;
        if (text == QLatin1String("strict"))
            return ParamBindingMode::Strict;
        if (text == QLatin1String("user"))
            return ParamBindingMode::User;
    }
    return ParamBindingMode::User;
}

inline double resolveAutoDouble(const QJsonObject &params,
                                const QString &key,
                                double userFallback,
                                double autoValue)
{
    const ParamBindingMode mode = paramBindingMode(params, key);
    if (mode == ParamBindingMode::Unset || mode == ParamBindingMode::Auto)
        return autoValue;
    if (!params.contains(key))
        return autoValue;
    const double v = params.value(key).toDouble(userFallback);
    if (isAutoSentinelDouble(v) && mode != ParamBindingMode::Strict)
        return autoValue;
    return v;
}

inline int resolveAutoInt(const QJsonObject &params,
                          const QString &key,
                          int userFallback,
                          int autoValue)
{
    const ParamBindingMode mode = paramBindingMode(params, key);
    if (mode == ParamBindingMode::Unset || mode == ParamBindingMode::Auto)
        return autoValue;
    if (!params.contains(key))
        return autoValue;
    const int v = params.value(key).toInt(userFallback);
    if (isAutoSentinelInt(v) && mode != ParamBindingMode::Strict)
        return autoValue;
    return v;
}

inline QString resolveAutoString(const QJsonObject &params,
                                 const QString &key,
                                 const QString &userFallback,
                                 const QString &autoValue)
{
    const ParamBindingMode mode = paramBindingMode(params, key);
    if (mode == ParamBindingMode::Unset || mode == ParamBindingMode::Auto)
        return autoValue;
    if (!params.contains(key))
        return autoValue;
    const QString text = params.value(key).toString(userFallback);
    if ((text.isEmpty() || text.compare(QLatin1String("auto"), Qt::CaseInsensitive) == 0
         || text.compare(QLatin1String("any"), Qt::CaseInsensitive) == 0)
        && mode != ParamBindingMode::Strict) {
        return autoValue;
    }
    return text;
}

/// Suggest expected line geometry centered in image when parameters are unset.
inline void suggestDefaultLineGeometry(const QSize &imageSize, QJsonObject *params)
{
    if (!params || imageSize.width() < 2 || imageSize.height() < 2)
        return;
    const bool missing =
        !params->contains(QStringLiteral("x0")) || !params->contains(QStringLiteral("y0"))
        || !params->contains(QStringLiteral("x1")) || !params->contains(QStringLiteral("y1"));
    if (!missing)
        return;
    const double cy = imageSize.height() * 0.5;
    const double margin = imageSize.width() * 0.25;
    params->insert(QStringLiteral("x0"), margin);
    params->insert(QStringLiteral("y0"), cy);
    params->insert(QStringLiteral("x1"), imageSize.width() - margin);
    params->insert(QStringLiteral("y1"), cy);
    params->insert(QStringLiteral("_mode_x0"), QStringLiteral("auto"));
    params->insert(QStringLiteral("_mode_y0"), QStringLiteral("auto"));
    params->insert(QStringLiteral("_mode_x1"), QStringLiteral("auto"));
    params->insert(QStringLiteral("_mode_y1"), QStringLiteral("auto"));
}

inline void suggestDefaultCircleGeometry(const QSize &imageSize, QJsonObject *params)
{
    if (!params || imageSize.width() < 2 || imageSize.height() < 2)
        return;
    const bool missing = !params->contains(QStringLiteral("cx"))
        || !params->contains(QStringLiteral("cy"))
        || !params->contains(QStringLiteral("radius"));
    if (!missing)
        return;
    params->insert(QStringLiteral("cx"), imageSize.width() * 0.5);
    params->insert(QStringLiteral("cy"), imageSize.height() * 0.5);
    params->insert(QStringLiteral("radius"), qMin(imageSize.width(), imageSize.height()) * 0.25);
    params->insert(QStringLiteral("_mode_cx"), QStringLiteral("auto"));
    params->insert(QStringLiteral("_mode_cy"), QStringLiteral("auto"));
    params->insert(QStringLiteral("_mode_radius"), QStringLiteral("auto"));
}

inline void suggestDefaultCaliperGeometry(const QSize &imageSize, QJsonObject *params)
{
    if (!params || imageSize.width() < 2 || imageSize.height() < 2)
        return;
    if (params->contains(QStringLiteral("cx")) && params->contains(QStringLiteral("cy")))
        return;
    params->insert(QStringLiteral("cx"), imageSize.width() * 0.5);
    params->insert(QStringLiteral("cy"), imageSize.height() * 0.5);
    if (!params->contains(QStringLiteral("length")) || isAutoSentinelDouble(params->value(QStringLiteral("length")).toDouble()))
        params->insert(QStringLiteral("length"), qMin(imageSize.width(), imageSize.height()) * 0.2);
    if (!params->contains(QStringLiteral("width")) || isAutoSentinelDouble(params->value(QStringLiteral("width")).toDouble()))
        params->insert(QStringLiteral("width"), 8.0);
    if (!params->contains(QStringLiteral("angle")))
        params->insert(QStringLiteral("angle"), 0.0);
}

inline int autoCaliperCountForArc(double radius, double angleSpanDeg, int fallback = 12)
{
    const double span = qBound(1.0, std::abs(angleSpanDeg), 360.0);
    const double arcLen = radius * (span * 3.141592653589793 / 180.0);
    return qBound(3, int(std::lround(arcLen / 15.0)), 36);
}

inline int autoCaliperCountForLine(double lengthPx, int fallback = 8)
{
    return qBound(2, int(std::lround(lengthPx / 20.0)), 48);
}

inline double autoSearchLength(double referenceSize, double fraction = 0.35)
{
    return qMax(12.0, referenceSize * fraction);
}

/// Snapshot of resolved Auto values for reproducibility / diagnostics.
inline QJsonObject makeAutoSnapshot(const QJsonObject &resolved)
{
    QJsonObject snap;
    snap.insert(QStringLiteral("resolved"), resolved);
    snap.insert(QStringLiteral("binding"), QStringLiteral("auto"));
    return snap;
}

} // namespace Selt

#endif // AUTOPARAMETER_H
