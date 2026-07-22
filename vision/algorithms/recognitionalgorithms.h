#ifndef RECOGNITIONALGORITHMS_H
#define RECOGNITIONALGORITHMS_H

#include "vision/model/overlayitems.h"
#include "vision/model/visionimage.h"

#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVector>

namespace Selt {

struct RecognitionHit
{
    QString text;
    QString symbology;
    double confidence{0.0};
    QVector<QPointF> corners;
    QRectF boundingRect;
};

struct RecognitionResult
{
    bool ok{false};
    bool found{false};
    QString diagnosticCode;
    QString errorMessage;
    QString joinedText;
    double confidence{0.0};
    QStringList symbologies;
    QVector<RecognitionHit> hits;
    OverlayList overlays;
    VisionImage overlayImage;
};

class RecognitionCapability
{
public:
    static bool hasQrBackend();
    static bool hasBarcodeBackend();
    static bool hasOcrBackend();
    static QString qrStatusText();
    static QString barcodeStatusText();
    static QString ocrStatusText();
};

class BarcodeQrAlgorithm
{
public:
    struct Options {
        QString symbology{QStringLiteral("auto")}; // auto | qr | barcode
        int maxCount{8};
        bool tryRotate{false};
        bool passthroughOnFail{true};
    };

    static bool apply(const VisionImage &input,
                      const Options &options,
                      RecognitionResult &out,
                      QString *error = nullptr);
};

class OcrAlgorithm
{
public:
    struct Options {
        QString language{QStringLiteral("eng")};
        QString tessdataPath;
        QString whitelist;
        bool multiLine{true};
        double scale{1.0};
        double minConfidence{0.0};
        bool binarize{false};
    };

    static bool apply(const VisionImage &input,
                      const Options &options,
                      RecognitionResult &out,
                      QString *error = nullptr);
};

} // namespace Selt

#endif // RECOGNITIONALGORITHMS_H
