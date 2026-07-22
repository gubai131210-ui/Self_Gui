#ifndef INVERTNODEPLUGIN_H
#define INVERTNODEPLUGIN_H

#include "sdk/include/selt/iplugin.h"

#include <QObject>

class InvertNodePlugin : public QObject, public Selt::INodePlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID SELT_NODE_PLUGIN_IID)
    Q_INTERFACES(Selt::INodePlugin)
public:
    Selt::PluginMetadata metadata() const override;
    QVector<ModuleDescriptor> descriptors() const override;
    void registerExecutors(Selt::NodeExecutorRegistry &registry) override;
};

#endif
