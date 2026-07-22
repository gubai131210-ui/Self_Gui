#include "plugin_host/pluginhost.h"
#include "sdk/include/selt/iplugin.h"
#include "vision/model/moduledescriptor.h"
#include "vision/registry/visionnodeids.h"
#include "vision/registry/visionnoderegistry.h"
#include "vision/runtime/nodeexecutorregistry.h"

#include <QtTest>

namespace {

const QString kMockType = QStringLiteral("plugin.mockEcho");

class MockEchoExecutor : public Selt::INodeExecutor
{
public:
    QString typeId() const override { return kMockType; }
    Selt::ExecutionResult execute(const Selt::ExecutionRequest &request, Selt::ExecutionContext &) override
    {
        Selt::ExecutionResult r;
        r.status = ModuleStatus::Success;
        r.outputs = request.inputs;
        return r;
    }
};

class MockEchoPlugin : public QObject, public Selt::INodePlugin
{
    Q_OBJECT
    Q_INTERFACES(Selt::INodePlugin)
public:
    Selt::PluginMetadata metadata() const override
    {
        Selt::PluginMetadata m;
        m.pluginId = QStringLiteral("test.mockEcho");
        m.displayName = QStringLiteral("Mock Echo");
        m.sdkVersion = Selt::VersionInfo::pluginSdkVersion;
        m.nodeTypeIds = {kMockType};
        return m;
    }
    QVector<ModuleDescriptor> descriptors() const override
    {
        ModuleDescriptor d;
        d.typeId = kMockType;
        d.displayName = QStringLiteral("Mock Echo");
        d.category = QStringLiteral("插件");
        d.description = QStringLiteral("test");
        d.ports = {
            {QStringLiteral("in"), QStringLiteral("输入"), true, Selt::DataTypeId::Image, true, {}},
            {QStringLiteral("out"), QStringLiteral("输出"), false, Selt::DataTypeId::Image, false, {}}};
        return {d};
    }
    void registerExecutors(Selt::NodeExecutorRegistry &registry) override
    {
        registry.registerFactory(kMockType, [] { return std::make_shared<MockEchoExecutor>(); });
    }
};

} // namespace

class PluginHostTest : public QObject
{
    Q_OBJECT
private slots:
    void registerExternalDescriptorSucceeds();
    void rejectDuplicateDescriptor();
    void mockPluginRegistersExecutor();
    void missingPluginDirectoryIsSoftFail();
};

void PluginHostTest::registerExternalDescriptorSucceeds()
{
    ModuleDescriptor d;
    d.typeId = QStringLiteral("plugin.testUniqueA");
    d.displayName = QStringLiteral("Unique A");
    d.category = QStringLiteral("插件");
    QVERIFY(VisionNodeRegistry::registerExternalDescriptor(d));
    QVERIFY(VisionNodeRegistry::supportedTypes().contains(d.typeId));
    QCOMPARE(VisionNodeRegistry::displayName(d.typeId), QStringLiteral("Unique A"));
}

void PluginHostTest::rejectDuplicateDescriptor()
{
    ModuleDescriptor d;
    d.typeId = VisionNodeIds::imageLoader();
    d.displayName = QStringLiteral("Conflict");
    QString error;
    QVERIFY(!VisionNodeRegistry::registerExternalDescriptor(d, &error));
    QVERIFY(error.contains(QStringLiteral("已存在")));
}

void PluginHostTest::mockPluginRegistersExecutor()
{
    MockEchoPlugin plugin;
    QString error;
    QVERIFY(VisionNodeRegistry::registerExternalDescriptors(plugin.descriptors(), &error));
    plugin.registerExecutors(Selt::NodeExecutorRegistry::instance());
    VisionNodeRegistry::setPluginIdForType(kMockType, plugin.metadata().pluginId);
    QVERIFY(Selt::NodeExecutorRegistry::instance().contains(kMockType));
    auto exec = Selt::NodeExecutorRegistry::instance().create(kMockType);
    QVERIFY(exec);
    QCOMPARE(VisionNodeRegistry::pluginIdForType(kMockType), QStringLiteral("test.mockEcho"));
}

void PluginHostTest::missingPluginDirectoryIsSoftFail()
{
    const QVector<Selt::PluginLoadInfo> loaded =
        Selt::PluginHost::instance().loadFromDirectory(QStringLiteral("Z:/path/that/does/not/exist"));
    QCOMPARE(loaded.size(), 0);
    QVERIFY(!Selt::PluginHost::instance().diagnostics().isEmpty());
}

QTEST_MAIN(PluginHostTest)
#include "test_plugin_host.moc"
