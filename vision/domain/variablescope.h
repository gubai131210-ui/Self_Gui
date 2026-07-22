#pragma once

#include "vision/domain/projectvariables.h"
#include "vision/data/datatype.h"

#include <QHash>
#include <QString>
#include <QVector>

namespace Selt {

/// 配置态变量作用域（与 ExecutionScope 执行范围独立命名）。
enum class VariableScopeKind {
    Global = 0,
    Project = 1,
    Flow = 2,
    Group = 3,
    Node = 4
};

QString variableScopeKindToString(VariableScopeKind kind);
VariableScopeKind variableScopeKindFromString(const QString &text, bool *ok = nullptr);

struct ScopedVariableHit
{
    ProjectVariableRecord record;
    VariableScopeKind scope{VariableScopeKind::Project};
    QString scopeOwnerId; // flowId / groupId / nodeId
    bool shadowed{false};
};

/// 近处覆盖远处：Node → Group → Flow → Project → Global。
class VariableScopeChain
{
public:
    void setGlobal(const ProjectVariableStore *store) { m_global = store; }
    void setProject(const ProjectVariableStore *store) { m_project = store; }
    void setFlow(const ProjectVariableStore *store, const QString &flowId = {})
    {
        m_flow = store;
        m_flowId = flowId;
    }
    void setGroup(const ProjectVariableStore *store, const QString &groupId = {})
    {
        m_group = store;
        m_groupId = groupId;
    }
    void setNode(const ProjectVariableStore *store, const QString &nodeId = {})
    {
        m_node = store;
        m_nodeId = nodeId;
    }
    /// 运行时写回层（键→值），优先于配置态变量；不落盘。
    void setRuntimeValues(const QHash<QString, DataValue> *values) { m_runtimeValues = values; }

    bool containsId(const QString &variableId) const;
    bool containsRuntimeKey(const QString &key) const;
    DataValue runtimeValue(const QString &key) const;
    ProjectVariableRecord recordById(const QString &variableId) const;
    ScopedVariableHit resolveById(const QString &variableId) const;
    /// 按名称查找（近处优先），用于继承预览。
    ScopedVariableHit resolveByName(const QString &name) const;
    QVector<ScopedVariableHit> allVisible() const;

private:
    const ProjectVariableStore *m_global{nullptr};
    const ProjectVariableStore *m_project{nullptr};
    const ProjectVariableStore *m_flow{nullptr};
    const ProjectVariableStore *m_group{nullptr};
    const ProjectVariableStore *m_node{nullptr};
    const QHash<QString, DataValue> *m_runtimeValues{nullptr};
    QString m_flowId;
    QString m_groupId;
    QString m_nodeId;
};

/// 应用级全局变量（跨工程会话可选用；默认进程内单例）。
class GlobalVariableStore
{
public:
    static GlobalVariableStore &instance();
    ProjectVariableStore &store() { return m_store; }
    const ProjectVariableStore &store() const { return m_store; }

private:
    GlobalVariableStore() = default;
    ProjectVariableStore m_store;
};

/// 单次运行的只读作用域快照 + 运行时写回层（不落盘）。
class VariableScopeSnapshot
{
public:
    ProjectVariableStore global;
    ProjectVariableStore project;
    ProjectVariableStore flow;
    ProjectVariableStore group;
    ProjectVariableStore node;
    /// 运行期写回（变量键 → 值），供后续节点读取；不写入工程文件。
    QHash<QString, DataValue> runtimeValues;
    QString flowId;
    QString groupId;
    QString nodeId;

    void runtimeWrite(const QString &key, const DataValue &value)
    {
        if (!key.trimmed().isEmpty())
            runtimeValues.insert(key, value);
    }

    DataValue runtimeRead(const QString &key) const
    {
        return runtimeValues.value(key);
    }

    VariableScopeChain chain() const
    {
        VariableScopeChain c;
        c.setGlobal(&global);
        c.setProject(&project);
        c.setFlow(&flow, flowId);
        c.setGroup(&group, groupId);
        c.setNode(&node, nodeId);
        c.setRuntimeValues(&runtimeValues);
        return c;
    }

    static VariableScopeSnapshot fromStores(const ProjectVariableStore *projectStore,
                                            const ProjectVariableStore *flowStore = nullptr,
                                            const QString &flowId = {},
                                            const ProjectVariableStore *globalStore = nullptr)
    {
        VariableScopeSnapshot snap;
        if (globalStore)
            snap.global = *globalStore;
        else
            snap.global = GlobalVariableStore::instance().store();
        if (projectStore)
            snap.project = *projectStore;
        if (flowStore)
            snap.flow = *flowStore;
        snap.flowId = flowId;
        return snap;
    }
};

} // namespace Selt
