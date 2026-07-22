#include "vision/domain/variablescope.h"

namespace Selt {
namespace {

const ProjectVariableStore *pickStore(const ProjectVariableStore *a,
                                      const ProjectVariableStore *b,
                                      const ProjectVariableStore *c,
                                      const ProjectVariableStore *d,
                                      const ProjectVariableStore *e,
                                      int index)
{
    switch (index) {
    case 0:
        return a;
    case 1:
        return b;
    case 2:
        return c;
    case 3:
        return d;
    case 4:
        return e;
    default:
        return nullptr;
    }
}

VariableScopeKind scopeAt(int index)
{
    switch (index) {
    case 0:
        return VariableScopeKind::Node;
    case 1:
        return VariableScopeKind::Group;
    case 2:
        return VariableScopeKind::Flow;
    case 3:
        return VariableScopeKind::Project;
    case 4:
        return VariableScopeKind::Global;
    default:
        return VariableScopeKind::Project;
    }
}

} // namespace

QString variableScopeKindToString(VariableScopeKind kind)
{
    switch (kind) {
    case VariableScopeKind::Global:
        return QStringLiteral("global");
    case VariableScopeKind::Flow:
        return QStringLiteral("flow");
    case VariableScopeKind::Group:
        return QStringLiteral("group");
    case VariableScopeKind::Node:
        return QStringLiteral("node");
    case VariableScopeKind::Project:
    default:
        return QStringLiteral("project");
    }
}

VariableScopeKind variableScopeKindFromString(const QString &text, bool *ok)
{
    const QString t = text.trimmed().toLower();
    auto setOk = [ok](bool v) {
        if (ok)
            *ok = v;
    };
    if (t == QLatin1String("global")) {
        setOk(true);
        return VariableScopeKind::Global;
    }
    if (t == QLatin1String("flow")) {
        setOk(true);
        return VariableScopeKind::Flow;
    }
    if (t == QLatin1String("group")) {
        setOk(true);
        return VariableScopeKind::Group;
    }
    if (t == QLatin1String("node")) {
        setOk(true);
        return VariableScopeKind::Node;
    }
    if (t == QLatin1String("project") || t.isEmpty()) {
        setOk(true);
        return VariableScopeKind::Project;
    }
    setOk(false);
    return VariableScopeKind::Project;
}

GlobalVariableStore &GlobalVariableStore::instance()
{
    static GlobalVariableStore store;
    return store;
}

bool VariableScopeChain::containsId(const QString &variableId) const
{
    if (!resolveById(variableId).record.id.isEmpty())
        return true;
    return containsRuntimeKey(variableId);
}

bool VariableScopeChain::containsRuntimeKey(const QString &key) const
{
    return m_runtimeValues && m_runtimeValues->contains(key);
}

DataValue VariableScopeChain::runtimeValue(const QString &key) const
{
    if (!m_runtimeValues)
        return {};
    return m_runtimeValues->value(key);
}

ProjectVariableRecord VariableScopeChain::recordById(const QString &variableId) const
{
    return resolveById(variableId).record;
}

ScopedVariableHit VariableScopeChain::resolveById(const QString &variableId) const
{
    ScopedVariableHit hit;
    if (variableId.isEmpty())
        return hit;
    for (int i = 0; i < 5; ++i) {
        const ProjectVariableStore *store =
            pickStore(m_node, m_group, m_flow, m_project, m_global, i);
        if (!store || !store->contains(variableId))
            continue;
        hit.record = store->record(variableId);
        hit.scope = scopeAt(i);
        if (hit.scope == VariableScopeKind::Flow)
            hit.scopeOwnerId = m_flowId;
        else if (hit.scope == VariableScopeKind::Group)
            hit.scopeOwnerId = m_groupId;
        else if (hit.scope == VariableScopeKind::Node)
            hit.scopeOwnerId = m_nodeId;
        return hit;
    }
    return hit;
}

ScopedVariableHit VariableScopeChain::resolveByName(const QString &name) const
{
    ScopedVariableHit hit;
    const QString key = name.trimmed();
    if (key.isEmpty())
        return hit;
    QStringList seenIds;
    for (int i = 0; i < 5; ++i) {
        const ProjectVariableStore *store =
            pickStore(m_node, m_group, m_flow, m_project, m_global, i);
        if (!store)
            continue;
        for (const ProjectVariableRecord &rec : store->records()) {
            if (rec.name.compare(key, Qt::CaseInsensitive) != 0)
                continue;
            hit.record = rec;
            hit.scope = scopeAt(i);
            if (hit.scope == VariableScopeKind::Flow)
                hit.scopeOwnerId = m_flowId;
            else if (hit.scope == VariableScopeKind::Group)
                hit.scopeOwnerId = m_groupId;
            else if (hit.scope == VariableScopeKind::Node)
                hit.scopeOwnerId = m_nodeId;
            hit.shadowed = !seenIds.isEmpty();
            return hit;
        }
    }
    return hit;
}

QVector<ScopedVariableHit> VariableScopeChain::allVisible() const
{
    QVector<ScopedVariableHit> out;
    QHash<QString, int> nameToIndex;
    // Far → near so near can overwrite map entries for display of effective value,
    // but we keep all records and mark shadowed names.
    for (int i = 4; i >= 0; --i) {
        const ProjectVariableStore *store =
            pickStore(m_node, m_group, m_flow, m_project, m_global, i);
        if (!store)
            continue;
        for (const ProjectVariableRecord &rec : store->records()) {
            ScopedVariableHit hit;
            hit.record = rec;
            hit.scope = scopeAt(i);
            if (hit.scope == VariableScopeKind::Flow)
                hit.scopeOwnerId = m_flowId;
            else if (hit.scope == VariableScopeKind::Group)
                hit.scopeOwnerId = m_groupId;
            else if (hit.scope == VariableScopeKind::Node)
                hit.scopeOwnerId = m_nodeId;
            const auto it = nameToIndex.constFind(rec.name.toLower());
            if (it != nameToIndex.cend())
                out[*it].shadowed = true;
            nameToIndex.insert(rec.name.toLower(), out.size());
            out.append(hit);
        }
    }
    return out;
}

} // namespace Selt
