#include "vision/algorithms/recognitionalgorithms.h"

#include "vision/runtime/diagnosticcodes.h"

#include <QDir>
#include <QFileInfo>
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

OverlayItem makeQuadOverlay(const QVector<QPointF> &corners)
{
    OverlayItem item;
    item.type = OverlayItemType::Contour;
    item.points = corners;
    item.color = QColor(255, 140, 0);
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

VisionImage drawOverlayImage(const VisionImage &input, const OverlayList &overlays)
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
            cv::polylines(display, &ptr, &n, 1, true, cv::Scalar(0, 140, 255), 2);
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

void finalizeJoined(RecognitionResult &out)
{
    QStringList texts;
    QStringList types;
    double confSum = 0.0;
    for (const RecognitionHit &hit : out.hits) {
        if (!hit.text.isEmpty())
            texts.append(hit.text);
        if (!hit.symbology.isEmpty() && !types.contains(hit.symbology))
            types.append(hit.symbology);
        confSum += hit.confidence;
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
    out.found = !out.hits.isEmpty();
}

#if defined(SELT_HAS_OPENCV_QR) && SELT_HAS_OPENCV_QR
bool decodeQr(const cv::Mat &mat, int maxCount, QVector<RecognitionHit> &hits)
{
    try {
        cv::QRCodeDetector detector;
        std::vector<std::string> decoded;
        cv::Mat points;
        if (!detector.detectAndDecodeMulti(mat, decoded, points))
            return false;
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
        return !hits.isEmpty();
    } catch (const cv::Exception &) {
        return false;
    }
}
#endif

#if defined(SELT_HAS_OPENCV_BARCODE) && SELT_HAS_OPENCV_BARCODE
bool decodeBarcode(const cv::Mat &mat, int maxCount, QVector<RecognitionHit> &hits)
{
    try {
        cv::barcode::BarcodeDetector detector;
        std::vector<std::string> decoded;
        std::vector<std::string> types;
        cv::Mat points;
        if (!detector.detectAndDecodeWithType(mat, decoded, types, points))
            return false;
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
        return !hits.isEmpty();
    } catch (const cv::Exception &) {
        return false;
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
        : QStringLiteral("缺少 OpenCV barcode 模块");
}

QString RecognitionCapability::ocrStatusText()
{
    return hasOcrBackend()
        ? QStringLiteral("Tesseract 可用")
        : QStringLiteral("未找到 Tesseract；OCR 节点将报告能力受限");
}

bool BarcodeQrAlgorithm::apply(const VisionImage &input,
                               const Options &options,
                               RecognitionResult &out,
                               QString *error)
{
    out = RecognitionResult{};
    if (input.isEmpty()) {
        out.diagnosticCode = DiagnosticCodes::imageEmpty();
        out.errorMessage = QStringLiteral("输入图像为空");
        if (error)
            *error = out.errorMessage;
        return false;
    }

    const QString mode = options.symbology.trimmed().toLower();
    const bool wantQr = mode == QLatin1String("auto") || mode == QLatin1String("qr");
    const bool wantBarcode = mode == QLatin1String("auto") || mode == QLatin1String("barcode");

    if (wantQr && !RecognitionCapability::hasQrBackend()
        && !(wantBarcode && RecognitionCapability::hasBarcodeBackend())) {
        out.diagnosticCode = DiagnosticCodes::capabilityLimited();
        out.errorMessage = RecognitionCapability::qrStatusText();
        if (error)
            *error = out.errorMessage;
        return false;
    }
    if (mode == QLatin1String("barcode") && !RecognitionCapability::hasBarcodeBackend()) {
        out.diagnosticCode = DiagnosticCodes::capabilityLimited();
        out.errorMessage = RecognitionCapability::barcodeStatusText();
        if (error)
            *error = out.errorMessage;
        return false;
    }
    if (mode == QLatin1String("qr") && !RecognitionCapability::hasQrBackend()) {
        out.diagnosticCode = DiagnosticCodes::capabilityLimited();
        out.errorMessage = RecognitionCapability::qrStatusText();
        if (error)
            *error = out.errorMessage;
        return false;
    }

    QVector<RecognitionHit> hits;
#if defined(SELT_HAS_OPENCV_QR) && SELT_HAS_OPENCV_QR
    if (wantQr)
        decodeQr(input.mat(), qMax(1, options.maxCount), hits);
#endif
#if defined(SELT_HAS_OPENCV_BARCODE) && SELT_HAS_OPENCV_BARCODE
    if (wantBarcode && hits.size() < qMax(1, options.maxCount))
        decodeBarcode(input.mat(), qMax(1, options.maxCount) - hits.size(), hits);
#else
    if (mode == QLatin1String("barcode")) {
        out.diagnosticCode = DiagnosticCodes::capabilityLimited();
        out.errorMessage = RecognitionCapability::barcodeStatusText();
        if (error)
            *error = out.errorMessage;
        return false;
    }
#endif

    if (options.tryRotate && hits.isEmpty()) {
        cv::Mat rotated;
        cv::rotate(input.mat(), rotated, cv::ROTATE_90_CLOCKWISE);
        VisionImage rotatedImage(rotated, input.sourcePath());
#if defined(SELT_HAS_OPENCV_QR) && SELT_HAS_OPENCV_QR
        if (wantQr)
            decodeQr(rotatedImage.mat(), qMax(1, options.maxCount), hits);
#endif
#if defined(SELT_HAS_OPENCV_BARCODE) && SELT_HAS_OPENCV_BARCODE
        if (wantBarcode && hits.isEmpty())
            decodeBarcode(rotatedImage.mat(), qMax(1, options.maxCount), hits);
#endif
    }

    out.hits = hits;
    finalizeJoined(out);
    out.ok = true;
    out.overlayImage = options.passthroughOnFail || out.found
        ? drawOverlayImage(input, out.overlays)
        : VisionImage{};
    if (!out.found) {
        out.diagnosticCode = DiagnosticCodes::noTarget();
        out.errorMessage = QStringLiteral("未检测到条码/二维码");
        if (error)
            *error = out.errorMessage;
        // Soft success with found=false so runtime can continue and inspect outputs.
        return true;
    }
    return true;
}

bool OcrAlgorithm::apply(const VisionImage &input,
                         const Options &options,
                         RecognitionResult &out,
                         QString *error)
{
    out = RecognitionResult{};
    if (input.isEmpty()) {
        out.diagnosticCode = DiagnosticCodes::imageEmpty();
        out.errorMessage = QStringLiteral("输入图像为空");
        if (error)
            *error = out.errorMessage;
        return false;
    }

#if !(defined(SELT_HAS_TESSERACT) && SELT_HAS_TESSERACT)
    Q_UNUSED(options);
    out.diagnosticCode = DiagnosticCodes::capabilityLimited();
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
        out.diagnosticCode = DiagnosticCodes::capabilityLimited();
        out.errorMessage = QStringLiteral("Tesseract 初始化失败（检查 tessdataPath / 语言包）");
        if (error)
            *error = out.errorMessage;
        return false;
    }
    if (!options.whitelist.isEmpty())
        api.SetVariable("tessedit_char_whitelist", options.whitelist.toUtf8().constData());
    api.SetPageSegMode(options.multiLine ? tesseract::PSM_AUTO : tesseract::PSM_SINGLE_LINE);

    cv::Mat gray;
    if (input.mat().channels() == 1)
        gray = input.mat();
    else if (input.mat().channels() == 4)
        cv::cvtColor(input.mat(), gray, cv::COLOR_BGRA2GRAY);
    else
        cv::cvtColor(input.mat(), gray, cv::COLOR_BGR2GRAY);

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
    const double confidence = qBound(0.0, 1.0, double(conf) / 100.0);

    if (rawText.isEmpty() || confidence < options.minConfidence) {
        out.ok = true;
        out.found = false;
        out.confidence = confidence;
        out.diagnosticCode = rawText.isEmpty() ? DiagnosticCodes::noTarget()
                                               : DiagnosticCodes::lowConfidence();
        out.errorMessage = rawText.isEmpty()
            ? QStringLiteral("未识别到文本")
            : QStringLiteral("识别置信度低于阈值");
        out.overlayImage = drawOverlayImage(input, {});
        if (error)
            *error = out.errorMessage;
        return true;
    }

    RecognitionHit hit;
    hit.text = rawText;
    hit.symbology = QStringLiteral("ocr");
    hit.confidence = confidence;
    hit.boundingRect = QRectF(0, 0, input.width(), input.height());
    out.hits.append(hit);
    finalizeJoined(out);
    out.ok = true;
    out.overlayImage = drawOverlayImage(input, out.overlays);
    return true;
#endif
}

} // namespace Selt
