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
    double score{0.0};
    QVector<QPointF> corners;
    QRectF boundingRect;
    QString strategy;
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

    /// 主命中几何（取 hits 中分数最高者），便于端口/预览统一消费
    QVector<QPointF> corners;
    QRectF boundingRect;

    /// 工业诊断扩展（向后兼容：旧调用方可忽略）
    QString strategyUsed;
    QString failureStage; ///< none|backend|detect|decode|quality|input
    QString backendStatus;
    int attemptCount{0};
    double bestScore{0.0};
    bool candidateDetected{false}; ///< 有候选框但未必解码成功
    QStringList triedStrategies;
    VisionImage debugImage;
    qint64 elapsedMs{0};
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
    static QString combinedStatusText();
};

class BarcodeQrAlgorithm
{
public:
    struct Options {
        QString symbology{QStringLiteral("auto")}; // auto | qr | barcode
        int maxCount{8};
        bool tryRotate{false};
        bool passthroughOnFail{true};

        /// none = 仅原图；auto = 快速路径失败后增强；full = 始终多策略
        QString enhanceMode{QStringLiteral("auto")};
        double scale{1.0}; ///< 基础缩放，1.0 表示原尺寸；auto 增强时会额外尝试 1.5/2.0
        bool tryInvert{true};
        bool tryClahe{true};
        bool tryAdaptive{true};
        bool trySharpen{true};
        bool tryDenoise{false};
        int maxAttempts{12};
        /// 单次识别总预算（毫秒）；0 表示不限制。超时后停止后续候选并保留已有诊断。
        int maxTimeMs{800};
        double minConfidence{0.0};
        bool keepDebugImage{true};
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
