#include "vision/algorithms/recognitionalgorithms.h"

#include "vision/runtime/diagnosticcodes.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QSet>
#include <QtMath>

#include <opencv2/imgproc.hpp>

#if defined(SELT_HAS_OPENCV_QR) && SELT_HAS_OPENCV_QR
#include <opencv2/objdetect.hpp>
#endif
#if defined(SELT_HAS_OPENCV_BARCODE) && SELT_HAS_OPENCV_BARCODE
#include <opencv2/objdetect/barcode.hpp>
#endif
#if defined(SELT_HAS_TESSERACT) && SELT_HAS_TESSERACT
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#endif

namespace Selt {
namespace {

struct CandidateView {
    cv::Mat mat;
    double scale{1.0};
    int rotateCode{-1}; ///< -1 none, else cv::RotateFlags
    QString strategy;
};

OverlayItem makeTextOverlay(const QString &text, const QRectF &rect)
{
    OverlayItem item;
    item.type = OverlayItemType::Text;
    item.text = text;
    item.rect = rect;
    item.color = QColor(0, 220, 80);
    item.penWidth = 2.0;
    return item;
}

OverlayItem makeQuadOverlay(const QVector<QPointF> &corners, const QColor &color = QColor(255, 140, 0))
{
    OverlayItem item;
    item.type = OverlayItemType::Contour;
    item.points = corners;
    item.color = color;
    item.penWidth = 2.0;
    if (!corners.isEmpty()) {
        qreal minX = corners.first().x();
        qreal minY = corners.first().y();
        qreal maxX = minX;
        qreal maxY = minY;
        for (const QPointF &p : corners) {
            minX = qMin(minX, p.x());
            minY = qMin(minY, p.y());
            maxX = qMax(maxX, p.x());
            maxY = qMax(maxY, p.y());
        }
        item.rect = QRectF(QPointF(minX, minY), QPointF(maxX, maxY));
    }
    return item;
}

QRectF rectFromCorners(const QVector<QPointF> &corners)
{
    if (corners.isEmpty())
        return {};
    qreal minX = corners.first().x();
    qreal minY = corners.first().y();
    qreal maxX = minX;
    qreal maxY = minY;
    for (const QPointF &p : corners) {
        minX = qMin(minX, p.x());
        minY = qMin(minY, p.y());
        maxX = qMax(maxX, p.x());
        maxY = qMax(maxY, p.y());
    }
    return QRectF(QPointF(minX, minY), QPointF(maxX, maxY));
}

/// 可选：把 overlay 烘焙进图像（导出/报告用）。预览路径不要用，否则会与 Overlay 层重影。
[[maybe_unused]] VisionImage drawOverlayImage(const VisionImage &input, const OverlayList &overlays)
{
    if (input.isEmpty())
        return {};
    cv::Mat display;
    if (input.mat().channels() == 1)
        cv::cvtColor(input.mat(), display, cv::COLOR_GRAY2BGR);
    else if (input.mat().channels() == 4)
        cv::cvtColor(input.mat(), display, cv::COLOR_BGRA2BGR);
    else
        display = input.mat().clone();

    for (const OverlayItem &item : overlays) {
        if (item.type == OverlayItemType::Contour && item.points.size() >= 2) {
            std::vector<cv::Point> pts;
            for (const QPointF &p : item.points)
                pts.emplace_back(int(std::lround(p.x())), int(std::lround(p.y())));
            const cv::Point *ptr = pts.data();
            int n = int(pts.size());
            cv::polylines(display, &ptr, &n, 1, true,
                          cv::Scalar(item.color.blue(), item.color.green(), item.color.red()), 2);
        } else if (item.type == OverlayItemType::Rectangle && item.rect.isValid()) {
            cv::rectangle(display,
                          cv::Rect(int(item.rect.x()), int(item.rect.y()),
                                   int(item.rect.width()), int(item.rect.height())),
                          cv::Scalar(0, 220, 80), 2);
        }
        if (!item.text.isEmpty()) {
            const QPointF origin = item.rect.isValid() ? item.rect.topLeft()
                                                      : (item.points.isEmpty() ? QPointF(8, 20)
                                                                              : item.points.first());
            cv::putText(display, item.text.toStdString(),
                        cv::Point(int(origin.x()), int(origin.y()) - 4),
                        cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 220, 80), 1, cv::LINE_AA);
        }
    }
    return VisionImage(display, input.sourcePath());
}

/// 预览输出图：只克隆原图，不把框/文字烤进像素。
/// 叠加由 OverlayList 交给 ImagePreview 绘制，避免“结果图烘焙 + Overlay 再画”造成重影。
VisionImage previewImageWithoutBurnIn(const VisionImage &input)
{
    if (input.isEmpty())
        return {};
    return input.clone();
}

void finalizeJoined(RecognitionResult &out)
{
    QStringList texts;
    QStringList types;
    double confSum = 0.0;
    double best = 0.0;
    int bestIndex = -1;
    for (int i = 0; i < out.hits.size(); ++i) {
        const RecognitionHit &hit = out.hits.at(i);
        if (!hit.text.isEmpty())
            texts.append(hit.text);
        if (!hit.symbology.isEmpty() && !types.contains(hit.symbology))
            types.append(hit.symbology);
        confSum += hit.confidence;
        const double score = hit.score > 0.0 ? hit.score : hit.confidence;
        if (score >= best) {
            best = score;
            bestIndex = i;
        }
        if (!hit.corners.isEmpty())
            out.overlays.append(makeQuadOverlay(hit.corners));
        else if (hit.boundingRect.isValid()) {
            OverlayItem box;
            box.type = OverlayItemType::Rectangle;
            box.rect = hit.boundingRect;
            box.color = QColor(255, 140, 0);
            out.overlays.append(box);
        }
        if (!hit.text.isEmpty()) {
            const QRectF labelRect = hit.boundingRect.isValid()
                ? hit.boundingRect
                : rectFromCorners(hit.corners);
            out.overlays.append(makeTextOverlay(hit.text, labelRect));
        }
    }
    out.joinedText = texts.join(QLatin1Char('\n'));
    out.symbologies = types;
    out.confidence = out.hits.isEmpty() ? 0.0 : confSum / double(out.hits.size());
    out.bestScore = best;
    out.found = !out.hits.isEmpty();
    if (bestIndex >= 0) {
        out.corners = out.hits.at(bestIndex).corners;
        out.boundingRect = out.hits.at(bestIndex).boundingRect;
        if (!out.boundingRect.isValid() && !out.corners.isEmpty())
            out.boundingRect = rectFromCorners(out.corners);
    }
}

cv::Mat toGray8u(const cv::Mat &src)
{
    cv::Mat gray;
    if (src.empty())
        return gray;
    if (src.channels() == 1)
        gray = src;
    else if (src.channels() == 4)
        cv::cvtColor(src, gray, cv::COLOR_BGRA2GRAY);
    else
        cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    if (gray.depth() != CV_8U)
        gray.convertTo(gray, CV_8U);
    return gray;
}

cv::Mat toBgr8u(const cv::Mat &src)
{
    cv::Mat bgr;
    if (src.empty())
        return bgr;
    if (src.channels() == 3)
        bgr = src;
    else if (src.channels() == 1)
        cv::cvtColor(src, bgr, cv::COLOR_GRAY2BGR);
    else if (src.channels() == 4)
        cv::cvtColor(src, bgr, cv::COLOR_BGRA2BGR);
    else
        cv::cvtColor(src, bgr, cv::COLOR_GRAY2BGR);
    if (bgr.depth() != CV_8U)
        bgr.convertTo(bgr, CV_8U);
    return bgr;
}

QPointF mapPointToOriginal(const QPointF &p, const CandidateView &view, int candW, int candH)
{
    double x = p.x();
    double y = p.y();
    // Undo rotation first (coordinates are in rotated image space).
    if (view.rotateCode == cv::ROTATE_90_CLOCKWISE) {
        // dst(x,y)=src(h-1-y,x); dst is H×W if src is W×H → candW=H, candH=W
        const double srcX = y;
        const double srcY = double(candW - 1) - x;
        x = srcX;
        y = srcY;
    } else if (view.rotateCode == cv::ROTATE_180) {
        x = double(candW - 1) - x;
        y = double(candH - 1) - y;
    } else if (view.rotateCode == cv::ROTATE_90_COUNTERCLOCKWISE) {
        // dst(x,y)=src(y,w-1-x); candW=H, candH=W
        const double srcX = y;
        const double srcY = double(candH - 1) - x;
        x = srcX;
        y = srcY;
    }
    if (view.scale > 1e-6) {
        x /= view.scale;
        y /= view.scale;
    }
    return QPointF(x, y);
}

void mapHitToOriginal(RecognitionHit &hit, const CandidateView &view)
{
    if (hit.corners.isEmpty() && !hit.boundingRect.isValid())
        return;
    const int candW = view.mat.cols;
    const int candH = view.mat.rows;
    QVector<QPointF> mapped;
    mapped.reserve(hit.corners.size());
    for (const QPointF &c : hit.corners)
        mapped.append(mapPointToOriginal(c, view, candW, candH));
    hit.corners = mapped;
    if (!mapped.isEmpty())
        hit.boundingRect = rectFromCorners(mapped);
    else if (hit.boundingRect.isValid()) {
        const QPointF tl = mapPointToOriginal(hit.boundingRect.topLeft(), view, candW, candH);
        const QPointF br = mapPointToOriginal(hit.boundingRect.bottomRight(), view, candW, candH);
        hit.boundingRect = QRectF(tl, br).normalized();
    }
}

bool cornersLookValid(const QVector<QPointF> &corners, int imageW, int imageH)
{
    if (corners.size() < 3)
        return true; // barcode may have fewer points
    QRectF box = rectFromCorners(corners);
    if (!box.isValid() || box.width() < 4.0 || box.height() < 4.0)
        return false;
    if (box.width() > imageW * 1.2 || box.height() > imageH * 1.2)
        return false;
    return true;
}

double scoreHit(const RecognitionHit &hit)
{
    double s = hit.confidence;
    if (!hit.text.isEmpty())
        s += 0.15;
    if (hit.corners.size() >= 4)
        s += 0.1;
    if (hit.boundingRect.isValid())
        s += 0.05;
    return qBound(0.0, 1.5, s);
}

QString hitKey(const RecognitionHit &hit)
{
    return hit.symbology + QLatin1Char('|') + hit.text.trimmed();
}

bool nearlySameGeometry(const RecognitionHit &a, const RecognitionHit &b)
{
    if (!a.boundingRect.isValid() || !b.boundingRect.isValid())
        return false;
    const QRectF ia = a.boundingRect.intersected(b.boundingRect);
    if (!ia.isValid())
        return false;
    const double areaA = qMax(1.0, a.boundingRect.width() * a.boundingRect.height());
    const double areaB = qMax(1.0, b.boundingRect.width() * b.boundingRect.height());
    const double inter = ia.width() * ia.height();
    return (inter / areaA) > 0.55 && (inter / areaB) > 0.55;
}

void appendUniqueHits(QVector<RecognitionHit> &dst, const QVector<RecognitionHit> &src, int maxCount)
{
    QSet<QString> seen;
    for (const RecognitionHit &h : dst)
        seen.insert(hitKey(h));
    for (RecognitionHit h : src) {
        if (dst.size() >= maxCount)
            break;
        h.score = scoreHit(h);
        const QString key = hitKey(h);
        if (seen.contains(key))
            continue;
        bool geometricDup = false;
        for (const RecognitionHit &existing : dst) {
            if (existing.symbology == h.symbology && nearlySameGeometry(existing, h)) {
                geometricDup = true;
                break;
            }
        }
        if (geometricDup)
            continue;
        seen.insert(key);
        dst.append(h);
    }
}

CandidateView makeCandidate(const cv::Mat &baseGray, double scale, int rotateCode, const QString &name)
{
    CandidateView view;
    view.scale = scale;
    view.rotateCode = rotateCode;
    view.strategy = name;
    cv::Mat scaled = baseGray;
    if (qAbs(scale - 1.0) > 1e-3) {
        cv::resize(baseGray, scaled, cv::Size(), scale, scale, cv::INTER_CUBIC);
    }
    if (rotateCode >= 0) {
        cv::Mat rotated;
        cv::rotate(scaled, rotated, rotateCode);
        view.mat = toBgr8u(rotated);
    } else {
        view.mat = toBgr8u(scaled);
    }
    return view;
}

void pushCandidate(QVector<CandidateView> &out, QSet<QString> &seen, const CandidateView &c, int maxAttempts)
{
    if (out.size() >= maxAttempts || c.mat.empty())
        return;
    if (seen.contains(c.strategy))
        return;
    seen.insert(c.strategy);
    out.append(c);
}

void pushScaled(QVector<CandidateView> &out,
                QSet<QString> &seen,
                const cv::Mat &base,
                double baseScale,
                const QString &prefix,
                int maxAttempts,
                bool fullEnhance)
{
    const QList<double> scales = fullEnhance
        ? QList<double>{1.0, 1.5, 2.0}
        : QList<double>{1.0, 1.5};
    for (double s : scales) {
        const double effective = qMax(0.5, baseScale) * s;
        pushCandidate(out, seen,
                      makeCandidate(base, effective, -1, prefix + QStringLiteral("@x%1").arg(s, 0, 'f', 1)),
                      maxAttempts);
    }
}

void pushRotateFamily(QVector<CandidateView> &out,
                      QSet<QString> &seen,
                      const cv::Mat &base,
                      double baseScale,
                      const QString &prefix,
                      int maxAttempts,
                      bool fullEnhance)
{
    const QList<double> scales = fullEnhance
        ? QList<double>{1.0, 1.5}
        : QList<double>{1.0};
    for (double s : scales) {
        const double effective = qMax(0.5, baseScale) * s;
        pushCandidate(out, seen,
                      makeCandidate(base, effective, cv::ROTATE_90_CLOCKWISE,
                                    prefix + QStringLiteral("@x%1_r90").arg(s, 0, 'f', 1)),
                      maxAttempts);
        if (fullEnhance) {
            pushCandidate(out, seen,
                          makeCandidate(base, effective, cv::ROTATE_180,
                                        prefix + QStringLiteral("@x%1_r180").arg(s, 0, 'f', 1)),
                          maxAttempts);
            pushCandidate(out, seen,
                          makeCandidate(base, effective, cv::ROTATE_90_COUNTERCLOCKWISE,
                                        prefix + QStringLiteral("@x%1_r270").arg(s, 0, 'f', 1)),
                          maxAttempts);
        }
    }
}

QVector<CandidateView> buildCandidates(const cv::Mat &src, const BarcodeQrAlgorithm::Options &options)
{
    QVector<CandidateView> candidates;
    QSet<QString> seen;
    const int maxAttempts = qMax(1, options.maxAttempts);
    const cv::Mat gray = toGray8u(src);
    if (gray.empty())
        return candidates;

    const QString mode = options.enhanceMode.trimmed().toLower();
    const bool wantEnhance = mode != QLatin1String("none");
    const bool fullEnhance = mode == QLatin1String("full");
    const double baseScale = qMax(0.5, options.scale);

    // 1) 原图快速识别
    pushCandidate(candidates, seen,
                  makeCandidate(gray, baseScale, -1, QStringLiteral("original")),
                  maxAttempts);

    if (!wantEnhance && !options.tryRotate)
        return candidates;
    // Skip x2 upscale on large images (e.g. 4K -> 8K).
    const bool allowScale2 = fullEnhance && qMax(gray.cols, gray.rows) < 2000;


    // 2) 灰度/缩放（无图额外倍率；full/auto 增强阶段会继续）
    if (wantEnhance) {
        pushCandidate(candidates, seen,
                      makeCandidate(gray, baseScale * 1.5, -1, QStringLiteral("gray@x1.5")),
                      maxAttempts);
        if (allowScale2) {
            pushCandidate(candidates, seen,
                          makeCandidate(gray, baseScale * 2.0, -1, QStringLiteral("gray@x2.0")),
                          maxAttempts);
        }
    }

    // 3) CLAHE
    if (wantEnhance && options.tryClahe) {
        cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(2.0, cv::Size(8, 8));
        cv::Mat claheMat;
        clahe->apply(gray, claheMat);
        pushScaled(candidates, seen, claheMat, baseScale, QStringLiteral("clahe"),
                   maxAttempts, allowScale2);
    }

    // 4) 锐化 / 轻度去噪
    if (wantEnhance && options.trySharpen) {
        cv::Mat blur;
        cv::GaussianBlur(gray, blur, cv::Size(0, 0), 1.0);
        cv::Mat sharp;
        cv::addWeighted(gray, 1.5, blur, -0.5, 0, sharp);
        pushScaled(candidates, seen, sharp, baseScale, QStringLiteral("sharpen"),
                   maxAttempts, allowScale2);
    }
    if (wantEnhance && options.tryDenoise) {
        cv::Mat denoised;
        cv::medianBlur(gray, denoised, 3);
        pushScaled(candidates, seen, denoised, baseScale, QStringLiteral("denoise"),
                   maxAttempts, allowScale2);
    }

    // 5) 自适应阈值
    if (wantEnhance && options.tryAdaptive) {
        cv::Mat bin;
        cv::adaptiveThreshold(gray, bin, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C,
                              cv::THRESH_BINARY, 31, 5);
        pushScaled(candidates, seen, bin, baseScale, QStringLiteral("adaptive"),
                   maxAttempts, allowScale2);
    }

    // 6) 反色
    if (wantEnhance && options.tryInvert) {
        cv::Mat inv;
        cv::bitwise_not(gray, inv);
        pushScaled(candidates, seen, inv, baseScale, QStringLiteral("invert"),
                   maxAttempts, allowScale2);
        if (options.tryAdaptive) {
            cv::Mat bin;
            cv::adaptiveThreshold(inv, bin, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C,
                                  cv::THRESH_BINARY, 31, 5);
            pushScaled(candidates, seen, bin, baseScale, QStringLiteral("invert_adaptive"),
                       maxAttempts, allowScale2);
        }
    }

    // 7) 旋转候选（放最后，避免拖慢清晰图）
    if (options.tryRotate) {
        pushRotateFamily(candidates, seen, gray, baseScale, QStringLiteral("original"),
                         maxAttempts, allowScale2);
        if (wantEnhance && options.tryClahe) {
            cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(2.0, cv::Size(8, 8));
            cv::Mat claheMat;
            clahe->apply(gray, claheMat);
            pushRotateFamily(candidates, seen, claheMat, baseScale, QStringLiteral("clahe"),
                             maxAttempts, allowScale2);
        }
    }
    return candidates;
}

enum class DecodeOutcome {
    None,
    DetectedOnly,
    Decoded
};

#if defined(SELT_HAS_OPENCV_QR) && SELT_HAS_OPENCV_QR
DecodeOutcome decodeQr(const cv::Mat &mat, int maxCount, QVector<RecognitionHit> &hits, bool *detectedOut)
{
    if (detectedOut)
        *detectedOut = false;
    try {
        cv::QRCodeDetector detector;
        std::vector<cv::Point> detectPts;
        const bool detected = detector.detect(mat, detectPts);
        if (detected && detectedOut)
            *detectedOut = true;

        std::vector<std::string> decoded;
        cv::Mat points;
        if (detector.detectAndDecodeMulti(mat, decoded, points)) {
            const int count = qMin(int(decoded.size()), maxCount);
            for (int i = 0; i < count; ++i) {
                const QString text = QString::fromStdString(decoded[size_t(i)]).trimmed();
                if (text.isEmpty())
                    continue;
                RecognitionHit hit;
                hit.text = text;
                hit.symbology = QStringLiteral("qr");
                hit.confidence = 1.0;
                if (!points.empty() && points.rows > i && points.channels() >= 2) {
                    for (int c = 0; c < points.cols; ++c) {
                        const cv::Point2f p = points.at<cv::Point2f>(i, c);
                        hit.corners.append(QPointF(p.x, p.y));
                    }
                    hit.boundingRect = rectFromCorners(hit.corners);
                }
                hits.append(hit);
            }
            if (!hits.isEmpty())
                return DecodeOutcome::Decoded;
        }

        if (detected) {
            // Single detect+decode fallback
            std::string info;
            std::vector<cv::Point> pts = detectPts;
            info = detector.decode(mat, pts);
            if (!info.empty()) {
                RecognitionHit hit;
                hit.text = QString::fromStdString(info).trimmed();
                hit.symbology = QStringLiteral("qr");
                hit.confidence = 0.95;
                for (const cv::Point &p : pts)
                    hit.corners.append(QPointF(p.x, p.y));
                hit.boundingRect = rectFromCorners(hit.corners);
                if (!hit.text.isEmpty()) {
                    hits.append(hit);
                    return DecodeOutcome::Decoded;
                }
            }
            return DecodeOutcome::DetectedOnly;
        }
        return DecodeOutcome::None;
    } catch (const cv::Exception &) {
        return DecodeOutcome::None;
    }
}
#endif

#if defined(SELT_HAS_OPENCV_BARCODE) && SELT_HAS_OPENCV_BARCODE
DecodeOutcome decodeBarcode(const cv::Mat &mat, int maxCount, QVector<RecognitionHit> &hits, bool *detectedOut)
{
    if (detectedOut)
        *detectedOut = false;
    try {
        cv::barcode::BarcodeDetector detector;
        std::vector<std::string> decoded;
        std::vector<std::string> types;
        cv::Mat points;
        if (!detector.detectAndDecodeWithType(mat, decoded, types, points))
            return DecodeOutcome::None;
        if (detectedOut)
            *detectedOut = !points.empty() || !decoded.empty();
        const int count = qMin(int(decoded.size()), maxCount);
        for (int i = 0; i < count; ++i) {
            const QString text = QString::fromStdString(decoded[size_t(i)]).trimmed();
            if (text.isEmpty())
                continue;
            RecognitionHit hit;
            hit.text = text;
            hit.symbology = i < int(types.size())
                ? QString::fromStdString(types[size_t(i)])
                : QStringLiteral("barcode");
            hit.confidence = 1.0;
            if (!points.empty() && points.rows > i && points.channels() >= 2) {
                for (int c = 0; c < points.cols; ++c) {
                    const cv::Point2f p = points.at<cv::Point2f>(i, c);
                    hit.corners.append(QPointF(p.x, p.y));
                }
                hit.boundingRect = rectFromCorners(hit.corners);
            }
            hits.append(hit);
        }
        if (!hits.isEmpty())
            return DecodeOutcome::Decoded;
        if (detectedOut && *detectedOut)
            return DecodeOutcome::DetectedOnly;
        return DecodeOutcome::None;
    } catch (const cv::Exception &) {
        return DecodeOutcome::None;
    }
}
#endif

} // namespace

bool RecognitionCapability::hasQrBackend()
{
#if defined(SELT_HAS_OPENCV_QR) && SELT_HAS_OPENCV_QR
    return true;
#else
    return false;
#endif
}

bool RecognitionCapability::hasBarcodeBackend()
{
#if defined(SELT_HAS_OPENCV_BARCODE) && SELT_HAS_OPENCV_BARCODE
    return true;
#else
    return false;
#endif
}

bool RecognitionCapability::hasOcrBackend()
{
#if defined(SELT_HAS_TESSERACT) && SELT_HAS_TESSERACT
    return true;
#else
    return false;
#endif
}

QString RecognitionCapability::qrStatusText()
{
    return hasQrBackend()
        ? QStringLiteral("OpenCV QRCodeDetector 可用")
        : QStringLiteral("缺少 OpenCV objdetect / QRCodeDetector");
}

QString RecognitionCapability::barcodeStatusText()
{
    return hasBarcodeBackend()
        ? QStringLiteral("OpenCV BarcodeDetector 可用")
        : QStringLiteral("缺少 OpenCV barcode 模块（条码模式将降级）");
}

QString RecognitionCapability::ocrStatusText()
{
    return hasOcrBackend()
        ? QStringLiteral("Tesseract 可用")
        : QStringLiteral("未找到 Tesseract；OCR 节点将报告能力受限");
}

QString RecognitionCapability::combinedStatusText()
{
    return QStringLiteral("QR: %1; Barcode: %2")
        .arg(hasQrBackend() ? QStringLiteral("OK") : QStringLiteral("缺失"),
             hasBarcodeBackend() ? QStringLiteral("OK") : QStringLiteral("缺失"));
}

bool BarcodeQrAlgorithm::apply(const VisionImage &input,
                               const Options &options,
                               RecognitionResult &out,
                               QString *error)
{
    out = RecognitionResult{};
    out.backendStatus = RecognitionCapability::combinedStatusText();
    out.failureStage = QStringLiteral("input");

    if (input.isEmpty() || input.mat().empty()) {
        out.diagnosticCode = DiagnosticCodes::imageInvalid();
        out.errorMessage = QStringLiteral("输入图像无效或为空");
        if (error)
            *error = out.errorMessage;
        return false;
    }

    const QString mode = options.symbology.trimmed().toLower();
    const bool wantQr = mode == QLatin1String("auto") || mode == QLatin1String("qr");
    const bool wantBarcode = mode == QLatin1String("auto") || mode == QLatin1String("barcode");

    if (wantQr && !RecognitionCapability::hasQrBackend()
        && !(wantBarcode && RecognitionCapability::hasBarcodeBackend())) {
        out.failureStage = QStringLiteral("backend");
        out.diagnosticCode = DiagnosticCodes::backendMissing();
        out.errorMessage = RecognitionCapability::qrStatusText();
        if (error)
            *error = out.errorMessage;
        return false;
    }
    if (mode == QLatin1String("barcode") && !RecognitionCapability::hasBarcodeBackend()) {
        out.failureStage = QStringLiteral("backend");
        out.diagnosticCode = DiagnosticCodes::backendMissing();
        out.errorMessage = RecognitionCapability::barcodeStatusText();
        if (error)
            *error = out.errorMessage;
        return false;
    }
    if (mode == QLatin1String("qr") && !RecognitionCapability::hasQrBackend()) {
        out.failureStage = QStringLiteral("backend");
        out.diagnosticCode = DiagnosticCodes::backendMissing();
        out.errorMessage = RecognitionCapability::qrStatusText();
        if (error)
            *error = out.errorMessage;
        return false;
    }

    Options effective = options;
    if (effective.tryRotate && effective.enhanceMode.trimmed().toLower() == QLatin1String("none"))
        effective.enhanceMode = QStringLiteral("auto");

    const QString enhance = effective.enhanceMode.trimmed().toLower();
    QElapsedTimer budget;
    budget.start();
    const int maxTimeMs = qMax(0, effective.maxTimeMs);

    QVector<CandidateView> candidates;
    if (enhance == QLatin1String("full")) {
        candidates = buildCandidates(input.mat(), effective);
    } else if (enhance == QLatin1String("none")) {
        Options fast = effective;
        fast.tryRotate = false;
        candidates = buildCandidates(input.mat(), fast);
    } else {
        // auto: 先原图 / 灰度缩放 / 可选单次旋转，失败后再完整增强。
        Options fast = effective;
        fast.enhanceMode = QStringLiteral("none");
        fast.tryClahe = false;
        fast.tryAdaptive = false;
        fast.trySharpen = false;
        fast.tryInvert = false;
        fast.tryDenoise = false;
        candidates = buildCandidates(input.mat(), fast);
        // 额外快速缩放（清晰小码常见）
        QSet<QString> seen;
        for (const CandidateView &c : candidates)
            seen.insert(c.strategy);
        pushCandidate(candidates, seen,
                      makeCandidate(toGray8u(input.mat()), qMax(0.5, effective.scale) * 1.5, -1,
                                    QStringLiteral("gray@x1.5")),
                      qMax(1, effective.maxAttempts));
        if (effective.tryRotate) {
            pushCandidate(candidates, seen,
                          makeCandidate(toGray8u(input.mat()), qMax(0.5, effective.scale),
                                        cv::ROTATE_90_CLOCKWISE, QStringLiteral("original@x1.0_r90")),
                          qMax(1, effective.maxAttempts));
        }
    }

    auto runCandidates = [&](const QVector<CandidateView> &views,
                             QVector<RecognitionHit> *accepted,
                             bool *anyDetected,
                             bool *anyDecoded,
                             VisionImage *lastDebug,
                             bool *timedOut) {
        const int maxCount = qMax(1, options.maxCount);
        for (const CandidateView &cand : views) {
            if (accepted->size() >= maxCount)
                break;
            if (maxTimeMs > 0 && budget.elapsed() >= maxTimeMs) {
                *timedOut = true;
                break;
            }
            out.triedStrategies.append(cand.strategy);
            ++out.attemptCount;

            QVector<RecognitionHit> local;
#if defined(SELT_HAS_OPENCV_QR) && SELT_HAS_OPENCV_QR
            if (wantQr) {
                bool detected = false;
                const DecodeOutcome qrOut =
                    decodeQr(cand.mat, maxCount - accepted->size(), local, &detected);
                *anyDetected = *anyDetected || detected || qrOut != DecodeOutcome::None;
                if (qrOut == DecodeOutcome::Decoded)
                    *anyDecoded = true;
            }
#endif
#if defined(SELT_HAS_OPENCV_BARCODE) && SELT_HAS_OPENCV_BARCODE
            if (wantBarcode && accepted->size() + local.size() < maxCount) {
                bool detected = false;
                QVector<RecognitionHit> barcodeHits;
                const DecodeOutcome bcOut = decodeBarcode(
                    cand.mat, maxCount - accepted->size() - local.size(), barcodeHits, &detected);
                *anyDetected = *anyDetected || detected || bcOut != DecodeOutcome::None;
                if (bcOut == DecodeOutcome::Decoded)
                    *anyDecoded = true;
                local.append(barcodeHits);
            }
#endif
            for (RecognitionHit &hit : local) {
                hit.strategy = cand.strategy;
                mapHitToOriginal(hit, cand);
                if (!cornersLookValid(hit.corners, input.width(), input.height()))
                    continue;
                if (hit.text.trimmed().isEmpty())
                    continue;
                hit.score = scoreHit(hit);
            }
            appendUniqueHits(*accepted, local, maxCount);
            if (options.keepDebugImage && !cand.mat.empty())
                *lastDebug = VisionImage(cand.mat.clone(), input.sourcePath());
            if (!accepted->isEmpty() && out.strategyUsed.isEmpty())
                out.strategyUsed = cand.strategy;
        }
    };

    QVector<RecognitionHit> accepted;
    bool anyDetected = false;
    bool anyDecoded = false;
    bool timedOut = false;
    VisionImage lastDebug;
    runCandidates(candidates, &accepted, &anyDetected, &anyDecoded, &lastDebug, &timedOut);

    // Auto enhance: only if fast path failed.
    if (accepted.isEmpty() && enhance == QLatin1String("auto") && !timedOut) {
        Options slow = effective;
        slow.enhanceMode = QStringLiteral("full");
        const QVector<CandidateView> enhanced = buildCandidates(input.mat(), slow);
        QVector<CandidateView> extra;
        QSet<QString> seen;
        for (const QString &s : out.triedStrategies)
            seen.insert(s);
        for (const CandidateView &c : enhanced) {
            if (seen.contains(c.strategy))
                continue;
            extra.append(c);
        }
        runCandidates(extra, &accepted, &anyDetected, &anyDecoded, &lastDebug, &timedOut);
    }

    out.candidateDetected = anyDetected || anyDecoded || !accepted.isEmpty();
    out.hits = accepted;
    finalizeJoined(out);
    out.ok = true;
    out.elapsedMs = budget.elapsed();
    if (options.keepDebugImage && !lastDebug.isEmpty())
        out.debugImage = lastDebug;

    if (!out.found) {
        if (anyDetected && !anyDecoded) {
            out.failureStage = QStringLiteral("decode");
            out.diagnosticCode = DiagnosticCodes::decodeFailed();
            out.errorMessage = timedOut
                ? QStringLiteral("检测到码区域但解码失败（已超时）")
                : QStringLiteral("检测到码区域但解码失败，可尝试增强/放大/反色");
        } else if (timedOut) {
            out.failureStage = QStringLiteral("detect");
            out.diagnosticCode = DiagnosticCodes::noCandidate();
            out.errorMessage = QStringLiteral("识别超时（已尝试 %1 次，耗时 %2 ms）")
                                   .arg(out.attemptCount)
                                   .arg(out.elapsedMs);
        } else {
            out.failureStage = QStringLiteral("detect");
            out.diagnosticCode = DiagnosticCodes::noCandidate();
            out.errorMessage = QStringLiteral("未检测到条码/二维码候选");
        }
        if (error)
            *error = out.errorMessage;
        // 不把 overlay 烤进输出图：预览会再画一遍 Overlay，烘焙会导致重影。
        out.overlayImage = options.passthroughOnFail ? previewImageWithoutBurnIn(input)
                                                    : VisionImage{};
        return true;
    }

    if (out.confidence < options.minConfidence) {
        out.failureStage = QStringLiteral("quality");
        out.diagnosticCode = DiagnosticCodes::qualityLow();
        out.errorMessage = QStringLiteral("识别置信度低于阈值");
        out.found = false;
        out.hits.clear();
        out.overlays.clear();
        out.joinedText.clear();
        out.corners.clear();
        out.boundingRect = QRectF();
        if (error)
            *error = out.errorMessage;
        out.overlayImage = options.passthroughOnFail ? previewImageWithoutBurnIn(input)
                                                    : VisionImage{};
        return true;
    }

    out.failureStage = QStringLiteral("none");
    out.diagnosticCode.clear();
    out.errorMessage.clear();
    if (out.strategyUsed.isEmpty() && !out.hits.isEmpty())
        out.strategyUsed = out.hits.first().strategy;
    out.overlayImage = previewImageWithoutBurnIn(input);
    return true;
}

bool OcrAlgorithm::apply(const VisionImage &input,
                         const Options &options,
                         RecognitionResult &out,
                         QString *error)
{
    out = RecognitionResult{};
    out.backendStatus = RecognitionCapability::ocrStatusText();
    out.failureStage = QStringLiteral("input");
    if (input.isEmpty()) {
        out.diagnosticCode = DiagnosticCodes::imageInvalid();
        out.errorMessage = QStringLiteral("输入图像为空");
        if (error)
            *error = out.errorMessage;
        return false;
    }

#if !(defined(SELT_HAS_TESSERACT) && SELT_HAS_TESSERACT)
    Q_UNUSED(options);
    out.failureStage = QStringLiteral("backend");
    out.diagnosticCode = DiagnosticCodes::backendMissing();
    out.errorMessage = RecognitionCapability::ocrStatusText();
    if (error)
        *error = out.errorMessage;
    return false;
#else
    QString tessdata = options.tessdataPath.trimmed();
    if (tessdata.isEmpty()) {
#ifdef SELT_DEFAULT_TESSDATA_PATH
        tessdata = QStringLiteral(SELT_DEFAULT_TESSDATA_PATH);
#endif
    }
    if (tessdata.isEmpty()) {
        const QByteArray env = qgetenv("TESSDATA_PREFIX");
        if (!env.isEmpty())
            tessdata = QString::fromLocal8Bit(env);
    }
    if (!tessdata.isEmpty()) {
        const QFileInfo info(tessdata);
        if (info.isFile())
            tessdata = info.absolutePath();
    }
    if (!tessdata.isEmpty()) {
        const QString trained = QDir(tessdata).filePath(options.language + QStringLiteral(".traineddata"));
        if (!QFileInfo::exists(trained)) {
            out.failureStage = QStringLiteral("backend");
            out.diagnosticCode = DiagnosticCodes::languagePackMissing();
            out.errorMessage = QStringLiteral("缺少语言包: %1").arg(trained);
            if (error)
                *error = out.errorMessage;
            return false;
        }
    }

    tesseract::TessBaseAPI api;
    const int initRc = api.Init(tessdata.isEmpty() ? nullptr : tessdata.toLocal8Bit().constData(),
                                options.language.toUtf8().constData());
    if (initRc != 0) {
        out.failureStage = QStringLiteral("backend");
        out.diagnosticCode = DiagnosticCodes::capabilityLimited();
        out.errorMessage = QStringLiteral("Tesseract 初始化失败（检查 tessdataPath / 语言包）");
        if (error)
            *error = out.errorMessage;
        return false;
    }
    if (!options.whitelist.isEmpty())
        api.SetVariable("tessedit_char_whitelist", options.whitelist.toUtf8().constData());
    api.SetPageSegMode(options.multiLine ? tesseract::PSM_AUTO : tesseract::PSM_SINGLE_LINE);

    cv::Mat gray = toGray8u(input.mat());
    cv::Mat processed = gray;
    if (options.binarize)
        cv::threshold(gray, processed, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
    if (options.scale > 1.01) {
        cv::Mat scaled;
        cv::resize(processed, scaled, cv::Size(), options.scale, options.scale, cv::INTER_CUBIC);
        processed = scaled;
    }

    api.SetImage(processed.data, processed.cols, processed.rows, processed.channels(),
                 int(processed.step));
    char *textPtr = api.GetUTF8Text();
    const QString rawText = textPtr ? QString::fromUtf8(textPtr).trimmed() : QString();
    delete[] textPtr;
    const int conf = api.MeanTextConf();
    const double confidence = qBound(0.0, double(conf) / 100.0, 1.0);
    out.attemptCount = 1;
    out.strategyUsed = options.binarize ? QStringLiteral("ocr_binarize") : QStringLiteral("ocr_gray");

    if (rawText.isEmpty() || confidence < options.minConfidence) {
        out.ok = true;
        out.found = false;
        out.confidence = confidence;
        out.failureStage = rawText.isEmpty() ? QStringLiteral("detect") : QStringLiteral("quality");
        out.diagnosticCode = rawText.isEmpty() ? DiagnosticCodes::noCandidate()
                                               : DiagnosticCodes::qualityLow();
        out.errorMessage = rawText.isEmpty()
            ? QStringLiteral("未识别到文本")
            : QStringLiteral("识别置信度低于阈值");
        out.overlayImage = previewImageWithoutBurnIn(input);
        if (error)
            *error = out.errorMessage;
        return true;
    }

    RecognitionHit hit;
    hit.text = rawText;
    hit.symbology = QStringLiteral("ocr");
    hit.confidence = confidence;
    hit.score = confidence;
    hit.strategy = out.strategyUsed;
    hit.boundingRect = QRectF(0, 0, input.width(), input.height());
    out.hits.append(hit);
    finalizeJoined(out);
    out.ok = true;
    out.failureStage = QStringLiteral("none");
    out.overlayImage = previewImageWithoutBurnIn(input);
    return true;
#endif
}

} // namespace Selt
