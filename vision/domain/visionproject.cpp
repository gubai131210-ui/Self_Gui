#include "vision/domain/visionproject.h"

#include "core/model/document.h"
#include "foundation/seltid.h"

VisionProject::VisionProject(QObject *parent)
    : QObject(parent)
    , m_projectId(Selt::IdGenerator::create(QStringLiteral("project")))
{
    ensureMainFlow();
}

void VisionProject::setProjectId(const QString &projectId)
{
    if (!projectId.isEmpty())
        m_projectId = projectId;
}

void VisionProject::setTitle(const QString &title)
{
    if (m_title == title)
        return;
    m_title = title;
    emit titleChanged(m_title);
    setModified(true);
}

void VisionProject::setVariables(const Selt::ProjectVariableStore &vars)
{
    m_variables = vars;
    emit variablesChanged();
    setModified(true);
}

void VisionProject::setResources(const Selt::ProjectResourceStore &resources)
{
    m_resources = resources;
    setModified(true);
}

VisionFlow *VisionProject::activeFlow() const
{
    return m_flows.value(m_activeFlowId, nullptr);
}

bool VisionProject::setActiveFlow(const QString &flowId)
{
    if (!m_flows.contains(flowId))
        return false;
    if (m_activeFlowId == flowId)
        return true;
    m_activeFlowId = flowId;
    emit activeFlowChanged(m_activeFlowId);
    return true;
}

VisionFlow *VisionProject::flow(const QString &flowId) const
{
    return m_flows.value(flowId, nullptr);
}

QVector<VisionFlow *> VisionProject::flows() const
{
    QVector<VisionFlow *> list;
    list.reserve(m_flowOrder.size());
    for (const QString &id : m_flowOrder) {
        if (VisionFlow *f = m_flows.value(id, nullptr))
            list.append(f);
    }
    return list;
}

QStringList VisionProject::flowNames() const
{
    QStringList names;
    for (const QString &id : m_flowOrder) {
        if (VisionFlow *f = m_flows.value(id, nullptr))
            names.append(f->name());
    }
    return names;
}

VisionFlow *VisionProject::createFlow(const QString &name)
{
    auto *flowObj = new VisionFlow(QString(), name, this);
    const bool isFirst = m_flows.isEmpty();
    if (isFirst)
        flowObj->setMainFlow(true);

    m_flows.insert(flowObj->flowId(), flowObj);
    m_flowOrder.append(flowObj->flowId());
    connectFlowSignals(flowObj);

    if (m_activeFlowId.isEmpty())
        m_activeFlowId = flowObj->flowId();

    emit flowAdded(flowObj->flowId());
    emit flowListChanged();
    setModified(true);
    return flowObj;
}

VisionFlow *VisionProject::createFlowWithId(const QString &flowId, const QString &name)
{
    if (flowId.isEmpty() || m_flows.contains(flowId))
        return nullptr;
    auto *flowObj = new VisionFlow(flowId, name, this);
    const bool isFirst = m_flows.isEmpty();
    if (isFirst)
        flowObj->setMainFlow(true);
    m_flows.insert(flowId, flowObj);
    m_flowOrder.append(flowId);
    connectFlowSignals(flowObj);
    if (m_activeFlowId.isEmpty())
        m_activeFlowId = flowId;
    emit flowAdded(flowId);
    emit flowListChanged();
    setModified(true);
    return flowObj;
}

bool VisionProject::removeFlow(const QString &flowId)
{
    VisionFlow *f = m_flows.value(flowId, nullptr);
    if (!f || m_flows.size() <= 1)
        return false;

    const bool wasActive = (m_activeFlowId == flowId);
    const QString nextId = adjacentFlowId(flowId);
    const bool wasMain = f->isMainFlow();

    m_flows.remove(flowId);
    m_flowOrder.removeAll(flowId);
    f->deleteLater();

    if (wasMain && !m_flowOrder.isEmpty()) {
        if (VisionFlow *newMain = m_flows.value(m_flowOrder.first(), nullptr))
            newMain->setMainFlow(true);
    }

    if (wasActive)
        setActiveFlow(nextId);

    emit flowRemoved(flowId);
    emit flowListChanged();
    setModified(true);
    return true;
}

bool VisionProject::renameFlow(const QString &flowId, const QString &name)
{
    VisionFlow *f = m_flows.value(flowId, nullptr);
    if (!f || name.trimmed().isEmpty())
        return false;
    if (f->name() == name)
        return true;
    f->setName(name.trimmed());
    emit flowRenamed(flowId, f->name());
    emit flowListChanged();
    setModified(true);
    return true;
}

void VisionProject::importLegacyDocument(const Document &document)
{
    clearAllFlows();
    VisionFlow *main = createFlow(QStringLiteral("主流程"));
    main->setMainFlow(true);
    m_activeFlowId = main->flowId();
    setTitle(document.title().isEmpty() ? QStringLiteral("未命名项目") : document.title());

    QString error;
    if (!main->replaceGraph(document.nodes(), document.connections(), &error)) {
        main->clear();
    }
    setActiveFlow(main->flowId());
    emit activeFlowChanged(m_activeFlowId);
    setModified(false);
}

void VisionProject::exportActiveFlowToDocument(Document &document) const
{
    VisionFlow *active = activeFlow();
    if (!active)
        return;
    document.replaceAll(m_title, document.settings(), active->nodes(), active->connections());
}

void VisionProject::clearAllFlows()
{
    const QStringList ids = m_flowOrder;
    m_flowOrder.clear();
    m_activeFlowId.clear();
    for (const QString &id : ids) {
        VisionFlow *f = m_flows.take(id);
        if (f)
            f->deleteLater();
    }
    m_flows.clear();
    m_variables.clear();
    m_resources.clear();
    emit flowListChanged();
    emit variablesChanged();
}

void VisionProject::markClean()
{
    setModified(false);
}

void VisionProject::setModified(bool modified)
{
    if (m_modified == modified)
        return;
    m_modified = modified;
    emit modifiedChanged(m_modified);
}

void VisionProject::ensureMainFlow()
{
    if (!m_flows.isEmpty())
        return;
    VisionFlow *flowObj = createFlow(QStringLiteral("主流程"));
    flowObj->setMainFlow(true);
    m_activeFlowId = flowObj->flowId();
    setModified(false);
}

void VisionProject::connectFlowSignals(VisionFlow *flowObj)
{
    if (!flowObj)
        return;
    connect(flowObj, &VisionFlow::graphChanged, this, [this]() { setModified(true); });
    connect(flowObj, &VisionFlow::nameChanged, this, [this](const QString &) {
        emit flowListChanged();
        setModified(true);
    });
}

QString VisionProject::adjacentFlowId(const QString &removedId) const
{
    const int idx = m_flowOrder.indexOf(removedId);
    if (idx < 0)
        return m_flowOrder.isEmpty() ? QString() : m_flowOrder.first();
    if (idx + 1 < m_flowOrder.size())
        return m_flowOrder.at(idx + 1);
    if (idx - 1 >= 0)
        return m_flowOrder.at(idx - 1);
    return QString();
}
