#include "vision/algorithms/roiapplier.h"

#include "vision/algorithms/opencvguard.h"
#include "vision/runtime/diagnosticcodes.h"

#include <opencv2/imgproc.hpp>

namespace Selt {
namespace {

cv::Mat buildMask(const cv::Mat &src, const ExtendedRoi &roi)
{
    cv::Mat mask = cv::Mat::zeros(src.size(), CV_8UC1);
    switch (roi.shape) {
    case RoiShapeType::Circle: {
        const int r = qMax(1, int(std::lround(roi.radius)));
        cv::circle(mask, cv::Point(int(std::lround(roi.center.x())), int(std::lround(roi.center.y()))),
                   r, cv::Scalar(255), cv::FILLED);
        break;
    }
    case RoiShapeType::Ellipse:
    case RoiShapeType::RotatedRect: {
        cv::RotatedRect rr(cv::Point2f(float(roi.center.x()), float(roi.center.y())),
                           cv::Size2f(float(qMax(1.0, roi.width)), float(qMax(1.0, roi.height))),
                           float(roi.angleDeg));
        cv::Point2f pts[4];
        rr.points(pts);
        std::vector<cv::Point> poly;
        poly.reserve(4);
        for (const cv::Point2f &p : pts)
            poly.emplace_back(cv::Point(int(std::lround(p.x)), int(std::lround(p.y))));
        cv::fillConvexPoly(mask, poly, cv::Scalar(255));
        break;
    }
    case RoiShapeType::Polygon: {
        if (roi.polygon.size() < 3)
            break;
        std::vector<cv::Point> poly;
        poly.reserve(size_t(roi.polygon.size()));
        for (const QPointF &p : roi.polygon)
            poly.emplace_back(cv::Point(int(std::lround(p.x())), int(std::lround(p.y()))));
        const cv::Point *pts = poly.data();
        const int npts = int(poly.size());
        cv::fillPoly(mask, &pts, &npts, 1, cv::Scalar(255));
        break;
    }
    case RoiShapeType::Rectangle:
    default: {
        const QRectF box = roi.boundingRect().intersected(QRectF(0, 0, src.cols, src.rows));
        if (box.width() >= 1.0 && box.height() >= 1.0) {
            cv::rectangle(mask,
                          cv::Rect(int(box.x()), int(box.y()), int(box.width()), int(box.height())),
                          cv::Scalar(255), cv::FILLED);
        }
        break;
    }
    }
    return mask;
}

} // namespace

ExtendedRoi RoiApplier::parseFromParameters(const QJsonObject &params)
{
    return parseFromParameters(params, false);
}

ExtendedRoi RoiApplier::parseFromParameters(const QJsonObject &params, bool preferParameterRoi)
{
    if (preferParameterRoi) {
        const RoiRect legacy = RoiRect::fromJson(params.value(QStringLiteral("roi")).toObject());
        ExtendedRoi fromLegacy = ExtendedRoi::fromLegacyRect(legacy);
        if (fromLegacy.isValid())
            return fromLegacy;
    }
    if (params.contains(QStringLiteral("extendedRoi"))) {
        ExtendedRoi roi = ExtendedRoi::fromJson(params.value(QStringLiteral("extendedRoi")).toObject());
        if (roi.isValid())
            return roi;
    }
    const RoiRect legacy = RoiRect::fromJson(params.value(QStringLiteral("roi")).toObject());
    return ExtendedRoi::fromLegacyRect(legacy);
}

RoiApplyResult RoiApplier::apply(const VisionImage &input, const ExtendedRoi &roi, RoiApplyMode mode)
{
    RoiApplyResult out;
    if (input.isEmpty()) {
        out.errorMessage = QStringLiteral("ROI 输入图像为空");
        out.diagnosticCode = DiagnosticCodes::imageEmpty();
        return out;
    }
    if (!roi.isValid()) {
        out.ok = true;
        out.image = input;
        out.originOffset = QPointF(0.0, 0.0);
        return out;
    }

    ExtendedRoi clamped = roi;
    clamped.normalize();
    clamped.clampToImage(QSize(input.mat().cols, input.mat().rows));
    if (!clamped.isValid()) {
        out.errorMessage = QStringLiteral("ROI 无效或越界");
        out.diagnosticCode = DiagnosticCodes::invalidRoi();
        return out;
    }

    QString error;
    const bool ran = runOpenCv([&]() {
        const cv::Mat &src = input.mat();
        if (mode == RoiApplyMode::Crop || mode == RoiApplyMode::CropMasked) {
            const QRectF box = clamped.boundingRect().intersected(QRectF(0, 0, src.cols, src.rows));
            if (box.width() < 1.0 || box.height() < 1.0) {
                out.errorMessage = QStringLiteral("ROI 裁剪区域无效");
                out.diagnosticCode = DiagnosticCodes::invalidRoi();
                return;
            }
            const cv::Rect cvRoi(int(box.x()), int(box.y()), int(box.width()), int(box.height()));
            cv::Mat cropped = src(cvRoi).clone();
            if (mode == RoiApplyMode::CropMasked && clamped.shape != RoiShapeType::Rectangle) {
                ExtendedRoi local = clamped;
                local.clampToImage(QSize(src.cols, src.rows));
                // Shift geometry into cropped coordinates.
                if (local.shape == RoiShapeType::Circle || local.shape == RoiShapeType::Ellipse
                    || local.shape == RoiShapeType::RotatedRect) {
                    local.center -= QPointF(box.x(), box.y());
                } else if (local.shape == RoiShapeType::Polygon) {
                    for (QPointF &p : local.polygon)
                        p -= QPointF(box.x(), box.y());
                } else {
                    local.rect = QRectF(0, 0, box.width(), box.height());
                }
                cv::Mat mask = buildMask(cropped, local);
                if (cv::countNonZero(mask) == 0) {
                    out.errorMessage = QStringLiteral("ROI 掩膜为空");
                    out.diagnosticCode = DiagnosticCodes::invalidRoi();
                    return;
                }
                cv::Mat masked = cv::Mat::zeros(cropped.size(), cropped.type());
                cropped.copyTo(masked, mask);
                cropped = masked;
            }
            out.image = VisionImage(cropped, input.sourcePath());
            out.ok = true;
            out.appliedRoi = clamped;
            out.originOffset = QPointF(box.x(), box.y());
            return;
        }

        cv::Mat mask = buildMask(src, clamped);
        if (cv::countNonZero(mask) == 0) {
            out.errorMessage = QStringLiteral("ROI 掩膜为空");
            out.diagnosticCode = DiagnosticCodes::invalidRoi();
            return;
        }
        cv::Mat masked = cv::Mat::zeros(src.size(), src.type());
        src.copyTo(masked, mask);
        out.image = VisionImage(masked, input.sourcePath());
        out.ok = true;
        out.appliedRoi = clamped;
        out.originOffset = QPointF(0.0, 0.0);
    }, &error);

    if (!ran) {
        out.ok = false;
        out.errorMessage = error;
        out.diagnosticCode = DiagnosticCodes::opencvException();
    }
    return out;
}

RoiApplyResult RoiApplier::applyFromParameters(const VisionImage &input,
                                               const QJsonObject &params,
                                               RoiApplyMode mode)
{
    const bool preferParamRoi = params.value(QStringLiteral("_preferParameterRoi")).toBool(false);
    return apply(input, parseFromParameters(params, preferParamRoi), mode);
}

} // namespace Selt
