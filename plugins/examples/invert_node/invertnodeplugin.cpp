#include "invertnodeplugin.h"

#include "vision/model/modulevisualstyle.h"
#include "vision/runtime/nodeexecutorregistry.h"

#include <QElapsedTimer>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

namespace {

const QString kTypeId = QStringLiteral("plugin.invertImage");

class InvertExecutor : public Selt::INodeExecutor
{
public:
    QString typeId() const override { return kTypeId; }
    Selt::ExecutionResult execute(const Selt::ExecutionRequest &request, Selt::ExecutionContext &) override
    {
        Selt::ExecutionResult r;
        QElapsedTimer t;
        t.start();
        VisionImage input = request.inputs.value(QStringLiteral("image")).toImage();
        if (input.isEmpty()) {
            r.status = ModuleStatus::Failed;
            r.errorMessage = QStringLiteral("输入图像为空");
            r.elapsedMs = t.elapsed();
            return r;
        }
        cv::Mat dst;
        cv::bitwise_not(input.mat(), dst);
        r.outputs.insert(QStringLiteral("image"), Selt::DataValue(VisionImage(dst)));
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

} // namespace

Selt::PluginMetadata InvertNodePlugin::metadata() const
{
    Selt::PluginMetadata meta;
    meta.pluginId = QStringLiteral("selt.example.invert");
    meta.displayName = QStringLiteral("Invert Image Example");
    meta.pluginVersion = QStringLiteral("1.0.0");
    meta.sdkVersion = Selt::VersionInfo::pluginSdkVersion;
    meta.abiTag = QStringLiteral("qt6-opencv");
    meta.compiler = QStringLiteral("mingw");
    meta.nodeTypeIds = {kTypeId};
    meta.capabilities = {QStringLiteral("algorithm-node")};
    return meta;
}

QVector<ModuleDescriptor> InvertNodePlugin::descriptors() const
{
    ModuleDescriptor d;
    d.typeId = kTypeId;
    d.displayName = QStringLiteral("反色(示例插件)");
    d.category = QStringLiteral("插件");
    d.description = QStringLiteral("示例插件：对输入图像做反色");
    d.helpText = d.description;
    d.iconKey = QStringLiteral("插件");
    d.fixedSize = QSizeF(160, 72);
    d.fillColor = Selt::fillColorForCategory(d.category);
    d.accentColor = Selt::accentColorForCategory(d.category);
    d.borderColor = d.accentColor;
    d.ports = {
        {QStringLiteral("in"), QStringLiteral("输入"), true, Selt::DataTypeId::Image, true,
         QStringLiteral("输入图像")},
        {QStringLiteral("out"), QStringLiteral("输出"), false, Selt::DataTypeId::Image, false,
         QStringLiteral("反色图像")}};
    d.inputKeys = {QStringLiteral("image")};
    d.outputKeys = {QStringLiteral("image")};
    return {d};
}

void InvertNodePlugin::registerExecutors(Selt::NodeExecutorRegistry &registry)
{
    registry.registerFactory(kTypeId, [] { return std::make_shared<InvertExecutor>(); });
}
