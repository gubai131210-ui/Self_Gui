#include "vision/algorithms/preprocessalgorithms.h"
#include "vision/nodes/imageexecutorhelpers.h"
#include "vision/registry/visionnodeids.h"
#include "vision/runtime/nodeexecutorregistry.h"

#include <QElapsedTimer>
#include <memory>
#include <opencv2/imgproc.hpp>

namespace Selt {
namespace {

class GaussianBlurExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::gaussianBlur(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        QElapsedTimer t;
        t.start();
        VisionImage input = applyRoiFromRequest(request.inputs.value(QStringLiteral("image")).toImage(),
                                                request.parameters);
        int kx = resolveIntParam(request, QStringLiteral("ksizeX"), 5);
        int ky = resolveIntParam(request, QStringLiteral("ksizeY"), 5);
        QString err;
        if (!ensureOddKernel(kx, &err) || !ensureOddKernel(ky, &err))
            return failResult(err, DiagnosticCodes::invalidParameter(), t.elapsed());
        VisionImage out;
        if (!GaussianBlurAlgorithm::apply(input, out, kx, ky,
                                          resolveRealInput(request, QStringLiteral("sigmaX"), 0.0),
                                          resolveRealInput(request, QStringLiteral("sigmaY"), 0.0),
                                          &err))
            return failResult(err, DiagnosticCodes::opencvException(), t.elapsed());
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("image"), DataValue(out));
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class MedianBlurExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::medianBlur(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        QElapsedTimer t;
        t.start();
        VisionImage input = applyRoiFromRequest(request.inputs.value(QStringLiteral("image")).toImage(),
                                                request.parameters);
        int k = resolveIntParam(request, QStringLiteral("ksize"), 5);
        QString err;
        if (!ensureOddKernel(k, &err))
            return failResult(err, DiagnosticCodes::invalidParameter(), t.elapsed());
        VisionImage out;
        if (!MedianBlurAlgorithm::apply(input, out, k, &err))
            return failResult(err, DiagnosticCodes::opencvException(), t.elapsed());
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("image"), DataValue(out));
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class BilateralFilterExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::bilateralFilter(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        QElapsedTimer t;
        t.start();
        VisionImage input = applyRoiFromRequest(request.inputs.value(QStringLiteral("image")).toImage(),
                                                request.parameters);
        VisionImage out;
        QString err;
        if (!BilateralFilterAlgorithm::apply(input, out,
                                             resolveIntParam(request, QStringLiteral("diameter"), 9),
                                             resolveRealInput(request, QStringLiteral("sigmaColor"), 75.0),
                                             resolveRealInput(request, QStringLiteral("sigmaSpace"), 75.0),
                                             &err))
            return failResult(err, DiagnosticCodes::opencvException(), t.elapsed());
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("image"), DataValue(out));
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class OtsuThresholdExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::otsuThreshold(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        QElapsedTimer t;
        t.start();
        VisionImage input = applyRoiFromRequest(request.inputs.value(QStringLiteral("image")).toImage(),
                                                request.parameters);
        VisionImage out;
        QString err;
        if (!OtsuThresholdAlgorithm::apply(input, out,
                                           resolveRealInput(request, QStringLiteral("maxValue"), 255.0),
                                           resolveBoolParam(request, QStringLiteral("invert"), false),
                                           &err))
            return failResult(err, DiagnosticCodes::opencvException(), t.elapsed());
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("image"), DataValue(out));
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class AdaptiveThresholdExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::adaptiveThreshold(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        QElapsedTimer t;
        t.start();
        VisionImage input = applyRoiFromRequest(request.inputs.value(QStringLiteral("image")).toImage(),
                                                request.parameters);
        int block = resolveIntParam(request, QStringLiteral("blockSize"), 11);
        QString err;
        if (!ensureOddKernel(block, &err))
            return failResult(err, DiagnosticCodes::invalidParameter(), t.elapsed());
        const QString method = resolveStringParam(request, QStringLiteral("method"),
                                                  QStringLiteral("gaussian"));
        const auto m = method.contains(QStringLiteral("mean"), Qt::CaseInsensitive)
            ? AdaptiveThresholdAlgorithm::Method::AdaptiveMean
            : AdaptiveThresholdAlgorithm::Method::AdaptiveGaussian;
        VisionImage out;
        if (!AdaptiveThresholdAlgorithm::apply(input, out,
                                               resolveRealInput(request, QStringLiteral("maxValue"), 255.0),
                                               m, block,
                                               resolveRealInput(request, QStringLiteral("C"), 2.0),
                                               resolveBoolParam(request, QStringLiteral("invert"), false),
                                               &err))
            return failResult(err, DiagnosticCodes::opencvException(), t.elapsed());
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("image"), DataValue(out));
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class SobelExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::sobel(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        QElapsedTimer t;
        t.start();
        VisionImage input = applyRoiFromRequest(request.inputs.value(QStringLiteral("image")).toImage(),
                                                request.parameters);
        int k = resolveIntParam(request, QStringLiteral("ksize"), 3);
        QString err;
        if (!ensureOddKernel(k, &err))
            return failResult(err, DiagnosticCodes::invalidParameter(), t.elapsed());
        VisionImage out;
        if (!SobelAlgorithm::apply(input, out,
                                   resolveIntParam(request, QStringLiteral("dx"), 1),
                                   resolveIntParam(request, QStringLiteral("dy"), 0),
                                   k,
                                   resolveRealInput(request, QStringLiteral("scale"), 1.0),
                                   &err))
            return failResult(err, DiagnosticCodes::opencvException(), t.elapsed());
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("image"), DataValue(out));
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class ColorConvertExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::colorConvert(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        QElapsedTimer t;
        t.start();
        VisionImage input = applyRoiFromRequest(request.inputs.value(QStringLiteral("image")).toImage(),
                                                request.parameters);
        VisionImage out;
        QString err;
        if (!ColorConvertAlgorithm::apply(input, out,
                                          resolveStringParam(request, QStringLiteral("mode"),
                                                             QStringLiteral("gray")),
                                          &err))
            return failResult(err, DiagnosticCodes::imageTypeMismatch(), t.elapsed());
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("image"), DataValue(out));
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class GeometricTransformExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::geometricTransform(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        QElapsedTimer t;
        t.start();
        VisionImage input = request.inputs.value(QStringLiteral("image")).toImage();
        VisionImage out;
        QString err;
        if (!GeometricTransformAlgorithm::apply(input, out,
                                                resolveStringParam(request, QStringLiteral("op"),
                                                                   QStringLiteral("rotate90")),
                                                &err))
            return failResult(err, DiagnosticCodes::invalidParameter(), t.elapsed());
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("image"), DataValue(out));
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class MorphGradientExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::morphGradient(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        QElapsedTimer t;
        t.start();
        QString error;
        QString code;
        VisionImage input = requireImageWithRoi(request, &error, &code);
        if (input.isEmpty())
            return failResult(error, code.isEmpty() ? DiagnosticCodes::imageEmpty() : code, t.elapsed());
        int ksize = resolveIntParam(request, QStringLiteral("ksize"), 3);
        if (ksize % 2 == 0)
            ++ksize;
        cv::Mat src = input.mat().clone();
        if (src.channels() > 1)
            cv::cvtColor(src, src, cv::COLOR_BGR2GRAY);
        cv::Mat out;
        const cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(ksize, ksize));
        cv::morphologyEx(src, out, cv::MORPH_GRADIENT, kernel);
        VisionImage result(out, input.sourcePath());
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("out"), DataValue(result));
        r.outputs.insert(QStringLiteral("image"), DataValue(result));
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

} // namespace

void registerPreprocessExecutors(NodeExecutorRegistry &reg)
{
    reg.registerFactory(VisionNodeIds::gaussianBlur(), [] { return std::make_shared<GaussianBlurExecutor>(); });
    reg.registerFactory(VisionNodeIds::medianBlur(), [] { return std::make_shared<MedianBlurExecutor>(); });
    reg.registerFactory(VisionNodeIds::bilateralFilter(), [] { return std::make_shared<BilateralFilterExecutor>(); });
    reg.registerFactory(VisionNodeIds::otsuThreshold(), [] { return std::make_shared<OtsuThresholdExecutor>(); });
    reg.registerFactory(VisionNodeIds::adaptiveThreshold(), [] { return std::make_shared<AdaptiveThresholdExecutor>(); });
    reg.registerFactory(VisionNodeIds::sobel(), [] { return std::make_shared<SobelExecutor>(); });
    reg.registerFactory(VisionNodeIds::colorConvert(), [] { return std::make_shared<ColorConvertExecutor>(); });
    reg.registerFactory(VisionNodeIds::geometricTransform(), [] { return std::make_shared<GeometricTransformExecutor>(); });
    reg.registerFactory(VisionNodeIds::morphGradient(), [] { return std::make_shared<MorphGradientExecutor>(); });
}

} // namespace Selt
