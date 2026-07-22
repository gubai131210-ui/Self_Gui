#ifndef NODEEXECUTORREGISTRY_H
#define NODEEXECUTORREGISTRY_H

#include "vision/runtime/inodeexecutor.h"

#include <QHash>
#include <QStringList>
#include <functional>

namespace Selt {

class NodeExecutorRegistry
{
public:
    using Factory = std::function<NodeExecutorPtr()>;

    static NodeExecutorRegistry &instance();

    void registerFactory(const QString &typeId, Factory factory);
    bool contains(const QString &typeId) const;
    NodeExecutorPtr create(const QString &typeId) const;
    QStringList registeredTypes() const;

private:
    NodeExecutorRegistry() = default;
    QHash<QString, Factory> m_factories;
};

} // namespace Selt

#endif // NODEEXECUTORREGISTRY_H
