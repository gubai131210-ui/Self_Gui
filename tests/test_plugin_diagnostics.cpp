#include "plugin_host/pluginhost.h"

#include <QtTest>

class TestPluginDiagnostics : public QObject
{
    Q_OBJECT

private slots:
    void reportsMissingPluginDependency();
    void deploymentRequirementsAlwaysAvailable();
};

void TestPluginDiagnostics::reportsMissingPluginDependency()
{
    QJsonArray dependencies;
    dependencies.append(QJsonObject{
        {QStringLiteral("pluginId"), QStringLiteral("missing.plugin")},
        {QStringLiteral("nodeTypeId"), QStringLiteral("plugin.test")},
        {QStringLiteral("sdkVersion"), 99},
        {QStringLiteral("abiTag"), QStringLiteral("qt6-opencv")}});

    const QStringList diagnostics = Selt::PluginHost::instance().diagnoseDependencies(dependencies);
    QVERIFY(!diagnostics.isEmpty());
    QVERIFY(diagnostics.first().contains(QStringLiteral("缺少插件")));
}

void TestPluginDiagnostics::deploymentRequirementsAlwaysAvailable()
{
    const QStringList requirements = Selt::PluginHost::instance().deploymentRequirements();
    QVERIFY(!requirements.isEmpty());
}

QTEST_MAIN(TestPluginDiagnostics)
#include "test_plugin_diagnostics.moc"
