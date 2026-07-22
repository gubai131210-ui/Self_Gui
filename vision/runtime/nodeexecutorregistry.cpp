#include "vision/runtime/nodeexecutorregistry.h"

namespace Selt {

NodeExecutorRegistry &NodeExecutorRegistry::instance()
{
    static NodeExecutorRegistry reg;
    return reg;
}

void NodeExecutorRegistry::registerFactory(const QString &typeId, Factory factory)
{
    m_factories.insert(typeId, std::move(factory));
}

bool NodeExecutorRegistry::contains(const QString &typeId) const
{
    return m_factories.contains(typeId);
}

NodeExecutorPtr NodeExecutorRegistry::create(const QString &typeId) const
{
    const Factory factory = m_factories.value(typeId);
    return factory ? factory() : NodeExecutorPtr{};
}

QStringList NodeExecutorRegistry::registeredTypes() const
{
    return m_factories.keys();
}

} // namespace Selt
